#pragma once
#include "Widgets.h"
#include "../core/Purrrstressor.h"

namespace ui
{
// Everything the views draw, refreshed by the editor timer (message thread only).
struct Live
{
    double fs = 44100.0;
    float inPk = 0, outPk = 0;       // linear, decaying peak hold
    float grNow = 0;                 // dB, positive
    float thd = 0; bool redline = false;
    float trimDb = 0, matchDb = 0;
    // parameters in real units
    float inputDb = 15, outputDb = 0; int ratio = 4, dist = 0, headroomIdx = 3;
    bool bypass = false, detHP = false, detBand = false, link = true, audioHP = false, autoTrim = false, levelMatch = false;
    float hist[360] {}; int hi = 0;
};

inline float l2d (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }

// Base of the three display canvases: drawn in the mockup's canvas units, scaled into a bordered box.
class CanvasView : public Component, public TipClient
{
public:
    CanvasView (const Ctx& c, const Live& l, float cw, float ch) : ctx (c), live (l), cvW (cw), cvH (ch) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat();
        g.setColour (t.disp);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (1.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (inner.toNearestInt());
            g.addTransform (AffineTransform::scale (inner.getWidth() / cvW, inner.getHeight() / cvH).translated (inner.getX(), inner.getY()));
            draw (g, t, cvW, cvH);
        }
        g.setColour (t.line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
protected:
    virtual void draw (Graphics&, const theme::Look&, float w, float h) = 0;
    static void label (Graphics& g, const String& s, float x, float y, Justification j, Colour c)
    {
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        g.setColour (c);
        g.drawSingleLineText (s, roundToInt (x), roundToInt (y), j);
    }
    static void dashedLine (Graphics& g, Line<float> ln, float a, float b, float w)
    {
        const float d[2] = { a, b };
        g.drawDashedLine (ln, d, 2, w);
    }
    const Ctx& ctx;
    const Live& live;
    float cvW, cvH;
};

class CurveView : public CanvasView
{
public:
    CurveView (const Ctx& c, const Live& l) : CanvasView (c, l, 300, 100) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float x0 = -60, x1 = 6, y0 = -60, y1 = 18;
        auto X = [=] (float v) { return 24.0f + (v - x0) / (x1 - x0) * (w - 30.0f); };
        auto Y = [=] (float v) { return h - 14.0f - (v - y0) / (y1 - y0) * (h - 22.0f); };
        for (float v : { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
        {
            g.setColour (t.line); g.drawLine (X (v), 4, X (v), h - 14, 1.0f);
            label (g, String ((int) v), X (v), h - 4, Justification::horizontallyCentred, t.dim);
        }
        for (float v : { -48.0f, -24.0f, 0.0f, 12.0f })
        {
            g.setColour (t.line); g.drawLine (24, Y (v), w - 6, Y (v), 1.0f);
            label (g, String ((int) v), 21, Y (v) + 3, Justification::right, t.dim);
        }
        g.setColour (Colour (0xff4a4d55));
        dashedLine (g, { X (-60), Y (-60), X (6), Y (6) }, 3, 3, 1.0f);
        Path curve;
        bool first = true;
        for (float v = x0; v <= x1 + 0.01f; v += 0.5f)
        {
            const float gr = live.bypass ? 0.0f : purr::Compressor::staticGrDb (v + live.inputDb, live.ratio);
            const float o = live.bypass ? v : v + live.inputDb - gr + live.outputDb;
            const Point<float> pt (X (v), Y (jlimit (y0, y1, o)));
            if (first) curve.startNewSubPath (pt); else curve.lineTo (pt);
            first = false;
        }
        g.setColour (t.accent);
        g.strokePath (curve, PathStrokeType (2.0f));
        const float li = l2d (live.inPk), lo = l2d (live.outPk);
        if (li > -60.0f)
        {
            g.setColour (Colours::white);
            g.fillEllipse (X (jmin (x1, li)) - 4, Y (jlimit (y0, y1, lo)) - 4, 8, 8);
        }
    }
};

class FiltView : public CanvasView
{
public:
    FiltView (const Ctx& c, const Live& l) : CanvasView (c, l, 300, 100) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float f0 = 20, f1 = 20000;
        auto X = [=] (float f) { return 24.0f + std::log (f / f0) / std::log (f1 / f0) * (w - 30.0f); };
        auto Y = [=] (float d) { return h - 14.0f - (d + 36.0f) / (36.0f + 18.0f) * (h - 22.0f); };
        for (float f : { 20.0f, 100.0f, 1000.0f, 10000.0f })
        {
            g.setColour (t.line); g.drawLine (X (f), 4, X (f), h - 14, 1.0f);
            label (g, f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), X (f), h - 4, Justification::horizontallyCentred, t.dim);
        }
        for (float d : { -30.0f, -12.0f, 0.0f, 12.0f })
        {
            g.setColour (t.line); g.drawLine (24, Y (d), w - 6, Y (d), 1.0f);
            label (g, String ((int) d), 21, Y (d) + 3, Justification::right, t.dim);
        }
        for (int pass = 0; pass < 2; ++pass)
        {
            Path path;
            for (int i = 0; i <= 120; ++i)
            {
                const float f = f0 * std::pow (f1 / f0, (float) i / 120.0f);
                float d = pass == 0 ? purr::Compressor::detectorResponseDb (live.detHP, live.detBand, live.fs, f)
                                    : (live.audioHP ? purr::Compressor::audioHpResponseDb (live.fs, f) : 0.0f);
                d = jlimit (-36.0f, 18.0f, d);
                const Point<float> pt (X (f), Y (d));
                if (i == 0) path.startNewSubPath (pt); else path.lineTo (pt);
            }
            g.setColour (pass == 0 ? t.accent : theme::pink);
            if (pass == 0) g.strokePath (path, PathStrokeType (2.0f));
            else
            {
                Path dashed; const float d[2] = { 5.0f, 3.0f };
                PathStrokeType (1.6f).createDashedStroke (dashed, path, d, 2);
                g.fillPath (dashed);
            }
        }
    }
};

class HistView : public CanvasView
{
public:
    HistView (const Ctx& c, const Live& l) : CanvasView (c, l, 408, 100) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        auto Y = [=] (float d) { return 8.0f + d / 30.0f * (h - 22.0f); };
        for (int d = 0; d <= 30; d += 6)
        {
            g.setColour (t.line); g.drawLine (24, Y ((float) d), w - 4, Y ((float) d), 1.0f);
            label (g, "-" + String (d), 21, Y ((float) d) + 3, Justification::right, t.dim);
        }
        Path p;
        p.startNewSubPath (26, Y (0));
        for (int i = 0; i < 360; ++i)
            p.lineTo (26.0f + (float) i / 359.0f * (w - 30.0f), Y (jmin (30.0f, live.hist[(live.hi + i) % 360])));
        p.lineTo (w - 4, Y (0));
        p.closeSubPath();
        g.setColour (Colour::fromFloatRGBA (164 / 255.0f, 61 / 255.0f, 1.0f, 0.35f));
        g.fillPath (p);
        g.setColour (theme::selb);
        g.strokePath (p, PathStrokeType (1.6f));
        g.setFont (theme::font (theme::FontKind::mono, 11.0f));
        g.setColour (t.ink);
        g.drawSingleLineText ("GR " + String (live.grNow, 1) + " dB", 32, roundToInt (h - 6));
    }
};

// Input or output level strip, 26 x 300. Click to clear the clip marker.
class MeterView : public Component, public TipClient
{
public:
    MeterView (const Ctx& c) : ctx (c) {}
    float lin = 0.0f;
    bool latch = false;
    void update (float l)
    {
        lin = l;
        if (l2d (l) >= 0.0f) latch = true;
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float w = (float) getWidth(), h = (float) getHeight(), mn = -60, mx = 6;
        auto y = [=] (float db) { return h - 6.0f - (jlimit (mn, mx, db) - mn) / (mx - mn) * (h - 12.0f); };
        g.fillAll (t.disp);
        const float db = l2d (lin), top = y (db);
        auto f = [=] (float d) { return (d - mn) / (mx - mn); };
        ColourGradient grad (theme::blue, 0.0f, y (mn), theme::red, 0.0f, y (mx), false);
        grad.addColour (f (-12.0f), theme::blue);
        grad.addColour (f (-12.0f) + 0.001, theme::purple);
        grad.addColour (f (0.0f), theme::purple);
        grad.addColour (f (0.0f) + 0.001, theme::red);
        g.setGradientFill (grad);
        g.fillRect (2.0f, top, 12.0f, y (mn) - top);
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        for (float tk : { 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float yy = y (tk);
            g.setColour (t.dim);
            g.fillRect (14.0f, yy, 3.0f, 1.0f);
            g.drawSingleLineText (String ((int) tk), roundToInt (w - 1.0f), roundToInt (yy + 3.0f), Justification::right);
        }
        g.setColour (latch ? theme::red : Colour (0xff2a2c31));
        g.fillRect (2.0f, 0.0f, 12.0f, 4.0f);
    }
    void mouseDown (const MouseEvent&) override { latch = false; repaint(); }
private:
    const Ctx& ctx;
};

// The top plate: labels, LEDs and brackets. Click areas and buttons are child components.
class PlateView : public Component
{
public:
    PlateView (const Ctx& c, const Live& l) : ctx (c), live (l) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        g.addTransform (AffineTransform::translation (-layout::top.x, -layout::top.y));
        for (int i = 0; i < layout::numPlateTexts; ++i)
        {
            auto& x = layout::plateTexts[i];
            String s (x.s);
            if (x.upper) s = s.toUpperCase();
            const auto kind = x.ui ? (x.bold ? theme::FontKind::bold : theme::FontKind::regular) : theme::FontKind::mono;
            drawText (g, s, rc (x.r), theme::font (kind, x.size), x.title ? t.title : t.dim, Justification::centred, x.spacing);
        }
        g.setColour (t.dim);
        g.drawRoundedRectangle (rc (layout::bracket).reduced (0.5f), 10.0f, 1.0f);

        auto led = [&] (const layout::Led& l, bool on) { drawLed (g, l, on, t); };
        const float gr = live.grNow;
        static const float lad[16] = { 26, 23, 20, 17, 14, 12, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
        for (int i = 0; i < 16; ++i) led (layout::ladder[i], gr >= lad[i] - 0.25f);
        for (int i = 0; i < 8; ++i) led (layout::ratioLeds[i], live.ratio == i);
        led (layout::thdLeds[0], live.thd >= 0.01f);
        led (layout::thdLeds[1], live.redline);
        led (layout::detLeds[0], live.detHP);
        led (layout::detLeds[1], live.detBand);
        led (layout::detLeds[2], live.link);
        led (layout::audLeds[0], live.audioHP);
        led (layout::audLeds[1], live.dist == 1);
        led (layout::audLeds[2], live.dist == 2);
    }
private:
    static void drawLed (Graphics& g, const layout::Led& l, bool on, const theme::Look& t)
    {
        const Colour c = theme::ledColour (l.color, t);
        if (on)
        {
            ColourGradient glow (c.withAlpha (0.6f), l.cx, l.cy, c.withAlpha (0.0f), l.cx + 12.5f, l.cy, true);
            glow.addColour (6.0 / 12.5, c.withAlpha (0.55f));
            g.setGradientFill (glow);
            g.fillEllipse (l.cx - 12.5f, l.cy - 12.5f, 25.0f, 25.0f);
            g.setColour (c);
            g.fillEllipse (l.cx - 6.0f, l.cy - 6.0f, 12.0f, 12.0f);
            g.setColour (Colours::white.withAlpha (0.35f));
            g.drawEllipse (l.cx - 5.0f, l.cy - 5.0f, 10.0f, 10.0f, 1.0f);
        }
        else
        {
            g.setColour (Colour (0xff26282c));
            g.fillEllipse (l.cx - 6.0f, l.cy - 6.0f, 12.0f, 12.0f);
            g.setColour (Colours::black.withAlpha (0.55f));
            g.drawEllipse (l.cx - 5.5f, l.cy - 5.5f, 11.0f, 11.0f, 1.0f);
        }
    }
    const Ctx& ctx;
    const Live& live;
};
} // namespace ui
