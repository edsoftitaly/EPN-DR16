// EPN DR-16 :: program algorithms (internal rate)
#pragma once
#include "Primitives.h"
#include "Programs.h"
#include <array>

namespace epn
{
// ===========================================================================
// FDN reverb voicings. Topology reconstructed from the manual's descriptions:
// lines/diffusion/early reflections tuned per program character.
// ===========================================================================
struct FdnVoicing
{
    int nLines;            // 4 or 8
    float lineMs[8];
    int nDiff; float diffMs[6]; float diffG;
    float nestMs[8]; float nestG;          // allpass embedded in each loop (0 = none)
    float modMs;                           // loop delay modulation depth
    int nEr; float erMs[12]; float erG[12]; float erLevel;
    float lateLevel;
    float topLpHz;                         // output "top end restraint" (0 = none)
    bool sizeFromDecay;                    // ROOM B1: constant diffusion, decay scales room size
};

inline const FdnVoicing& fdnVoicing (int v)
{
    static const FdnVoicing t[9] = {
        // 0 AMBIENCE: between hall and plate, dimensional elements minimised (no ER), low colouration
        { 8, { 37.1f, 41.3f, 47.9f, 53.3f, 59.7f, 67.1f, 73.9f, 79.3f },
          4, { 4.7f, 3.6f, 12.7f, 9.3f }, 0.70f,
          { 6.1f, 7.3f, 8.9f, 5.3f, 9.7f, 4.3f, 7.9f, 6.7f }, 0.40f,
          0.12f, 0, {}, {}, 0.0f, 0.55f, 0.0f, false },
        // 1 ROOM A1: live room, deliberately high colouration (few lines, little diffusion), top end restrained
        { 4, { 19.7f, 23.3f, 29.1f, 31.7f },
          2, { 3.1f, 5.3f }, 0.50f,
          {}, 0.0f,
          0.0f, 8, { 4.9f, 7.7f, 11.3f, 13.9f, 17.3f, 21.1f, 26.3f, 29.9f },
                   { 0.85f, -0.7f, 0.62f, -0.55f, 0.48f, -0.42f, 0.36f, -0.3f }, 0.75f, 0.70f, 9500.0f, false },
        // 2 HALL C1: strong initial reflections defining hall size, high presence
        { 8, { 61.0f, 67.7f, 73.1f, 83.9f, 89.3f, 97.7f, 103.1f, 113.9f },
          4, { 5.3f, 4.1f, 13.7f, 10.1f }, 0.70f,
          {}, 0.0f,
          0.25f, 12, { 9.7f, 14.3f, 19.1f, 23.9f, 29.3f, 34.1f, 41.9f, 47.3f, 53.9f, 61.1f, 70.3f, 79.7f },
                   { 0.95f, -0.88f, 0.80f, -0.74f, 0.68f, -0.62f, 0.55f, -0.50f, 0.44f, -0.39f, 0.33f, -0.28f }, 1.0f, 0.55f, 0.0f, false },
        // 3 PLATE A1: high diffusion, medium colouration, even decay
        { 8, { 29.3f, 33.1f, 37.7f, 41.9f, 44.3f, 49.1f, 53.7f, 58.9f },
          6, { 1.3f, 2.1f, 3.7f, 5.9f, 8.3f, 11.1f }, 0.75f,
          { 3.1f, 4.7f, 5.3f, 6.1f, 3.9f, 7.1f, 4.3f, 5.7f }, 0.60f,
          0.18f, 0, {}, {}, 0.0f, 0.60f, 0.0f, false },
        // 4 HALL B3: as C1 with lower-energy initial reflections
        { 8, { 59.3f, 66.1f, 75.7f, 81.1f, 91.9f, 96.7f, 107.3f, 116.1f },
          4, { 5.9f, 4.3f, 14.1f, 10.7f }, 0.72f,
          {}, 0.0f,
          0.25f, 12, { 11.3f, 15.9f, 21.7f, 25.1f, 31.9f, 36.7f, 43.1f, 49.9f, 57.1f, 63.7f, 72.9f, 83.1f },
                   { 0.95f, -0.88f, 0.80f, -0.74f, 0.68f, -0.62f, 0.55f, -0.50f, 0.44f, -0.39f, 0.33f, -0.28f }, 0.40f, 0.58f, 0.0f, false },
        // 5 ROOM A0: original ROOM A1 without the top end restraint
        { 4, { 19.7f, 23.3f, 29.1f, 31.7f },
          2, { 3.1f, 5.3f }, 0.50f,
          {}, 0.0f,
          0.0f, 8, { 4.9f, 7.7f, 11.3f, 13.9f, 17.3f, 21.1f, 26.3f, 29.9f },
                   { 0.85f, -0.7f, 0.62f, -0.55f, 0.48f, -0.42f, 0.36f, -0.3f }, 0.75f, 0.70f, 0.0f, false },
        // 6 ROOM B1: constant diffusion density with time, decay control alters room size
        { 8, { 11.3f, 13.7f, 17.9f, 19.1f, 23.3f, 27.7f, 29.9f, 31.3f },
          4, { 1.7f, 2.9f, 4.1f, 6.3f }, 0.75f,
          { 1.9f, 2.3f, 3.1f, 2.7f, 3.7f, 1.3f, 2.1f, 3.3f }, 0.55f,
          0.0f, 0, {}, {}, 0.0f, 0.60f, 0.0f, true },
        // 7 HALL A1: earlier hall, less smooth decay (less diffusion, no modulation)
        { 8, { 61.0f, 67.7f, 73.1f, 83.9f, 89.3f, 97.7f, 103.1f, 113.9f },
          2, { 5.3f, 13.7f }, 0.60f,
          {}, 0.0f,
          0.0f, 12, { 9.7f, 14.3f, 19.1f, 23.9f, 29.3f, 34.1f, 41.9f, 47.3f, 53.9f, 61.1f, 70.3f, 79.7f },
                   { 0.95f, -0.88f, 0.80f, -0.74f, 0.68f, -0.62f, 0.55f, -0.50f, 0.44f, -0.39f, 0.33f, -0.28f }, 0.85f, 0.55f, 0.0f, false },
        // 8 PLATE B1: alternative plate
        { 8, { 27.1f, 31.9f, 35.3f, 40.7f, 46.1f, 50.3f, 55.9f, 61.3f },
          6, { 1.1f, 1.9f, 3.3f, 5.1f, 7.9f, 12.3f }, 0.72f,
          { 2.7f, 5.1f, 4.1f, 6.7f, 3.5f, 6.3f, 4.9f, 5.5f }, 0.50f,
          0.14f, 0, {}, {}, 0.0f, 0.60f, 0.0f, false },
    };
    return t[std::clamp (v, 0, 8)];
}

class FdnReverb
{
public:
    void prepare()
    {
        for (int i = 0; i < 8; ++i) { lines[i].init (msToSamp (130.0) + 64); nests[i].init (msToSamp (12.0), 0.5f); }
        for (auto& a : diff) a.init (msToSamp (15.0), 0.7f);
        er.init (msToSamp (90.0));
    }

    void setVoicing (int v)
    {
        voice = &fdnVoicing (v);
        for (int i = 0; i < voice->nDiff; ++i)
        {
            diff[(size_t) i].D = std::max (1, msToSamp (voice->diffMs[i]));
            diff[(size_t) i].g = voice->diffG;
        }
        for (int i = 0; i < 8; ++i)
        {
            nests[(size_t) i].D = std::max (1, msToSamp (voice->nestMs[i] > 0 ? voice->nestMs[i] : 1.0f));
            nests[(size_t) i].g = voice->nestG;
            lfoPhase[i] = (float) i * 0.785f;
            lfoInc[i] = (float) (2.0 * kPi * (0.31 + 0.07 * i) / kIntFs);
        }
        topLp.setCutoff (voice->topLpHz, kIntFs); topLp2.setCutoff (voice->topLpHz, kIntFs);
        sizeScale = 1.0f;
        computeLengths();
    }

    void clear()
    {
        for (auto& l : lines) l.clear();
        for (auto& n : nests) n.clear();
        for (auto& a : diff) a.clear();
        for (auto& f : filt) f.clear();
        er.clear(); topLp.clear(); topLp2.clear();
    }

    // rt: indicated decay (s). unity: lossless (AMBIENCE 9.9 flat / FREEZE)
    void setDecay (double rt, int lo, int hi, bool loAvail, bool hiAvail, bool unity)
    {
        lossless = unity;   // integer reads: interpolation would act as a per-pass low-pass
        if (voice->sizeFromDecay)
        {
            float s = (float) std::clamp (rt / 0.7, 0.25, 1.0);
            if (std::abs (s - sizeScale) > 1.0e-4f) { sizeScale = s; computeLengths(); }
        }
        for (int i = 0; i < voice->nLines; ++i)
        {
            double loop = len[i] + (voice->nestG > 0.0f ? nests[(size_t) i].D : 0);
            filt[i].set (loop, rt, lo, hi, loAvail, hiAvail, unity);
        }
    }

    inline void process (float x, float& outL, float& outR, Word16& word) noexcept
    {
        const auto& v = *voice;
        const int n = v.nLines;

        // early reflections from the predelayed input
        float eL = 0.0f, eR = 0.0f;
        if (v.nEr > 0)
        {
            for (int k = 0; k < v.nEr; ++k)
            {
                float s = er.read (erD[k]) * v.erG[k];
                if (k & 1) eR += s; else eL += s * 1.0f;
                if (k & 2) eL += 0.35f * s; else eR += 0.35f * s;
            }
            er.push (x);
        }

        float d = x;
        for (int i = 0; i < v.nDiff; ++i) d = diff[(size_t) i].process (d);

        float r[8], m[8];
        for (int i = 0; i < n; ++i)
        {
            float D = (float) len[i];
            if (v.modMs > 0.0f && ! lossless)
            {
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] > 6.2831853f) lfoPhase[i] -= 6.2831853f;
                D += modDepth * (1.0f + std::sin (lfoPhase[i]));
            }
            r[i] = lossless ? lines[i].read (len[i]) : lines[i].readFrac (D);
            float y = filt[i].process (r[i]);
            if (v.nestG > 0.0f) y = nests[(size_t) i].process (y);
            m[i] = y;
        }
        hadamard (m, n);
        const float inj = 0.5f;
        for (int i = 0; i < n; ++i)
            lines[i].push (word (m[i] + ((i & 1) ? -inj : inj) * d));

        float lL = 0.0f, lR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            lL += (outSignL[i] ? r[i] : -r[i]);
            lR += (outSignR[i] ? r[i] : -r[i]);
        }
        const float lg = v.lateLevel / std::sqrt ((float) n);
        outL = lL * lg + eL * v.erLevel * 0.35f;
        outR = lR * lg + eR * v.erLevel * 0.35f;
        if (v.topLpHz > 0.0f) { outL = topLp.process (outL); outR = topLp2.process (outR); }
    }

private:
    static inline void hadamard (float* x, int n) noexcept
    {
        for (int h = 1; h < n; h <<= 1)
            for (int i = 0; i < n; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    float a = x[j], b = x[j + h];
                    x[j] = a + b; x[j + h] = a - b;
                }
        const float s = 1.0f / std::sqrt ((float) n);
        for (int i = 0; i < n; ++i) x[i] *= s;
    }

    void computeLengths()
    {
        for (int i = 0; i < voice->nLines; ++i)
            len[i] = std::max (8, msToSamp (voice->lineMs[i] * sizeScale));
        for (int k = 0; k < voice->nEr; ++k) erD[k] = std::max (1, msToSamp (voice->erMs[k]));
        modDepth = (float) (voice->modMs * 0.001 * kIntFs * 0.5);
    }

    const FdnVoicing* voice = &fdnVoicing (0);
    DelayLine lines[8];
    std::array<Allpass, 8> nests;
    std::array<Allpass, 6> diff;
    DecayFilter filt[8];
    DelayLine er;
    OnePoleLP topLp, topLp2;
    int len[8] {}, erD[12] {};
    float lfoPhase[8] {}, lfoInc[8] {};
    float modDepth = 0.0f, sizeScale = 1.0f;
    bool lossless = false;
    static constexpr bool outSignL[8] = { true, false, true, true, false, true, false, false };
    static constexpr bool outSignR[8] = { true, true, false, true, false, false, true, false };
};

// ===========================================================================
// Tapped programs: NONLIN 1/2, REVERSE 1/2, IMAGE P1
// Dense (diffused) or discrete reflection patterns under a programmed
// amplitude contour. Output 1 = diffused, output 2 = discrete where the manual
// says so.
// ===========================================================================
class TapReverb
{
public:
    void prepare()
    {
        diffLine.init (msToSamp (1300.0));
        rawLine.init (msToSamp (1300.0));
        const float dm[4] = { 1.7f, 3.1f, 4.9f, 7.3f };
        for (int i = 0; i < 4; ++i) ap[(size_t) i].init (msToSamp (dm[i]), 0.6f);
        tapsL.reserve (1200); tapsR.reserve (1200);
    }
    void clear()
    {
        diffLine.clear(); rawLine.clear();
        for (auto& a : ap) a.clear();
        lpL.clear(); lpR.clear();
    }

    void configure (TapMode m, float decay, int hi, bool hiAvail)
    {
        lpL.setCutoff (hiAvail ? hfHalfDecayHz (hi) : 0.0, kIntFs);
        lpR.a = lpL.a;
        if (m == mode && std::abs (decay - lastDecay) < 1.0e-4f) return;
        mode = m; lastDecay = decay;
        build();
    }

    inline void process (float x, float& outL, float& outR) noexcept
    {
        float d = x;
        for (auto& a : ap) d = a.process (d);
        diffLine.push (d);
        rawLine.push (x);
        float l = 0.0f, r = 0.0f;
        for (const auto& t : tapsL) l += t.g * (t.diffuse ? diffLine.read (t.d) : rawLine.read (t.d));
        for (const auto& t : tapsR) r += t.g * (t.diffuse ? diffLine.read (t.d) : rawLine.read (t.d));
        outL = lpL.a > 0.0f ? lpL.process (l) : l;
        outR = lpR.a > 0.0f ? lpR.process (r) : r;
    }

    // gate length T (s) for a decay control value. Manual: values "not calibrated".
    static double gateSeconds (TapMode m, float decay)
    {
        double dv = std::clamp ((double) decay, 0.1, 9.9);
        if (m == TapMode::Image) return 0.006 + (dv / 9.9) * 0.594;   // "few ms .. about 0.6 s"
        return 0.1 * dv;                                             // [est]
    }

private:
    struct Tap { int d; float g; bool diffuse; };

    static double envelope (TapMode m, double t, double T)
    {
        switch (m)
        {
            case TapMode::Nonlin2:
            case TapMode::Image:   // decays only very slightly, then dies away very rapidly
            {
                const double k = 0.86 * T;
                if (t < k) return std::pow (10.0, (-6.0 * t / k) / 20.0);
                return std::pow (10.0, (-6.0 - 54.0 * (t - k) / (T - k)) / 20.0);
            }
            case TapMode::Nonlin1: // does not decay at all, then very rapidly dies away
            {
                const double k = 0.90 * T;
                if (t < k) return 1.0;
                return std::pow (10.0, (-60.0 * (t - k) / (T - k)) / 20.0);
            }
            case TapMode::Reverse1:
            case TapMode::Reverse2: // builds up then suddenly stops
                return std::pow (10.0, (-30.0 * (1.0 - t / T)) / 20.0);
            default: return 0.0;
        }
    }

    void makeSet (std::vector<Tap>& out, bool diffuse, uint32_t seed, double T, int pan /*0 none,1 L,2 R*/)
    {
        Lcg rng (seed);
        const double spacingMs = diffuse ? std::max (T * 1000.0 / 240.0, 1.2) : std::max (T * 1000.0 / 22.0, 9.0);
        double tMs = diffuse ? 0.8 : 2.0 + rng.uni() * spacingMs * 0.5;
        std::vector<Tap> v;
        double energy = 0.0;
        while (tMs < T * 1000.0)
        {
            double e = envelope (mode, tMs * 0.001, T);
            float g = (float) e * (rng.uni() < 0.5f ? -1.0f : 1.0f) * (0.6f + 0.4f * rng.uni());
            if (pan)
            {
                double th = (tMs * 0.001 / T) * kPi * 0.5;          // image pans L -> R
                g *= (float) (pan == 1 ? std::cos (th) : std::sin (th));
            }
            v.push_back ({ std::max (1, msToSamp (tMs)), g, diffuse });
            energy += (double) g * g;
            tMs += spacingMs * (0.55 + 0.9 * rng.uni());
        }
        if (energy > 0.0)
        {
            float s = (float) std::sqrt ((pan ? 0.25 : 0.5) / energy) * (diffuse ? 1.0f : 1.25f);
            for (auto& t : v) t.g *= s;
        }
        out = std::move (v);
    }

    void build()
    {
        const double T = gateSeconds (mode, lastDecay);
        switch (mode)
        {
            case TapMode::Nonlin2:  makeSet (tapsL, true, 0x1111u, T, 0); makeSet (tapsR, true, 0x2222u, T, 0); break;
            case TapMode::Nonlin1:  makeSet (tapsL, true, 0x3333u, T, 0); makeSet (tapsR, false, 0x4444u, T, 0); break;
            case TapMode::Reverse1: makeSet (tapsL, true, 0x5555u, T, 0); makeSet (tapsR, false, 0x6666u, T, 0); break;
            case TapMode::Reverse2: makeSet (tapsL, true, 0x7777u, T, 0); makeSet (tapsR, false, 0x8888u, T, 0); break;
            case TapMode::Image:    makeSet (tapsL, true, 0x9999u, T, 1); makeSet (tapsR, true, 0x9999u, T, 2); break;
            default: tapsL.clear(); tapsR.clear(); break;
        }
    }

    TapMode mode = TapMode::None;
    float lastDecay = -1.0f;
    DelayLine diffLine, rawLine;
    std::array<Allpass, 4> ap;
    std::vector<Tap> tapsL, tapsR;
    OnePoleLP lpL, lpR;
};

// ===========================================================================
// CHORUS: five voices panned across the image, randomised delays.
// PRE = basic separation (0..50 ms per voice), DECAY = randomising depth.
// ===========================================================================
class Chorus
{
public:
    void prepare() { line.init (msToSamp (300.0)); reset(); }
    void clear() { line.clear(); lpL.clear(); lpR.clear(); }
    void reset()
    {
        Lcg r (0xC0DEu);
        for (int v = 0; v < 5; ++v) { target[v] = r.uni(); s1[v] = s2[v] = target[v]; count[v] = (int) (r.uni() * kIntFs); }
    }
    void set (float sepMs, float decay, int hi)
    {
        sep = sepMs; depthMs = decay * 0.35f;
        lpL.setCutoff (hfHalfDecayHz (hi), kIntFs); lpR.a = lpL.a;
    }
    inline void process (float x, float& outL, float& outR) noexcept
    {
        line.push (x);
        static const float panL[5] = { 1.0f, 0.924f, 0.707f, 0.383f, 0.0f };
        static const float panR[5] = { 0.0f, 0.383f, 0.707f, 0.924f, 1.0f };
        static const int order[5] = { 2, 0, 4, 1, 3 };   // spread voices in time across the image
        float l = 0.0f, r = 0.0f;
        const float c = 0.99985f;                          // ~1 Hz smoothing
        for (int v = 0; v < 5; ++v)
        {
            if (--count[v] <= 0) { target[v] = rng.uni(); count[v] = (int) ((0.35f + 0.9f * rng.uni()) * (float) kIntFs); }
            s1[v] = c * s1[v] + (1.0f - c) * target[v];
            s2[v] = c * s2[v] + (1.0f - c) * s1[v];
            float dMs = 0.6f + sep * (float) (order[v] + 1) + depthMs * s2[v];
            float D = std::max (1.0f, dMs * 0.001f * (float) kIntFs);
            float y = line.readFrac (D);
            l += panL[v] * y; r += panR[v] * y;
        }
        outL = lpL.process (l * 0.55f);
        outR = lpR.process (r * 0.55f);
    }
private:
    DelayLine line; OnePoleLP lpL, lpR; Lcg rng { 0xFACEu };
    float sep = 10.0f, depthMs = 1.0f;
    float target[5] {}, s1[5] {}, s2[5] {}; int count[5] {};
};

// ===========================================================================
// ECHO (two independent outputs A/B) and DELAY (output 2 only)
// ===========================================================================
struct EchoLine
{
    DelayLine d; int D = 1; float fb = 0.0f; float loK = 1.0f;
    float xc = 0.0f, lp = 0.0f; OnePoleLP hf; bool useHf = false;
    void prepare (int maxMs) { d.init (msToSamp (maxMs) + 8); xc = (float) std::exp (-2.0 * kPi * 350.0 / kIntFs); }
    void clear() { d.clear(); lp = 0.0f; hf.clear(); }
    // decay control 0..9.9 = regeneration. 0 = single repeat.
    void set (float ms, float decay, int lo, int hi, bool loAvail, bool hiAvail)
    {
        D = std::max (1, msToSamp (ms));
        fb = std::clamp (decay / 9.9f, 0.0f, 1.0f) * 0.985f;
        loK = loAvail ? (float) std::pow (10.0, lo / 20.0) : 1.0f;
        useHf = hiAvail && hi < 0;
        hf.setCutoff (useHf ? hfHalfDecayHz (hi) : 0.0, kIntFs);
    }
    inline float process (float x, Word16& w) noexcept
    {
        float y = d.read (D);
        float f = y;
        if (loK != 1.0f) { lp = (1.0f - xc) * f + xc * lp; f = (f - lp) + std::min (loK * fb, 0.995f) / std::max (fb, 1.0e-6f) * lp; }
        if (useHf) f = hf.process (f);
        d.push (w (x + fb * f));
        return y;
    }
};
} // namespace epn
