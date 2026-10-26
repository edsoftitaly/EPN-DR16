// Offline verification for the DR-16 DSP core (no JUCE).
#include "../Source/dsp/Machine.h"
#include <cstdio>
#include <chrono>

using namespace epn;

static double schroederRT (std::vector<float> l, std::vector<float> r, double fs, double hiDb, double loDb, double bandHz = 1000.0)
{
    if (bandHz > 0)   // 1-octave-ish band (two 2nd-order bandpasses)
    {
        for (auto* v : { &l, &r })
            for (int pass = 0; pass < 2; ++pass)
            {
                double w = 2 * kPi * bandHz / fs, al = std::sin (w) / (2 * 1.4), a0 = 1 + al;
                double b0 = al / a0, b2 = -al / a0, a1 = -2 * std::cos (w) / a0, a2 = (1 - al) / a0, z1 = 0, z2 = 0;
                for (auto& x : *v) { double y = b0 * x + z1; z1 = -a1 * y + z2; z2 = b2 * x - a2 * y; x = (float) y; }
            }
    }
    size_t n = l.size();
    std::vector<double> e (n);
    double acc = 0;
    for (size_t i = n; i-- > 0;) { acc += (double) l[i] * l[i] + (double) r[i] * r[i]; e[i] = acc; }
    double e0 = e[0];
    if (e0 <= 0) return 0;
    // start at the peak energy point of the late field (skip predelay)
    double t1 = -1, t2 = -1;
    for (size_t i = 0; i < n; ++i)
    {
        double db = 10 * std::log10 (e[i] / e0 + 1e-30);
        if (t1 < 0 && db <= hiDb) t1 = i / fs;
        if (t2 < 0 && db <= loDb) { t2 = i / fs; break; }
    }
    if (t1 < 0 || t2 < 0) return -1;
    return (t2 - t1) * 60.0 / (hiDb - loDb);
}

static void run (Dr16Core& core, const MachineParams& p, double fs, double seconds, std::vector<float>& L, std::vector<float>& R, bool impulse = true)
{
    core.prepare (fs);
    core.setParams (p);
    size_t n = (size_t) (seconds * fs);
    std::vector<float> in (n, 0.0f);
    if (impulse) in[(size_t) (0.01 * fs)] = 0.9f;
    L.assign (n, 0); R.assign (n, 0);
    const int B = 256;
    for (size_t i = 0; i < n; i += B)
    {
        int m = (int) std::min ((size_t) B, n - i);
        core.process (in.data() + i, L.data() + i, R.data() + i, m);
    }
}

int main()
{
    const double fs = 48000.0;
    Dr16Core core;
    std::vector<float> L, R;
    int fails = 0;

    std::printf ("== Decay accuracy (indicated vs measured T20 RT60, lo=0 hi=0) ==\n");
    for (int prog : { 1, 2, 3, 4, 5, 12, 13, 14, 15 })
    {
        for (float d : { 1.0f, 2.5f, 5.0f })
        {
            const auto& s = program (prog);
            float dd = std::min (d, s.decMax);
            MachineParams p; p.program = prog; p.decay = dd; p.lo = 0; p.hi = 0; p.preMs = 0;
            run (core, p, fs, std::max (3.0, dd * 2.2 + 1.0), L, R);
            double rt = schroederRT (L, R, fs, -5, -25);
            bool bad = false;
            for (size_t i = 0; i < L.size(); ++i) if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true;
            if (bad) ++fails;
            std::printf ("  %-9s  set %.1f s  -> %.2f s %s\n", s.name, dd, rt, bad ? "NaN!" : "");
            if (dd == s.decMax || d == 5.0f) {}
        }
    }

    std::printf ("\n== HF control (AMBIENCE 2.0 s): RT at 1k vs 8k octave proxy via hi=-9 ==\n");
    {
        MachineParams p; p.program = 1; p.decay = 2.0f;
        p.hi = 0; run (core, p, fs, 6.0, L, R); double a = schroederRT (L, R, fs, -5, -25);
        p.hi = -9; run (core, p, fs, 6.0, L, R); double b = schroederRT (L, R, fs, -5, -25);
        p.hi = 0; p.lo = 9; run (core, p, fs, 12.0, L, R); double c = schroederRT (L, R, fs, -5, -25);
        std::printf ("  broadband RT: hi 0 = %.2f, hi -9 = %.2f, lo +9 = %.2f\n", a, b, c);
    }

    std::printf ("\n== Tapped programs: gate length (time to -40 dB envelope) ==\n");
    for (int prog : { 8, 18, 17, 9, 10 })
    {
        for (float d : { 1.0f, 3.0f, 6.0f })
        {
            MachineParams p; p.program = prog; p.decay = d; p.preMs = 0;
            run (core, p, fs, 2.0, L, R);
            // find last sample above -40 dB of peak
            float pk = 0; size_t last = 0;
            for (size_t i = 0; i < L.size(); ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
            for (size_t i = 0; i < L.size(); ++i) if (std::max (std::abs (L[i]), std::abs (R[i])) > pk * 0.01f) last = i;
            std::printf ("  %-9s d=%.1f  expected %.0f ms  measured tail end %.0f ms  peak %.3f\n", program (prog).name, d,
                         TapReverb::gateSeconds (program (prog).tap, d) * 1000.0, (last / fs - 0.01) * 1000.0, pk);
        }
    }

    std::printf ("\n== FREEZE / AMBIENCE 9.9 stability (30 s white noise burst then hold) ==\n");
    for (int prog : { 11, 1 })
    {
        MachineParams p; p.program = prog; p.decay = (prog == 11) ? 0.2f : 9.9f;
        core.prepare (fs); core.setParams (p);
        size_t n = (size_t) (30 * fs);
        std::vector<float> in (n, 0.0f), l (n), r (n);
        Lcg g (7);
        for (size_t i = (size_t) (0.3 * fs); i < (size_t) (1.3 * fs); ++i) in[i] = 0.25f * g.bip();
        core.process (in.data(), l.data(), r.data(), (int) n);
        auto rms = [&] (double t0, double t1) { double a = 0; size_t c = 0; for (size_t i = (size_t) (t0 * fs); i < (size_t) (t1 * fs); ++i) { a += l[i] * l[i]; ++c; } return 10 * std::log10 (a / c + 1e-30); };
        std::printf ("  %-9s rms @2-3s %.1f dB, @27-30s %.1f dB  overflow=%d\n", program (prog).name, rms (2, 3), rms (27, 30), (int) core.takeOverflow());
    }

    std::printf ("\n== Converter chain frequency response (DELAY out 1 = straight through) ==\n");
    for (double f : { 100.0, 1000.0, 10000.0, 15000.0, 18000.0, 20000.0 })
    {
        MachineParams p; p.program = 16; p.preMs = 100; p.decay = 0;
        core.prepare (fs); core.setParams (p);
        size_t n = (size_t) (1.0 * fs);
        std::vector<float> in (n), l (n), r (n);
        for (size_t i = 0; i < n; ++i) in[i] = 0.5f * (float) std::sin (2 * kPi * f * i / fs);
        core.process (in.data(), l.data(), r.data(), (int) n);
        double a = 0; for (size_t i = n / 2; i < n; ++i) a += l[i] * l[i];
        a = std::sqrt (2 * a / (n / 2)) / 0.5;
        std::printf ("  %6.0f Hz : %+.2f dB\n", f, 20 * std::log10 (a + 1e-12));
    }

    std::printf ("\n== CPU: 10 s stereo-out render per program @48k ==\n");
    for (int prog = 1; prog <= 18; ++prog)
    {
        MachineParams p; p.program = prog; p.decay = program (prog).decDef; p.preMs = (float) program (prog).preDef;
        core.prepare (fs); core.setParams (p);
        size_t n = (size_t) (10 * fs);
        std::vector<float> in (n), l (n), r (n);
        Lcg g (3); for (auto& v : in) v = 0.1f * g.bip();
        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < n; i += 512) core.process (in.data() + i, l.data() + i, r.data() + i, (int) std::min<size_t> (512, n - i));
        double ms = std::chrono::duration<double, std::milli> (std::chrono::high_resolution_clock::now() - t0).count();
        bool bad = false; for (size_t i = 0; i < n; ++i) if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) bad = true;
        if (bad) ++fails;
        std::printf ("  %2d %-9s %6.1f ms  (%.2f%% realtime) %s\n", prog, program (prog).name, ms, ms / 100.0, bad ? "NaN!" : "");
    }
    std::printf ("\n== Host sample rates: all programs, 3 s noise, finite + level ==\n");
    for (double hfs : { 44100.0, 88200.0, 96000.0, 192000.0 })
    {
        double worst = 0; int bad = 0;
        for (int prog = 1; prog <= 18; ++prog)
        {
            MachineParams p; p.program = prog; p.decay = program (prog).decDef; p.preMs = (float) program (prog).preDef;
            core.prepare (hfs); core.setParams (p);
            size_t n = (size_t) (3 * hfs);
            std::vector<float> in (n), l (n), r (n);
            Lcg g (5); for (auto& v : in) v = 0.1f * g.bip();
            for (size_t i = 0; i < n; i += 480) core.process (in.data() + i, l.data() + i, r.data() + i, (int) std::min<size_t> (480, n - i));
            for (size_t i = 0; i < n; ++i) { if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) ++bad; worst = std::max (worst, (double) std::abs (l[i])); }
        }
        fails += bad;
        std::printf ("  %6.0f Hz  non-finite samples: %d   max |out| %.3f\n", hfs, bad, worst);
    }
    std::printf ("\nfails: %d\n", fails);
    return fails;
}
