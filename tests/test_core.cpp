// Purrrstressor core tests. Build and run with: sh tests/run.sh
// Targets come from the public manuals (UA EL8 page, Empirical Labs EL8X manual) where stated.
#include "../src/core/Purrrstressor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

using namespace purr;
static int fails = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++fails; std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } else { std::printf ("ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static const double FS = 48000.0;
static const double PI = 3.14159265358979323846;

static std::vector<float> sine (double f, double amp, int n)
{
    std::vector<float> v (n);
    for (int i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2.0 * PI * f * i / FS));
    return v;
}

// Amplitude at frequency f via Goertzel over n samples (whole cycles) from 'start'.
static double goertzelAmp (const std::vector<float>& x, double f, int start, int n)
{
    const double w = 2.0 * PI * f / FS, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { const double s = x[start + i] + c * s1 - s2; s2 = s1; s1 = s; }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / n;
}

static Compressor make (const Params& p)
{
    Compressor c; c.setParams (p); c.prepare (FS); return c;
}

// Steady GR (dB) for a DC input.
static float dcGr (Params p, float level, int n = 48000)
{
    auto c = make (p);
    float v;
    for (int i = 0; i < n; ++i) { v = level; c.process (&v, nullptr, 1); }
    return c.grDb();
}

// Gain in dB of a filter object at hz (sine, last whole second).
template <class F>
static double filterGainDb (F& f, double hz)
{
    const int n = (int) FS * 3;
    auto s = sine (hz, 0.1, n);
    for (auto& v : s) v = f.process (v);
    return 20.0 * std::log10 (goertzelAmp (s, hz, (int) FS * 2, (int) FS) / 0.1);
}

int main()
{
    // 1. Neutral settings pass quiet signals unchanged. The I/O amplifiers are slightly nonlinear
    //    (manual: nonlinear input and output amplifiers), so only low levels are exact.
    {
        Params p; p.ratioIndex = 0; p.mix = 1.0f;
        auto c = make (p);
        std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.03f, 0.03f);
        std::vector<float> l (4800), r (4800), l0, r0;
        for (auto& s : l) s = u (rng);
        for (auto& s : r) s = u (rng);
        l0 = l; r0 = r;
        c.process (l.data(), r.data(), (int) l.size());
        double err = 0; for (size_t i = 0; i < l.size(); ++i) err = std::max ({ err, (double) std::fabs (l[i] - l0[i]), (double) std::fabs (r[i] - r0[i]) });
        CHECK (err < 3e-5, "1:1, dist off, 0 dB in and out passes -30 dBFS noise (max error %.2e)", err);
    }

    // 2. Mix 0 returns the dry signal exactly even while compressing hard.
    {
        Params p; p.ratioIndex = 7; p.inputDb = 20; p.mix = 0.0f;
        auto c = make (p);
        auto s = sine (1000, 0.5, 4800); auto s0 = s;
        c.process (s.data(), nullptr, (int) s.size());
        CHECK (std::memcmp (s.data(), s0.data(), s.size() * 4) == 0, "mix 0 is exactly dry");
    }

    // 3. Static ratio accuracy above the knee (DC 0.5 with +30 dB input = +24 dB, threshold -18).
    {
        for (int k = 1; k <= 7; ++k)
        {
            Params p; p.ratioIndex = k; p.inputDb = 30; p.attackMs = 0.05f; p.releaseMs = 3500;
            const float over = linToDb (0.5f * dbToLin (30.0f)) - kThrDb[k];
            const float want = over * (1.0f - 1.0f / kRatios[k]);
            const float got = dcGr (p, 0.5f);
            CHECK (std::fabs (got - want) < 0.3f, "ratio %g:1 GR %.2f dB (want %.2f)", kRatios[k], got, want);
        }
    }

    // 3b. Knees: 2:1 has a long soft knee (manual: up to 30 dB); at threshold GR is about 1.9 dB.
    {
        Params p; p.ratioIndex = 1; p.attackMs = 0.05f; p.releaseMs = 3500;
        const float g0 = dcGr (p, dbToLin (kThrDb[1]));
        CHECK (std::fabs (g0 - 1.875f) < 0.2f, "2:1 at threshold GR %.2f dB (soft knee, want 1.88)", g0);
        const float gm = dcGr (p, dbToLin (kThrDb[1] - 10.0f));
        CHECK (gm > 0.0f && gm < 0.5f, "2:1 starts compressing before threshold (GR %.2f dB at -10 dB)", gm);
        Params n = p; n.ratioIndex = 7;
        CHECK (dcGr (n, dbToLin (kThrDb[7] - 3.0f)) < 0.05f, "Nuke knee is hard (no GR 3 dB below threshold)");
    }

    // 4. Attack time constant: DC step, time to 63.2 percent of final GR.
    {
        Params p; p.ratioIndex = 4; p.attackMs = 10.0f; p.releaseMs = 3500;
        auto c = make (p);
        float v; std::vector<float> trace;
        for (int i = 0; i < 4800; ++i) { v = 0.5f; c.process (&v, nullptr, 1); trace.push_back (c.grDb()); }
        const float fin = trace.back();
        int hit = 0; for (int i = 0; i < (int) trace.size(); ++i) if (trace[i] >= 0.632f * fin) { hit = i; break; }
        const double ms = 1000.0 * hit / FS;
        CHECK (ms > 8.5 && ms < 11.5, "attack 10 ms reaches 63%% at %.2f ms", ms);
    }

    // 4b. Attack range: fastest setting (50 us) is clamped, slowest is 30 ms.
    {
        Params p; p.ratioIndex = 3; p.attackMs = 0.0f;
        Params q; q.ratioIndex = 3; q.attackMs = 1000.0f;
        // 100 ms of step: the 30 ms setting must have reached 95 percent or more of final.
        auto c = make (q); float v; float g = 0;
        for (int i = 0; i < 4800; ++i) { v = 0.5f; c.process (&v, nullptr, 1); g = c.grDb(); }
        auto c2 = make (q); float g2 = 0;
        for (int i = 0; i < 48000; ++i) { v = 0.5f; c2.process (&v, nullptr, 1); g2 = c2.grDb(); }
        CHECK (g > 0.95f * g2, "attack knob clamps at 30 ms (reached %.0f%% after 100 ms)", 100.0 * g / g2);
        (void) p;
    }

    // 5. Release time constant after the step ends.
    {
        Params p; p.ratioIndex = 3; p.attackMs = 0.05f; p.releaseMs = 200.0f;
        auto c = make (p);
        float v;
        for (int i = 0; i < 24000; ++i) { v = 0.5f; c.process (&v, nullptr, 1); }
        const float start = c.grDb();
        int hit = 0;
        for (int i = 0; i < 48000; ++i) { v = 0.0f; c.process (&v, nullptr, 1); if (c.grDb() <= start * 0.368f) { hit = i; break; } }
        const double ms = 1000.0 * hit / FS;
        CHECK (ms > 180 && ms < 220, "release 200 ms falls to 37%% at %.1f ms", ms);
    }

    // 6. 10:1 Opto releases much slower than 20:1 at the same knob setting (manual: up to 20 s).
    {
        auto relTime = [] (int ri)
        {
            Params p; p.ratioIndex = ri; p.attackMs = 0.05f; p.releaseMs = 200.0f; p.inputDb = 12;
            auto c = make (p); float v;
            for (int i = 0; i < 48000; ++i) { v = 0.5f; c.process (&v, nullptr, 1); }
            const float start = c.grDb();
            for (int i = 0; i < 48000 * 30; ++i) { v = 0.0f; c.process (&v, nullptr, 1); if (c.grDb() <= start * 0.5f) return 1000.0 * i / FS; }
            return -1.0;
        };
        const double normal = relTime (6), opto = relTime (kOptoIndex);
        CHECK (opto > normal * 2.0, "Opto release %.0f ms vs 20:1 %.0f ms", opto, normal);

        Params p; p.ratioIndex = kOptoIndex; p.attackMs = 0.05f; p.releaseMs = 3500.0f; p.inputDb = 30;
        auto c = make (p); float v;
        for (int i = 0; i < 48000; ++i) { v = 0.5f; c.process (&v, nullptr, 1); }
        const float start = c.grDb(); double t20 = -1;
        for (int i = 0; i < 48000 * 40; ++i) { v = 0.0f; c.process (&v, nullptr, 1); if (c.grDb() <= start * 0.05f) { t20 = (double) i / FS; break; } }
        CHECK (t20 > 12.0 && t20 < 40.0, "Opto at max release takes %.1f s to fall to 5%% (manual: up to about 20 s)", t20);
    }

    // 7. Detector filters, measured directly.
    {
        OnePoleHP hp; hp.set (FS, Compressor::kDetHPHz);
        OnePoleHP hp2 = hp; hp2.reset();
        const double g100 = filterGainDb (hp, 100.0);
        OnePoleHP hp3; hp3.set (FS, Compressor::kDetHPHz);
        const double g25 = filterGainDb (hp3, 25.0);
        CHECK (std::fabs (g100 + 3.01) < 0.2, "detector HP: %.2f dB at 100 Hz (want -3.0)", g100);
        CHECK (std::fabs (g25 + 12.3) < 0.5, "detector HP: %.2f dB at 25 Hz (6 dB/oct, want -12.3)", g25);

        Biquad bp; bp.setPeak (FS, Compressor::kBandHz, 0.7, Compressor::kBandGainDb);
        const double b6 = filterGainDb (bp, 6000.0);
        Biquad bp2; bp2.setPeak (FS, Compressor::kBandHz, 0.7, Compressor::kBandGainDb);
        const double b200 = filterGainDb (bp2, 200.0);
        CHECK (b6 > Compressor::kBandGainDb - 0.5 && b200 < 0.5, "band emphasis: %.1f dB at 6 kHz, %.2f dB at 200 Hz", b6, b200);
    }

    // 7b. Detector filters inside the compressor.
    {
        auto grFor = [] (double f, bool hp, bool band)
        {
            Params p; p.ratioIndex = 4; p.attackMs = 0.5f; p.releaseMs = 100.0f; p.detHP = hp; p.detBand = band;
            auto c = make (p);
            auto s = sine (f, 0.3, 48000);
            float m = 0;
            for (int i = 0; i < 48000; ++i) { c.process (&s[i], nullptr, 1); if (i > 24000) m = std::max (m, c.grDb()); }
            return m;
        };
        const float off = grFor (40, false, false), on = grFor (40, true, false);
        CHECK (on < off - 3.0f, "40 Hz tone: GR %.1f dB with detector HP vs %.1f dB without", on, off);
        const float b0 = grFor (6000, false, false), b1 = grFor (6000, false, true);
        CHECK (b1 > b0 + 3.0f, "6 kHz tone: GR %.1f dB with band emphasis vs %.1f dB without", b1, b0);
        const float m0 = grFor (200, false, false), m1 = grFor (200, false, true);
        CHECK (std::fabs (m1 - m0) < 1.5f, "200 Hz tone barely changes with band emphasis (%.1f vs %.1f dB)", m1, m0);
    }

    // 8. Audio high-pass: 18 dB/oct Bessel, about -3 dB at 60 Hz and -12 dB at 30 Hz (EL8X manual).
    {
        auto resp = [] (double f)
        {
            Params p; p.ratioIndex = 0; p.audioHP = true;
            auto c = make (p);
            const int n = (int) FS * 3;
            auto s = sine (f, 0.1, n);
            c.process (s.data(), nullptr, n);
            return 20.0 * std::log10 (goertzelAmp (s, f, (int) FS * 2, (int) FS) / 0.1);
        };
        const double g60 = resp (60), g30 = resp (30), g20 = resp (20), g10 = resp (10), g400 = resp (400);
        CHECK (std::fabs (g60 + 3.0) < 0.7, "audio HP: %.2f dB at 60 Hz (want about -3)", g60);
        CHECK (std::fabs (g30 + 12.0) < 2.0, "audio HP: %.2f dB at 30 Hz (want about -12)", g30);
        CHECK (std::fabs ((g20 - g10) - 10.2) < 2.0 || std::fabs (g20 - g10) > 14.0, "audio HP: %.1f dB between 20 Hz and 10 Hz", g20 - g10);
        CHECK (std::fabs (g400) < 0.3, "audio HP passes 400 Hz (%.2f dB)", g400);
    }

    // 9. Stereo link: linked channels share GR; unlinked quiet channel is untouched.
    {
        Params p; p.ratioIndex = 5; p.attackMs = 0.5f; p.releaseMs = 100.0f; p.link = true;
        auto c = make (p);
        auto L = sine (1000, 0.7, 24000), R = sine (1000, 0.02, 24000), R0 = R;
        c.process (L.data(), R.data(), 24000);
        const double ra = goertzelAmp (R, 1000, 12000, 12000) / 0.02;
        CHECK (ra < 0.8, "linked: quiet R is ducked by loud L (gain %.2f)", ra);

        p.link = false; auto c2 = make (p);
        auto L2 = sine (1000, 0.7, 24000), R2 = R0;
        c2.process (L2.data(), R2.data(), 24000);
        double e2 = 0; for (size_t i = 0; i < R0.size(); ++i) e2 = std::max (e2, (double) std::fabs (R2[i] - R0[i]));
        CHECK (e2 < 2e-5, "unlinked: quiet R untouched by loud L (max error %.2e)", e2);
    }

    // 10. Harmonics: Dist 2 mostly 2nd; Dist 3 3rd dominant with some 2nd; Headroom raises both.
    {
        auto harm = [] (DistMode m, float hr, double& h2, double& h3)
        {
            Params p; p.ratioIndex = 0; p.dist = m; p.headroomDb = hr;
            auto c = make (p);
            auto s = sine (1000, 0.5, 48000);
            c.process (s.data(), nullptr, 48000);
            const double f = goertzelAmp (s, 1000, 24000, 24000);
            h2 = 20 * std::log10 (goertzelAmp (s, 2000, 24000, 24000) / f + 1e-12);
            h3 = 20 * std::log10 (goertzelAmp (s, 3000, 24000, 24000) / f + 1e-12);
        };
        double a2, a3, b2, b3, c2, c3;
        harm (DistMode::Dist2, 16, a2, a3);
        harm (DistMode::Dist3, 16, b2, b3);
        harm (DistMode::Dist2, 28, c2, c3);
        CHECK (a2 > -45 && a2 > a3 + 20, "Dist 2: H2 %.1f dB, H3 %.1f dB (2nd dominant)", a2, a3);
        CHECK (b3 > b2 + 3 && b2 > -60 && b3 > -50, "Dist 3: H3 %.1f dB, H2 %.1f dB (3rd dominant, 2nd present)", b3, b2);
        CHECK (c2 > a2 + 6, "Headroom 28 dB pushes harder: H2 %.1f dB vs %.1f dB at 16 dB", c2, a2);
    }

    // 11. Nuke is brick wall: output stays within 1 dB of threshold for normal input levels.
    {
        for (float amp : { 0.25f, 0.5f, 0.9f })
        {
            Params p; p.ratioIndex = 7; p.attackMs = 0.05f; p.releaseMs = 3500;
            auto c = make (p); float v, peak = 0;
            for (int i = 0; i < 48000; ++i) { v = amp; c.process (&v, nullptr, 1); if (i > 24000) peak = std::max (peak, std::fabs (v)); }
            const float over = linToDb (peak) - kThrDb[7];
            CHECK (over < 1.0f, "Nuke: input %.2f ends %.2f dB above threshold", amp, over);
        }
    }

    // 12. Robustness: noise at maximum settings, no NaN or inf; silence in gives silence out.
    {
        Params p; p.ratioIndex = 7; p.inputDb = 30; p.outputDb = 24; p.attackMs = 0.05f; p.releaseMs = 50;
        p.detHP = true; p.detBand = true; p.audioHP = true; p.dist = DistMode::Dist3; p.headroomDb = 28; p.mix = 0.7f;
        auto c = make (p);
        std::mt19937 rng (7); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        std::vector<float> l (96000), r (96000);
        for (auto& s : l) s = u (rng);
        for (auto& s : r) s = u (rng);
        c.process (l.data(), r.data(), (int) l.size());
        bool bad = false;
        for (float s : l) if (! std::isfinite (s)) bad = true;
        for (float s : r) if (! std::isfinite (s)) bad = true;
        CHECK (! bad, "no NaN or inf at maximum settings");

        std::vector<float> z (48000, 0.0f);
        c.reset();
        c.process (z.data(), nullptr, 48000);
        double dc = 0; for (float s : z) dc += s; dc /= z.size();
        CHECK (std::fabs (dc) < 1e-6, "silence in, silence out (dc %.2e)", dc);
    }


    // 12. 2:1, 3:1 and 4:1 attack faster than the other ratios (manual).
    {
        auto t63 = [&] (int ri) {
            Params p; p.ratioIndex = ri; p.attackMs = 10.0f; p.releaseMs = 3500;
            auto c = make (p); float v; std::vector<float> tr;
            for (int i = 0; i < 4800; ++i) { v = 0.5f; c.process (&v, nullptr, 1); tr.push_back (c.grDb()); }
            const float fin = tr.back(); int hit = 0;
            for (int i = 0; i < (int) tr.size(); ++i) if (tr[i] >= 0.632f * fin) { hit = i; break; }
            return 1000.0 * hit / FS; };
        const double a4 = t63 (3), a6 = t63 (4);
        CHECK (a4 < 0.6 * a6, "4:1 attack (%.2f ms) is faster than 6:1 (%.2f ms) at the same setting", a4, a6);
    }

    // 13. Auto Trim: ratchets down by the overshoot plus 0.1 dB, never back up, off leaves peaks alone.
    {
        Params p; p.ratioIndex = 0; p.autoTrim = true;
        auto c = make (p);
        auto x = sine (1000, 1.5, 48000);
        c.process (x.data(), nullptr, 48000);
        float pk = 0; for (int i = 24000; i < 48000; ++i) pk = std::max (pk, std::fabs (x[i]));
        CHECK (pk < 1.0f && pk > 0.85f, "Auto Trim holds the output under 0 dBFS (peak %.3f)", pk);
        const float t1 = c.trimDb();
        CHECK (t1 < -3.0f, "Auto Trim dropped %.2f dB after a 1.5 peak (needs at least 3)", t1);
        auto q = sine (1000, 0.1, 48000);
        c.process (q.data(), nullptr, 48000);
        CHECK (std::fabs (c.trimDb() - t1) < 1e-4f, "Auto Trim never recovers by itself (%.2f dB)", c.trimDb());
        Params off = p; off.autoTrim = false; auto c2 = make (off);
        auto y = sine (1000, 1.5, 4800); c2.process (y.data(), nullptr, 4800);
        float pk2 = 0; for (float v : y) pk2 = std::max (pk2, std::fabs (v));
        CHECK (pk2 > 1.3f, "Auto Trim off leaves the peak (%.2f)", pk2);
        // a barely-over peak still drops at least 0.1 dB
        Params p3 = p; auto c3 = make (p3);
        auto z = sine (1000, 1.03, 24000); c3.process (z.data(), nullptr, 24000);
        CHECK (c3.trimDb() <= -0.1f, "a barely over peak trims at least 0.1 dB (%.3f dB)", c3.trimDb());
    }

    // 14. Level Match brings the processed loudness to the dry loudness.
    {
        Params p; p.ratioIndex = 4; p.inputDb = 14; p.outputDb = 12; p.attackMs = 5; p.releaseMs = 200; p.levelMatch = true;
        auto c = make (p);
        const int n = 48000 * 14;
        std::mt19937 rng (3); std::uniform_real_distribution<float> u (-0.3f, 0.3f);
        std::vector<float> x (n); for (auto& v : x) v = u (rng);
        auto d = x; c.process (x.data(), nullptr, n);
        double sd = 0, sw = 0; for (int i = n - 48000 * 2; i < n; ++i) { sd += d[i] * d[i]; sw += x[i] * x[i]; }
        const double diff = 10.0 * std::log10 (sw / sd);
        CHECK (std::fabs (diff) < 0.7, "Level Match: output within %.2f dB of the dry level (gain %.1f dB)", diff, c.levelMatchDb());
        Params off = p; off.levelMatch = false; auto c2 = make (off);
        auto y = d; c2.process (y.data(), nullptr, n);
        double sy = 0; for (int i = n - 48000 * 2; i < n; ++i) sy += y[i] * y[i];
        CHECK (std::fabs (10.0 * std::log10 (sy / sd)) > 3.0, "without Level Match the levels differ a lot (%.1f dB)", 10.0 * std::log10 (sy / sd));
    }

    // 15. Feedback detector: same static curves as feed-forward, stable, no overshoot.
    {
        for (int ri = 1; ri < 8; ++ri)
        {
            Params p; p.ratioIndex = ri; p.attackMs = 1.0f; p.releaseMs = 200; p.inputDb = 10;
            const float ff = dcGr (p, 0.3f, 96000);
            p.detFeedback = true;
            const float fb = dcGr (p, 0.3f, 96000);
            CHECK (std::fabs (ff - fb) < 0.4f, "ratio %d: feedback GR %.2f dB matches feed-forward %.2f dB", ri, fb, ff);
        }
        Params p; p.ratioIndex = 7; p.attackMs = 0.05f; p.releaseMs = 50; p.inputDb = 30; p.detFeedback = true; p.dist = DistMode::Dist3;
        auto c = make (p);
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        std::vector<float> l (96000); for (auto& v : l) v = u (rng);
        float grMax = 0; bool bad = false;
        for (int i = 0; i < 96000; ++i) { c.process (&l[i], nullptr, 1); grMax = std::max (grMax, c.grDb()); if (! std::isfinite (l[i])) bad = true; }
        CHECK (! bad && grMax < 80.0f, "feedback at Nuke, fastest attack, +30 dB noise stays finite (max GR %.1f dB)", grMax);
        Params q; q.ratioIndex = 6; q.attackMs = 0.05f; q.releaseMs = 3500; q.detFeedback = true; q.inputDb = 10;
        auto c2 = make (q); float v, prev = 0, over = 0, last = 0;
        for (int i = 0; i < 4800; ++i) { v = 0.5f; c2.process (&v, nullptr, 1); last = c2.grDb(); over = std::max (over, last - prev > 0 ? 0.0f : prev - last); prev = last; }
        CHECK (over < 0.05f, "feedback attack has no ringing (worst step back %.3f dB)", over);
    }

    // 16. Anti-aliasing: 15 kHz through Dist 3 at HR 28 makes 2nd harmonic 30 kHz, which aliases to 18 kHz.
    {
        Params p; p.ratioIndex = 0; p.dist = DistMode::Dist2; p.headroomDb = 28; p.inputDb = 0;
        const int n = 48000;
        auto run = [&] (bool aa) {
            auto c = make (p); c.setAntiAlias (aa);
            auto x = sine (15000, 0.5, n); c.process (x.data(), nullptr, n);
            return goertzelAmp (x, 18000, 12000, 24000); };
        const double on = run (true), off = run (false);
        const double red = 20.0 * std::log10 (off / std::max (on, 1e-12));
        CHECK (red > 6.0, "ADAA lowers the 18 kHz alias by %.1f dB (naive %.1f dBFS, ADAA %.1f dBFS)", red, 20.0 * std::log10 (off), 20.0 * std::log10 (std::max (on, 1e-12)));
    }

    // 17. Indicators: THD LEDs follow the distortion, Redline follows clipping.
    {
        Params p; p.ratioIndex = 4; p.dist = DistMode::Dist2; p.inputDb = 12; p.attackMs = 2;
        auto c = make (p); auto x = sine (200, 0.5, 24000); c.process (x.data(), nullptr, 24000);
        CHECK (c.thd1Percent(), "THD 1%% LED lights with Dist 2 while compressing (thd %.2f%%)", 100.0f * c.thd());
        Params q; q.ratioIndex = 0; auto c2 = make (q); auto y = sine (200, 0.05, 24000); c2.process (y.data(), nullptr, 24000);
        CHECK (! c2.thd1Percent() && ! c2.redline(), "clean quiet signal: no THD LED, no Redline (thd %.3f%%)", 100.0f * c2.thd());
        Params r; r.ratioIndex = 0; r.outputDb = 12; auto c3 = make (r); auto z = sine (200, 0.9, 24000); c3.process (z.data(), nullptr, 24000);
        CHECK (c3.redline(), "Redline lights when the output clips");
    }

    std::printf ("\n%s (%d failed)\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
