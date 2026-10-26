// EPN DR-16 :: DSP primitives
// Everything here runs at the internal machine rate (kIntFs) unless noted.
#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace epn
{
constexpr double kPi = 3.14159265358979323846;

// Internal sample rate of the emulated machine.
// The original RMX16 rate is not officially documented; 18 kHz bandwidth with
// steep analogue filters points at ~40 kHz (SOS UA RMX16 review quotes 40 kHz).
constexpr double kIntFs = 40000.0;

inline int msToSamp (double ms) { return (int) std::lround (ms * 0.001 * kIntFs); }

// 16-bit two's complement processing word with saturation.
// The RMX16 crunched 16-bit fixed point; overflow lights the o/flo LED.
struct Word16
{
    bool enabled = true;
    bool overflow = false;
    inline float operator() (float x) noexcept
    {
        if (! enabled) return x;
        float s = x * 32768.0f;
        if (s > 32767.0f)  { s = 32767.0f;  overflow = true; }
        else if (s < -32768.0f) { s = -32768.0f; overflow = true; }
        return (float) std::lrintf (s) * (1.0f / 32768.0f);
    }
};

struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void init (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    // read(D) BEFORE push(): returns x[n-D], D >= 1
    inline float read (int D) const noexcept { return buf[(size_t) ((w - D) & mask)]; }
    inline float readFrac (float D) const noexcept
    {
        int i = (int) D; float f = D - (float) i;
        float a = read (i), b = read (i + 1);
        return a + (b - a) * f;
    }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
};

// Schroeder allpass
struct Allpass
{
    DelayLine d; int D = 1; float g = 0.5f;
    void init (int samples, float gain) { D = std::max (1, samples); g = gain; d.init (D + 4); }
    void clear() { d.clear(); }
    inline float process (float x) noexcept
    {
        float v = d.read (D);
        float u = x + g * v;
        d.push (u);
        return v - g * u;
    }
};

struct OnePoleLP
{
    float a = 0.0f, z = 0.0f; // y = (1-a)x + a y
    void setCutoff (double hz, double fs)
    {
        if (hz <= 0.0 || hz >= 0.49 * fs) { a = 0.0f; return; }
        a = (float) std::exp (-2.0 * kPi * hz / fs);
    }
    void clear() { z = 0.0f; }
    inline float process (float x) noexcept { z = (1.0f - a) * x + a * z; return z; }
};

// ---------------------------------------------------------------------------
// RMX16 decay-filter law (manual, Appendix C):
//   HF control: setting 0..-9 -> frequency at which the reverberant decay time
//               is HALF the indicated time: 18k,15k,10k,8k,6.5k,5k,3.5k,2k,1.5k,1k
//   LF control: affects decay time at 350 Hz, from 1/4x (-9) to 4x (+9).
// ---------------------------------------------------------------------------
inline double hfHalfDecayHz (int hi)
{
    static const double t[10] = { 18000, 15000, 10000, 8000, 6500, 5000, 3500, 2000, 1500, 1000 };
    hi = std::clamp (hi, -9, 0);
    return t[-hi];
}
inline double lfDecayMultiplier (int lo) { return std::pow (4.0, std::clamp (lo, -9, 9) / 9.0); }

// Per-loop decay filter: two-band gain (350 Hz crossover) + one-pole HF damper
// solved exactly so that |H(f_half)| gives half the decay time at f_half.
struct DecayFilter
{
    float gMid = 1.0f, gLow = 1.0f;
    float xc = 0.0f, lpLow = 0.0f;    // crossover state
    float hfA = 0.0f, hfZ = 0.0f;
    bool unity = true, useLow = false;

    void clear() { lpLow = hfZ = 0.0f; }

    // loopSamples: loop delay length; rt: indicated RT60 (s); rt <= 0 -> mute loop
    void set (double loopSamples, double rt, int lo, int hi, bool loAvail, bool hiAvail, bool forceUnity)
    {
        if (forceUnity) { unity = true; gMid = gLow = 1.0f; hfA = 0.0f; useLow = false; return; }
        unity = false;
        auto gainFor = [loopSamples] (double t) { return t <= 1.0e-4 ? 0.0 : std::pow (10.0, -3.0 * loopSamples / (kIntFs * t)); };

        double gm = gainFor (rt);
        gMid = (float) gm;

        useLow = loAvail && lo != 0;
        gLow = useLow ? (float) gainFor (rt * lfDecayMultiplier (lo)) : gMid;
        xc = (float) std::exp (-2.0 * kPi * 350.0 / kIntFs);

        // HF: relative factor G = g(RT/2)/g(RT) = gm  at f_half
        int h = hiAvail ? hi : 0;
        double fh = hfHalfDecayHz (h);
        double G = gm;
        if (G >= 0.999999 || G <= 0.0) { hfA = 0.0f; return; }
        double w = 2.0 * kPi * std::min (fh, 0.499 * kIntFs) / kIntFs;
        double A = 1.0 - G * G;
        double B = -2.0 * (1.0 - G * G * std::cos (w));
        double disc = B * B - 4.0 * A * A;
        if (disc < 0.0) disc = 0.0;
        double a = (-B - std::sqrt (disc)) / (2.0 * A);
        hfA = (float) std::clamp (a, 0.0, 0.9999);
    }

    inline float process (float x) noexcept
    {
        if (unity) return x;
        float y;
        if (useLow)
        {
            lpLow = (1.0f - xc) * x + xc * lpLow;
            y = gMid * (x - lpLow) + gLow * lpLow;
        }
        else y = gMid * x;
        hfZ = (1.0f - hfA) * y + hfA * hfZ;
        return hfZ;
    }
};

// Deterministic tiny PRNG (per-program fixed seeds -> repeatable "ROM" voicings)
struct Lcg
{
    uint32_t s;
    explicit Lcg (uint32_t seed) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s = s * 1664525u + 1013904223u; return s; }
    inline float uni() { return (float) ((next() >> 8) & 0xFFFFFF) / 16777216.0f; }   // [0,1)
    inline float bip() { return uni() * 2.0f - 1.0f; }
};

// ---------------------------------------------------------------------------
// Host-rate filters (converter chain)
// ---------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process (double x) noexcept
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0; }
    void lowpass (double fc, double q, double fs)
    {
        double w = 2.0 * kPi * fc / fs, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
        double a0 = 1.0 + al;
        b0 = (1.0 - c) * 0.5 / a0; b1 = (1.0 - c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
    }
    void peak (double fc, double q, double dB, double fs)
    {
        fc = std::min (fc, 0.45 * fs);
        double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * fc / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        double a0 = 1.0 + al / A;
        b0 = (1.0 + al * A) / a0; b1 = -2.0 * c / a0; b2 = (1.0 - al * A) / a0;
        a1 = -2.0 * c / a0; a2 = (1.0 - al / A) / a0;
    }
};

// Even-order Butterworth low-pass as cascaded biquads
struct ButterLP
{
    std::vector<Biquad> st;
    void design (int order, double fc, double fs)
    {
        st.clear();
        fc = std::min (fc, 0.45 * fs);
        int n = order / 2;
        for (int k = 0; k < n; ++k)
        {
            double q = 1.0 / (2.0 * std::sin (kPi * (2.0 * k + 1.0) / (2.0 * order)));
            Biquad b; b.lowpass (fc, q, fs); st.push_back (b);
        }
    }
    void clear() { for (auto& b : st) b.clear(); }
    inline float process (float x) noexcept
    {
        double y = x;
        for (auto& b : st) y = b.process (y);
        return (float) y;
    }
};

inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}
} // namespace epn
