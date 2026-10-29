// Purrrstressor DSP core v0.2. Plain C++17, no JUCE, no allocation after prepare().
// Original design: feed-forward compressor, dB-domain attack/release smoothing,
// per-ratio knee, detector filters, audio filter and two harmonic stages.
// Not based on any vendor code. Behaviour targets come from the public manuals
// (UA EL8 Distressor manual page; Empirical Labs EL8X manual).
// Anything marked OURS is our own choice, not a measurement of hardware.
// Known difference: the UA diagram shows the detector tapping the signal AFTER the
// distortion stage (feedback style). Ours is feed-forward (taps after Input Level)
// because feedback detection with very fast attacks and ratios up to Nuke needs
// extra loop stabilisation. Revisit once the basics are tuned by ear.
#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>

namespace purr {

inline float dbToLin (float db) { return std::pow (10.0f, db * 0.05f); }
inline float linToDb (float x)  { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }

// Biquad, transposed direct form II.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    // RBJ peaking EQ.
    void setPeak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 6.283185307179586 * f / fs, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + al / A;
        b0 = (float) ((1.0 + al * A) / a0);
        b1 = (float) (-2.0 * c / a0);
        b2 = (float) ((1.0 - al * A) / a0);
        a1 = b1;
        a2 = (float) ((1.0 - al / A) / a0);
    }
    // Analog s^2 / (s^2 + b*wc*s + c*wc^2) through the bilinear transform, corner prewarped to fc.
    void setHighPass2 (double fs, double fc, double b, double c)
    {
        const double wc = 6.283185307179586 * fc, K = wc / std::tan (wc / (2.0 * fs));
        const double a0 = K * K + b * wc * K + c * wc * wc;
        b0 = (float) (K * K / a0);
        b1 = (float) (-2.0 * K * K / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * K * K + 2.0 * c * wc * wc) / a0);
        a2 = (float) ((K * K - b * wc * K + c * wc * wc) / a0);
    }
};

// First order high-pass: analog s / (s + a*wc), bilinear, corner prewarped.
struct OnePoleHP
{
    float b0 = 1, b1 = -1, a1 = 0, x1 = 0, y1 = 0;
    void reset() { x1 = y1 = 0; }
    float process (float x)
    {
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x; y1 = y;
        return y;
    }
    void set (double fs, double fc, double a = 1.0)
    {
        const double wc = 6.283185307179586 * fc, K = wc / std::tan (wc / (2.0 * fs));
        const double a0 = K + a * wc;
        b0 = (float) (K / a0);
        b1 = -b0;
        a1 = (float) ((a * wc - K) / a0);
    }
};

// 3rd order Bessel high-pass (18 dB/oct), -3 dB at fc. Prototype poles are the
// standard delay-normalised 3rd order Bessel poles; scaled here to -3 dB at 1 rad/s.
struct BesselHP3
{
    OnePoleHP one;
    Biquad two;
    void reset() { one.reset(); two.reset(); }
    float process (float x) { return two.process (one.process (x)); }
    void set (double fs, double fc)
    {
        using C = std::complex<double>;
        const C p1 (-2.3221854, 0.0), p2 (-1.8389073, 1.7543810), p3 = std::conj (p2);
        const C num = p1 * p2 * p3;
        double lo = 0.1, hi = 10.0;
        for (int i = 0; i < 60; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            const C s (0.0, mid);
            (std::abs (num / ((s - p1) * (s - p2) * (s - p3))) > 0.70710678 ? lo : hi) = mid;
        }
        const double w3 = 0.5 * (lo + hi);
        const C r1 = 1.0 / (p1 / w3), r2 = 1.0 / (p2 / w3); // high-pass poles at fc = 1 rad/s
        one.set (fs, fc, -r1.real());
        two.setHighPass2 (fs, fc, -2.0 * r2.real(), std::norm (r2));
    }
};

// Ratio steps of the Ratio button. Index 5 is the 10:1 "Opto" curve (separate detector
// behaviour in the hardware), index 7 is Nuke.
inline constexpr float kRatios[8]   = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f, 20.0f, 100.0f };
// Knee width per ratio. Manuals: 2:1 and 3:1 are long parabolic knees (2:1 up to 30 dB),
// 4:1 and 6:1 steeper, 10:1 shorter, 20:1 and Nuke hard. Exact widths are OURS.
inline constexpr float kKneeDb[8]   = { 0.0f, 30.0f, 20.0f, 12.0f, 8.0f, 4.0f, 0.5f, 0.5f };
// Threshold per ratio. Manual: each ratio sets threshold and ratio together; values are
// not published, so all equal for now (OURS, to tune by ear).
inline constexpr float kThrDb[8]    = { -18.0f, -18.0f, -18.0f, -18.0f, -18.0f, -18.0f, -18.0f, -18.0f };
inline constexpr int   kOptoIndex = 5;

enum class DistMode { Off = 0, Dist2 = 1, Dist3 = 2 };

struct Params
{
    float inputDb   = 0.0f;    // 0 to +30, drives detector and audio (hardware: input gain and threshold)
    float outputDb  = 0.0f;    // -24 to +24
    float attackMs  = 10.0f;   // 0.05 to 30 (manual: 50 us to 30 ms)
    float releaseMs = 200.0f;  // 50 to 3500 (manual: 0.05 s to 3.5 s; Opto up to 20 s)
    int   ratioIndex = 3;      // 0..7 into kRatios
    bool  detHP     = false;   // detector high-pass, 100 Hz, 6 dB/oct
    bool  detBand   = false;   // detector band emphasis, boost around 6 kHz
    bool  link      = true;    // stereo link
    bool  audioHP   = false;   // audio high-pass, 18 dB/oct Bessel
    DistMode dist   = DistMode::Off;
    float headroomDb = 16.0f;  // plug-in only HR control, 4 to 28 in 4 dB steps, default 16
    float mix       = 1.0f;    // 0 dry .. 1 compressed (parallel, plug-in only)
};

class Compressor
{
public:
    static constexpr float kDetHPHz   = 100.0f; // manual
    static constexpr float kBandHz    = 6000.0f; // manual: "around 6 kHz"
    static constexpr float kBandGainDb = 9.0f;   // OURS, amount not published
    static constexpr float kAudioHPHz = 60.0f;   // -3 dB point from the EL8X manual (about -3 dB at 60 Hz, -12 dB at 30 Hz); UA page says 80 Hz cutoff
    static constexpr float kOptoReleaseGrow = 4.7f; // OURS: 3.5 s * (1 + 4.7) = about 20 s

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        smoothCoef = 1.0f - std::exp (-1.0f / (0.010f * (float) fs)); // about 10 ms
        dcR = 1.0f - 6.283185f * 10.0f / (float) fs;
        for (int c = 0; c < 2; ++c)
        {
            detHP[c].set (fs, kDetHPHz);
            detBand[c].setPeak (fs, kBandHz, 0.7, kBandGainDb);
            audHP[c].set (fs, kAudioHPHz);
        }
        setParams (p);
        curIn = tgtIn; curOut = tgtOut;
        reset();
    }

    void reset()
    {
        gr[0] = gr[1] = 0; grMeter = 0;
        for (int c = 0; c < 2; ++c) { detHP[c].reset(); detBand[c].reset(); audHP[c].reset(); dcX[c] = dcY[c] = 0; }
    }

    void setParams (const Params& np)
    {
        p = np;
        p.ratioIndex = std::clamp (p.ratioIndex, 0, 7);
        p.attackMs   = std::clamp (p.attackMs, 0.05f, 30.0f);
        p.releaseMs  = std::clamp (p.releaseMs, 50.0f, 3500.0f);
        p.mix        = std::clamp (p.mix, 0.0f, 1.0f);
        p.headroomDb = std::clamp (p.headroomDb, 4.0f, 28.0f);
        tgtIn  = dbToLin (std::clamp (p.inputDb, 0.0f, 30.0f));
        tgtOut = dbToLin (std::clamp (p.outputDb, -24.0f, 24.0f));
        aAtk   = std::exp (-1.0f / (p.attackMs * 0.001f * (float) fs));
    }

    // r may be nullptr for mono.
    void process (float* l, float* r, int n)
    {
        const bool stereo = (r != nullptr);
        const int  ri = p.ratioIndex;
        const float ratio = kRatios[ri];
        const bool compress = ratio > 1.0f;
        const int chans = stereo ? 2 : 1;

        for (int i = 0; i < n; ++i)
        {
            curIn  = approach (curIn,  tgtIn);
            curOut = approach (curOut, tgtOut);

            float dry[2] = { l[i], stereo ? r[i] : 0.0f };
            float x[2]   = { dry[0] * curIn, dry[1] * curIn };

            float lv[2] = { -180.0f, -180.0f };
            for (int c = 0; c < chans; ++c)
            {
                float d = x[c];
                if (p.detHP)   d = detHP[c].process (d);
                if (p.detBand) d = detBand[c].process (d);
                lv[c] = linToDb (std::fabs (d));
            }
            if (! stereo) lv[1] = lv[0];

            if (compress)
            {
                if (p.link || ! stereo)
                {
                    gr[0] = smooth (gr[0], grTarget (std::max (lv[0], lv[1]), ri), ri);
                    gr[1] = gr[0];
                }
                else
                {
                    gr[0] = smooth (gr[0], grTarget (lv[0], ri), ri);
                    gr[1] = smooth (gr[1], grTarget (lv[1], ri), ri);
                }
            }
            else gr[0] = gr[1] = 0.0f;

            grMeter = std::max (gr[0], gr[1]);

            float wet[2];
            for (int c = 0; c < chans; ++c)
            {
                // Order from the UA signal flow diagram: Input, VCA, Distortion, Audio HP, Output.
                float y = x[c] * dbToLin (-gr[c]);
                if (p.dist != DistMode::Off) y = distort (y, gr[c], c);
                if (p.audioHP) y = audHP[c].process (y);
                wet[c] = y * curOut;
            }

            for (int c = 0; c < chans; ++c)
            {
                float o;
                if (p.mix >= 1.0f) o = wet[c];
                else if (p.mix <= 0.0f) o = dry[c];
                else o = dry[c] * (1.0f - p.mix) + wet[c] * p.mix;

                if (! std::isfinite (o)) { reset(); o = 0.0f; }
                (c == 0 ? l : r)[i] = o;
            }
        }
    }

    float grDb() const { return grMeter; } // positive dB, for the GR meter

private:
    float approach (float cur, float tgt) const
    {
        const float d = tgt - cur;
        if (std::fabs (d) < 1.0e-7f) return tgt;
        return cur + d * smoothCoef;
    }

    // Static gain computer, returns gain reduction in dB (>= 0).
    static float grTarget (float levelDb, int ri)
    {
        const float ratio = kRatios[ri], thr = kThrDb[ri], w = kKneeDb[ri];
        const float over = levelDb - thr;
        if (2.0f * over < -w) return 0.0f;
        float out;
        if (2.0f * std::fabs (over) <= w)
        {
            const float k = over + w * 0.5f;
            out = levelDb + (1.0f / ratio - 1.0f) * k * k / (2.0f * w);
        }
        else out = thr + over / ratio;
        return std::max (0.0f, levelDb - out);
    }

    // One-pole smoothing in the dB domain; attack when GR grows, release when it falls.
    // Opto (10:1): release grows with depth, up to about 20 s at the longest setting (OURS).
    float smooth (float cur, float target, int ri) const
    {
        if (target > cur) return target + aAtk * (cur - target);
        float relMs = p.releaseMs;
        if (ri == kOptoIndex) relMs *= 1.0f + kOptoReleaseGrow * std::min (cur / 20.0f, 1.0f);
        const float aRel = std::exp (-1.0f / (relMs * 0.001f * (float) fs));
        return target + aRel * (cur - target);
    }

    // Dist 2: mostly 2nd harmonic. Dist 3: 3rd dominant with some 2nd (manuals).
    // Amount rises with gain reduction and with Headroom (HR, default 16 dB). Constants OURS.
    float distort (float x, float grDbNow, int c)
    {
        const float lf = 0.4f + 0.6f * std::min (grDbNow, 20.0f) / 20.0f;
        const float d  = dbToLin (p.headroomDb - 16.0f);
        const float a2 = (p.dist == DistMode::Dist2 ? 0.15f : 0.05f) * lf * d;
        float y = x + a2 * x * x;
        if (p.dist == DistMode::Dist3)
        {
            const float b = 0.3f * lf * d * d;
            const float lim = 0.9f / std::sqrt (3.0f * b); // keep the cubic monotonic
            const float xc = std::clamp (y, -lim, lim);
            y = xc - b * xc * xc * xc;
        }
        const float o = y - dcX[c] + dcR * dcY[c]; // DC blocker (x^2 makes DC)
        dcX[c] = y; dcY[c] = o;
        return o;
    }

    Params p;
    double fs = 48000.0;
    float smoothCoef = 0.01f, dcR = 0.999f, aAtk = 0.9f;
    float tgtIn = 1.0f, tgtOut = 1.0f, curIn = 1.0f, curOut = 1.0f;
    float gr[2] = { 0, 0 }, grMeter = 0;
    OnePoleHP detHP[2];
    Biquad detBand[2];
    BesselHP3 audHP[2];
    float dcX[2] = { 0, 0 }, dcY[2] = { 0, 0 };
};

} // namespace purr
