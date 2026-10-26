// EPN DR-16 :: the machine (internal rate) + converter chain (host rate)
#pragma once
#include "Engines.h"

namespace epn
{
struct MachineParams
{
    int program = 1;
    float preMs = 0.0f;
    float decay = 2.0f;
    int lo = 0, hi = 0;
    float echoBMs = 150.0f;   // ECHO output B
    float echoBDecay = 2.0f;
    bool vintage = true;      // 16-bit word + gain-ranged ADC + 16-bit DAC
};

// ---------------------------------------------------------------------------
// The DSP "card set": everything at kIntFs, mono in -> stereo out (output 1/2)
// ---------------------------------------------------------------------------
class Machine
{
public:
    void prepare()
    {
        pre.init (msToSamp (1250.0));
        fdn.prepare(); taps.prepare(); chorus.prepare();
        echoA.prepare (kEchoAMaxMs); echoB.prepare (kEchoBMaxMs);
        delay.prepare (820);
        applyProgram (true);
        fresh = true;   // first parameter load after prepare is not a "program change"
    }

    void setParams (const MachineParams& p)
    {
        const bool progChanged = p.program != cur.program;
        cur = p;
        word.enabled = p.vintage;
        if (progChanged) { applyProgram (fresh); fresh = false; return; }
        fresh = false;
        updateCoefficients();
    }

    bool takeOverflow() { bool o = word.overflow; word.overflow = false; return o; }

    inline void process (float x, float& outL, float& outR) noexcept
    {
        if (muteCount > 0)
        {
            --muteCount; outL = outR = 0.0f;
            if (muteCount == 0) clearAll();
            return;
        }
        x = word (x);
        const auto& s = program (cur.program);
        switch (s.algo)
        {
            case Algo::Fdn:
            case Algo::Freeze:
            {
                float in = x;
                if (s.algo == Algo::Freeze && freezeState != 2) in = 0.0f;   // 0.0 clear, 0.1 freeze/mute
                float y = preRead (in);
                fdn.process (y, outL, outR, word);
                break;
            }
            case Algo::Taps:   { float y = preRead (x); taps.process (y, outL, outR); break; }
            case Algo::Chorus: chorus.process (x, outL, outR); break;
            case Algo::Echo:   outL = echoA.process (x, word); outR = echoB.process (x, word); break;
            case Algo::Delay:  outL = x; outR = delay.process (x, word); break;   // out 1 straight through A/D/A
        }
        outL = word (outL); outR = word (outR);
    }

private:
    inline float preRead (float x) noexcept
    {
        pre.push (x);
        return preD > 0 ? pre.read (preD + 1) : x;
    }

    void clearAll()
    {
        pre.clear(); fdn.clear(); taps.clear(); chorus.clear();
        echoA.clear(); echoB.clear(); delay.clear();
    }

    void applyProgram (bool immediate)
    {
        const auto& s = program (cur.program);
        if (s.algo == Algo::Fdn || s.algo == Algo::Freeze) fdn.setVoicing (s.voicing);
        if (s.algo == Algo::Chorus) chorus.reset();
        updateCoefficients();
        // Manual: when changing programs the output is muted so 'memory flush-out' is not heard.
        if (immediate) clearAll(); else muteCount = (int) (0.25 * kIntFs);
    }

    void updateCoefficients()
    {
        const auto& s = program (cur.program);
        const float decay = std::clamp (cur.decay, s.decMin, s.decMax);
        const float preMs = std::clamp (cur.preMs, 0.0f, (float) s.preMaxMs);
        preD = msToSamp (preMs);

        switch (s.algo)
        {
            case Algo::Fdn:
            {
                // AMBIENCE: 9.9 = almost infinite; with both filters at 0 the decay is lossless
                // ("all original harmonic information without air absorption simulation").
                bool infinite = (cur.program == 1 && decay >= 9.85f);
                bool unity = infinite && cur.lo == 0 && cur.hi == 0;
                double rt = infinite ? 120.0 : (double) decay;
                fdn.setDecay (rt, cur.lo, cur.hi, s.lo, s.hi, unity);
                break;
            }
            case Algo::Freeze:
            {
                // 0.0 clear, 0.1 freeze/mute input, 0.2 and above freeze/add
                freezeState = decay < 0.05f ? 0 : (decay < 0.15f ? 1 : 2);
                if (freezeState == 0) fdn.setDecay (0.08, 0, 0, false, false, false);
                else fdn.setDecay (0.0, 0, 0, false, false, true);
                break;
            }
            case Algo::Taps:   taps.configure (s.tap, decay, cur.hi, s.hi); break;
            case Algo::Chorus: chorus.set (preMs, decay, cur.hi); break;
            case Algo::Echo:
                echoA.set (std::clamp (cur.preMs, 1.0f, (float) kEchoAMaxMs), decay, cur.lo, cur.hi, true, true);
                echoB.set (std::clamp (cur.echoBMs, 1.0f, (float) kEchoBMaxMs), std::clamp (cur.echoBDecay, 0.0f, 9.9f), cur.lo, cur.hi, true, true);
                break;
            case Algo::Delay:
                delay.set (std::max (1.0f, preMs), decay, 0, 0, false, false);
                break;
        }
    }

    MachineParams cur;
    Word16 word;
    DelayLine pre; int preD = 0;
    FdnReverb fdn; TapReverb taps; Chorus chorus;
    EchoLine echoA, echoB, delay;
    int muteCount = 0, freezeState = 2;
    bool fresh = true;
};

// ---------------------------------------------------------------------------
// Converter chain at host rate (modelled on the AMS input/ADC/DAC boards of
// the same generation, DMX 15-80S service schematics):
//   input board: balanced receiver + multi-stage active anti-alias low-pass
//   ADC: S/H + SAR with gain ranging (12-bit mantissa, 6 dB range steps)
//   DAC: 16-bit (PCM53 class) + deglitch hold + active reconstruction filter
// ---------------------------------------------------------------------------
class Dr16Core
{
public:
    static constexpr double kAaHz = 19600.0, kRcHz = 21000.0;
    static constexpr double kEqHz = 17000.0, kEqQ = 0.8, kEqDb = 3.6;
    void prepare (double hostFs)
    {
        fs = hostFs;
        // tuned so the full A/D/A chain lands on the AMS spec: 20 Hz - 18 kHz, -3 dB / +0 dB
        aa.design (8, kAaHz, fs);
        rc.design (6, kRcHz, fs);
        rcR.design (6, kRcHz, fs);
        eqL.peak (kEqHz, kEqQ, kEqDb, fs); eqR = eqL;
        stepIn = fs / kIntFs;     // host samples per internal sample
        stepOut = kIntFs / fs;    // internal samples per host sample
        machine.prepare();
        reset();
    }

    void reset()
    {
        aa.clear(); rc.clear(); rcR.clear(); eqL.clear(); eqR.clear();
        std::fill (std::begin (hin), std::end (hin), 0.0f);
        std::fill (std::begin (yl), std::end (yl), 0.0f);
        std::fill (std::begin (yr), std::end (yr), 0.0f);
        fracIn = 0.0; fracOut = 0.0;
        fifoL.assign (64, 0.0f); fifoR.assign (64, 0.0f);
        fr = 0; fw = 4; // prime 4 samples
    }

    void setParams (const MachineParams& p) { vintage = p.vintage; machine.setParams (p); }
    bool takeOverflow() { bool o = machine.takeOverflow() || adcClip; adcClip = false; return o; }

    // mono in -> wet stereo out, host rate
    void process (const float* in, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            // ---- input board + ADC side
            float x = aa.process (in[i]);
            hin[0] = hin[1]; hin[1] = hin[2]; hin[2] = hin[3]; hin[3] = x;
            while (fracIn < 1.0)
            {
                float s = hermite (hin[0], hin[1], hin[2], hin[3], (float) fracIn);
                s = adc (s);
                float l, r;
                machine.process (s, l, r);
                if (vintage) { l = dac16 (l); r = dac16 (r); }
                fifoL[(size_t) fw] = l; fifoR[(size_t) fw] = r; fw = (fw + 1) & 63;
                fracIn += stepIn;
            }
            fracIn -= 1.0;

            // ---- DAC side: pull internal samples as needed
            while (fracOut >= 1.0)
            {
                pull();
                fracOut -= 1.0;
            }
            outL[i] = (float) eqL.process (rc.process (hermite (yl[0], yl[1], yl[2], yl[3], (float) fracOut)));
            outR[i] = (float) eqR.process (rcR.process (hermite (yr[0], yr[1], yr[2], yr[3], (float) fracOut)));
            fracOut += stepOut;
        }
    }

private:
    inline void pull() noexcept
    {
        float l = 0.0f, r = 0.0f;
        if (fr != fw) { l = fifoL[(size_t) fr]; r = fifoR[(size_t) fr]; fr = (fr + 1) & 63; }
        else { l = yl[3]; r = yr[3]; }   // underrun safety
        yl[0] = yl[1]; yl[1] = yl[2]; yl[2] = yl[3]; yl[3] = l;
        yr[0] = yr[1]; yr[1] = yr[2]; yr[2] = yr[3]; yr[3] = r;
    }

    // gain-ranging ADC: sign + 11-bit magnitude mantissa, exponent 0..4 (x6 dB)
    inline float adc (float x) noexcept
    {
        if (x > 1.0f) { x = 1.0f; adcClip = true; }
        else if (x < -1.0f) { x = -1.0f; adcClip = true; }
        if (! vintage) return x;
        float a = std::abs (x);
        int e = 0;
        while (e < 4 && a * 2.0f < 1.0f) { a *= 2.0f; ++e; }
        const float scale = (float) (1 << e) * 2048.0f;
        return std::nearbyint (x * scale) / scale;
    }
    static inline float dac16 (float x) noexcept
    {
        x = std::clamp (x, -1.0f, 32767.0f / 32768.0f);
        return std::nearbyint (x * 32768.0f) / 32768.0f;
    }

    double fs = 48000.0, stepIn = 1.2, stepOut = 0.83, fracIn = 0.0, fracOut = 0.0;
    ButterLP aa, rc, rcR;
    Biquad eqL, eqR;
    float hin[4] {}, yl[4] {}, yr[4] {};
    std::vector<float> fifoL, fifoR; int fr = 0, fw = 4;
    Machine machine;
    bool vintage = true, adcClip = false;
};
} // namespace epn
