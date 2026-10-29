#include "PluginEditor.h"
#include "Widgets.h"
#include "Views.h"
#include "Dialogs.h"
#include "Presets.h"

using namespace juce;
using namespace ui;

namespace
{
// ---- the wood cheeks: same three colours and vertical grain as PurrOne ---------------------------
struct Mul32
{
    uint32_t a;
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a ^ (a >> 15);
        t *= (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

Image makeWood (uint32_t seed, bool edgeRight)
{
    const int w = 104, h = 1280;
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);
    Mul32 r { seed };
    g.fillAll (Colour (0xff3d2210));
    for (int x = 0; x < w; ++x)
    {
        const double a = r.next();
        g.setColour (Colour::fromFloatRGBA (123 / 255.0f, 74 / 255.0f, 38 / 255.0f, (float) (0.40 * (0.4 + 0.6 * a))));
        g.fillRect (x, 0, 1, h);
        if (r.next() < 0.35)
        {
            const float al = (float) (0.75 * r.next());
            const double y0 = r.next() * h;
            const double len = h * (0.2 + 0.6 * r.next());
            g.setColour (Colour::fromFloatRGBA (90 / 255.0f, 52 / 255.0f, 25 / 255.0f, al));
            g.fillRect ((float) x, (float) y0, 1.0f, (float) len);
        }
        if (r.next() < 0.12)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRect (x, 0, 1, h);
        }
    }
    g.setColour (Colours::black);
    g.fillRect (edgeRight ? 0 : w - 3, 0, 3, h);
    return img;
}

std::unique_ptr<Drawable> makeLogo()
{
    static const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><path d="M8 6 L24 22 H40 L56 6 L60 36 Q60 58 32 60 Q4 58 4 36 Z" fill="#FF7BB1"/><path d="M12 14 L21 23 H14 Z M52 14 L43 23 H50 Z" fill="#FBBDD0"/><ellipse cx="22" cy="36" rx="4" ry="5" fill="#16171a"/><ellipse cx="42" cy="36" rx="4" ry="5" fill="#16171a"/><path d="M29 44 H35 L32 48 Z" fill="#16171a"/><path d="M32 48 Q28 53 24 50 M32 48 Q36 53 40 50" stroke="#16171a" stroke-width="2" fill="none"/><path d="M2 40 H14 M2 46 H14 M62 40 H50 M62 46 H50" stroke="#FBBDD0" stroke-width="1.6"/></svg>)";
    if (auto xml = XmlDocument::parse (svg)) return Drawable::createFromSVG (*xml);
    return {};
}

class Cheek : public Component, public TipClient
{
public:
    Cheek (Image i, std::function<void()> reset) : img (std::move (i)), onReset (std::move (reset)) {}
    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onReset) onReset(); }
private:
    Image img;
    std::function<void()> onReset;
};

struct Delay : Timer
{
    std::function<void()> fn;
    void start (int ms) { startTimer (ms); }
    void timerCallback() override { stopTimer(); if (fn) fn(); }
};

class Bubble : public Component
{
public:
    Bubble (const Ctx& c) : ctx (c) { setInterceptsMouseClicks (false, false); setVisible (false); }
    void showAt (const String& s, Rectangle<int> target, int parentW, int parentH)
    {
        text = s;
        AttributedString as = build();
        TextLayout tl; tl.createLayout (as, 282.0f);
        const int tw = (int) std::ceil (std::min (282.0f, tl.getWidth())) + 20, th = (int) std::ceil (tl.getHeight()) + 14;
        int x = jlimit (60, std::max (60, parentW - tw - 8), target.getX());
        int y = target.getBottom() + 6;
        if (y + th > parentH - 4) y = target.getY() - th - 6;
        setBounds (x, y, tw, th);
        setVisible (true);
        toFront (false);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        build().draw (g, r.reduced (10.0f, 7.0f));
    }
private:
    AttributedString build() const
    {
        AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
        as.append (text, theme::font (theme::FontKind::regular, 14.0f), ctx.t().ink);
        return as;
    }
    const Ctx& ctx;
    String text;
};

class Toast : public Component
{
public:
    Toast (const Ctx&) { setInterceptsMouseClicks (false, false); setVisible (false); done.fn = [this] { setVisible (false); }; }
    void say (const String& s, int parentW, int parentH)
    {
        text = s;
        const int w = (int) textWidth (theme::font (theme::FontKind::regular, 15.0f), s) + 30;
        setBounds ((parentW - w) / 2, parentH - 34 - 28, w, 28);
        setVisible (true); toFront (false); repaint();
        done.start (1600);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::regular, 15.0f), Colours::white);
    }
private:
    String text;
    Delay done;
};

float knobToDb (double k) { return (float) (k * 3.0); }
float knobToAttack (double k) { return (float) (0.05 * std::pow (600.0, k / 10.0)); }
float knobToRelease (double k) { return (float) (50.0 * std::pow (70.0, k / 10.0)); }
float knobToOutput (double k) { return (float) ((k - 5.0) * 4.8); }
double attackToKnob (double ms) { return 10.0 * std::log (ms / 0.05) / std::log (600.0); }
double releaseToKnob (double ms) { return 10.0 * std::log (ms / 50.0) / std::log (70.0); }
} // namespace

// =====================================================================================================
class Face : public Component, public DialogHost, private Timer
{
public:
    Face (PurrrstressorProcessor& p, std::function<void()> resetSize)
        : proc (p), onResetSize (std::move (resetSize)), logo (makeLogo()),
          curve (ctx, live), filt (ctx, live), hist (ctx, live), plate (ctx, live),
          meterIn (ctx), meterOut (ctx), bubble (ctx), toastView (ctx), modal (*this)
    {
        const String lk = proc.getSetting (setting::look, "C").toString();
        ctx.look = lk == "A" ? 'A' : lk == "B" ? 'B' : 'C';
        ctx.tips = (bool) proc.getSetting (setting::tips, true);
        ctx.power = (bool) proc.getSetting (setting::power, false);
        setSize (layout::W, layout::H);
        setWantsKeyboardFocus (true);

        addAndMakeVisible (cheekL = new Cheek (makeWood (11, true), onResetSize));
        addAndMakeVisible (cheekR = new Cheek (makeWood (29, false), onResetSize));
        cheekL->setBounds (roundToInt (layout::cheekL.x), 0, roundToInt (layout::cheekL.w), layout::H);
        cheekR->setBounds (roundToInt (layout::cheekR.x), 0, roundToInt (layout::cheekR.w), layout::H);
        cheekL->tip = cheekR->tip = "Double-click the wood to reset the window to 100%.";

        buildHeader();
        buildPlate();
        buildDisplays();
        buildKnobs();
        buildMeters();

        addAndMakeVisible (bubble);
        addAndMakeVisible (toastView);
        addChildComponent (modal);
        addMouseListener (this, true);
        tipTimer.fn = [this] { showTip(); };
        syncName();
        refreshLive (1.0f / 60.0f);
        startTimerHz (ctx.power ? 16 : 60);
    }
    ~Face() override { stopTimer(); removeMouseListener (this); }
    void openForTesting (const String& n)
    {
        if (n == "settings") modal.openSettings();
        else if (n == "guide") modal.openGuide();
        else if (n == "about") modal.openAbout();
        else if (n == "presets") modal.openBrowser();
        else if (n == "save") modal.openSave (nullptr);
        else if (n == "export") modal.openExport (nullptr);
        else if (n == "import") modal.openImport();
        else if (n == "tip") { tipHost = &meterIn; tipText = "Info bubble check."; showTip(); }
    }

    // ---- DialogHost --------------------------------------------------------------------------
    PurrrstressorProcessor& processor() override { return proc; }
    presets::Store& store() override { return presetStore; }
    Ctx& context() override { return ctx; }
    String presetName() override { return proc.getSetting (setting::preset, presets::factory()[0].name).toString(); }
    void setPresetName (const String& n) override { proc.setSetting (setting::preset, n); syncName(); }
    void toast (const String& s) override { toastView.say (s, getWidth(), getHeight()); }
    void resetWindow() override { if (onResetSize) onResetSize(); }
    void settingsChanged() override
    {
        startTimerHz (ctx.power ? 16 : 60);
        repaint();
        for (auto* c : getChildren()) c->repaint();
    }

    Preset currentValues() override
    {
        Preset p;
        p.v[0] = real (ids::input) / 3.0;
        p.v[1] = attackToKnob (real (ids::attack));
        p.v[2] = releaseToKnob (real (ids::release));
        p.v[3] = real (ids::output) / 4.8 + 5.0;
        p.v[4] = real (ids::mix);
        p.v[5] = real (ids::ratio);
        p.v[6] = real (ids::detHP); p.v[7] = real (ids::detBand); p.v[8] = real (ids::link); p.v[9] = real (ids::audioHP);
        p.v[10] = real (ids::dist); p.v[11] = real (ids::headroom);
        p.v[12] = real (ids::bypass); p.v[13] = real (ids::autoTrim); p.v[14] = real (ids::levelMatch);
        for (int i = 0; i < 5; ++i) p.v[i] = std::round (p.v[i] * 1000.0) / 1000.0;
        return p;
    }

    void loadPreset (const Preset& p) override
    {
        setReal (ids::input, knobToDb (p.v[0]));
        setReal (ids::attack, knobToAttack (p.v[1]));
        setReal (ids::release, knobToRelease (p.v[2]));
        setReal (ids::output, knobToOutput (p.v[3]));
        setReal (ids::mix, (float) p.v[4]);
        setReal (ids::ratio, (float) p.v[5]);
        setReal (ids::detHP, (float) p.v[6]); setReal (ids::detBand, (float) p.v[7]);
        setReal (ids::link, (float) p.v[8]); setReal (ids::audioHP, (float) p.v[9]);
        setReal (ids::dist, (float) p.v[10]); setReal (ids::headroom, (float) p.v[11]);
        setReal (ids::autoTrim, (float) p.v[13]); setReal (ids::levelMatch, (float) p.v[14]);
        proc.resetAutoTrim();
        setPresetName (p.name);
        toast ("Loaded " + p.name);
        refreshLive (1.0f / 60.0f);
    }

    bool dirty() override
    {
        const Preset* base = presetStore.find (presetName());
        if (base == nullptr) base = &presets::factory()[0];
        const Preset now = currentValues();
        for (int i = 0; i < presets::kCount; ++i)
        {
            if (i == 12) continue; // bypass is not part of a preset
            if (std::abs (now.v[i] - base->v[i]) > (i == 4 ? 0.2 : i < 4 ? 0.02 : 0.01)) return true;
        }
        return false;
    }

    // ---- painting ----------------------------------------------------------------------------
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        g.fillAll (Colours::black);
        g.setColour (t.plate);
        g.fillRect (rc (layout::body).withTop (0).withHeight ((float) layout::H));
        // header
        g.setColour (t.plate2); g.fillRect (rc (layout::hdr));
        g.setColour (t.line); g.fillRect (layout::hdr.x, layout::hdr.h - 1.0f, layout::hdr.w, 1.0f);
        if (logo) logo->drawWithin (g, rc (layout::logo), RectanglePlacement::centred, 1.0f);
        drawText (g, "Purrrstressor", rc (layout::name), theme::font (theme::FontKind::brand, 26.0f), Colours::white, Justification::centred, 0.5f);
        drawText (g, "STUDIO 87", rc (layout::s87), theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centred, 2.0f);
        // top plate and displays
        g.setColour (t.line); g.fillRect (layout::top.x, layout::top.y + layout::top.h - 1.0f, layout::top.w, 1.0f);
        g.setColour (t.plate2); g.fillRect (rc (layout::disp));
        g.setColour (t.line); g.fillRect (layout::disp.x, layout::disp.y + layout::disp.h - 1.0f, layout::disp.w, 1.0f);
        for (int i = 0; i < 3; ++i)
            drawText (g, String (layout::dispCapText[i]).toUpperCase(), rc (layout::dispCaps[i]), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centredLeft, 1.5f);
        // knob labels
        for (int i = 0; i < 6; ++i)
            drawText (g, String (i == 0 ? "Input" : i == 1 ? "Attack" : i == 2 ? "Release" : i == 3 ? "Output" : i == 4 ? "Headroom" : "Mix").toUpperCase(),
                      rc (layout::cells[i].label), theme::font (theme::FontKind::bold, 15.0f), t.title, Justification::centred, i == 4 ? 2.0f : 3.0f);
        // meter column
        g.setColour (t.plate2); g.fillRect (rc (layout::meters));
        g.setColour (t.line); g.fillRect (layout::meters.x, layout::meters.y, 1.0f, layout::meters.h);
        drawText (g, "IN", rc (layout::meterCaps[0]), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centred, 1.5f);
        drawText (g, "OUT", rc (layout::meterCaps[1]), theme::font (theme::FontKind::regular, 12.0f), t.title, Justification::centred, 1.5f);
        auto pk = [] (float lin) { return lin > 1.0e-4f ? String (l2d (lin), 1) : String ("-inf"); };
        {   // the two readouts sit side by side with a 6 px gap, centred under the strips
            const auto f = theme::font (theme::FontKind::mono, 11.0f);
            const String a = pk (live.inPk), b = pk (live.outPk);
            const float wa = textWidth (f, a), wb = textWidth (f, b);
            const float cx = (layout::pkIn.x + layout::pkOut.x + layout::pkOut.w) * 0.5f;
            const float x0 = cx - (wa + 6.0f + wb) * 0.5f;
            drawText (g, a, { x0, layout::pkIn.y, wa, layout::pkIn.h }, f, t.dim);
            drawText (g, b, { x0 + wa + 6.0f, layout::pkIn.y, wb, layout::pkIn.h }, f, t.dim);
        }
        // footer
        g.setColour (t.plate2); g.fillRect (rc (layout::foot));
        g.setColour (t.line); g.fillRect (layout::foot.x, layout::foot.y, layout::foot.w, 1.0f);
        drawText (g, String ("Studio 87 ") + String::fromUTF8 ("\xc2\xb7") + " Purrrstressor v" + JucePlugin_VersionString,
                  { layout::foot.x + 12.0f, layout::foot.y, 400.0f, layout::foot.h }, theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredLeft, 1.0f);
        if (auto* ed = findParentComponentOfClass<PurrrstressorEditor>())
            drawText (g, String (ed->getWidth()) + String::fromUTF8 (" \xc3\x97 ") + String (ed->getHeight()) + " px",
                      { layout::foot.x + layout::foot.w - 12.0f - 120.0f, layout::foot.y, 120.0f, layout::foot.h },
                      theme::font (theme::FontKind::regular, 12.0f), t.dim, Justification::centredRight, 1.0f);
    }

    // ---- keyboard ----------------------------------------------------------------------------
    bool keyPressed (const KeyPress& k) override
    {
        if (modal.isOpen() || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown() || k.getModifiers().isAltDown()) return false;
        const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
        const bool shift = k.getModifiers().isShiftDown();
        if (c == 'b') toggle (ids::bypass);
        else if (c == 'r') cycleRatio (shift);
        else if (c == 'd') cycleDetector (false);
        else if (c == 'a') cycleAudio (false);
        else return false;
        return true;
    }

    // ---- info bubbles ------------------------------------------------------------------------
    void mouseMove (const MouseEvent& e) override { trackTip (e); }
    void mouseEnter (const MouseEvent& e) override { trackTip (e); }
    void mouseExit (const MouseEvent&) override { hideTip(); }
    void mouseDown (const MouseEvent& e) override
    {
        hideTip();
        if (dynamic_cast<TextEditor*> (e.eventComponent) == nullptr && ! modal.isOpen()) grabKeyboardFocus();
    }

private:
    void timerCallback() override
    {
        refreshLive (1.0f / (float) (ctx.power ? 16 : 60));
        hist.repaint(); curve.repaint(); filt.repaint();
        meterIn.update (live.inPk); meterOut.update (live.outPk);
        plate.repaint();
        repaint (layout::pkIn.x > 0 ? Rectangle<int> (1056, 370, 92, 24) : Rectangle<int>());
        syncControls();
    }

    float real (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setReal (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    void toggle (const char* id) { setReal (id, real (id) > 0.5f ? 0.0f : 1.0f); syncControls(); }

    void refreshLive (float dt)
    {
        live.fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        const float dec = std::exp (-dt / 0.1f);
        live.inPk = std::max (proc.inPeak.exchange (0.0f), live.inPk * dec);
        live.outPk = std::max (proc.outPeak.exchange (0.0f), live.outPk * dec);
        live.grNow = proc.grMeterDb.load();
        live.hist[live.hi] = proc.grPeakDb.exchange (0.0f);
        live.hi = (live.hi + 1) % 360;
        live.thd = proc.thdRatio.load(); live.redline = proc.redline.load() || live.outPk >= 1.0f;
        live.trimDb = proc.trimDb.load(); live.matchDb = proc.matchDb.load();
        live.inputDb = real (ids::input); live.outputDb = real (ids::output);
        live.ratio = (int) real (ids::ratio); live.dist = (int) real (ids::dist); live.headroomIdx = (int) real (ids::headroom);
        live.bypass = real (ids::bypass) > 0.5f; live.detHP = real (ids::detHP) > 0.5f; live.detBand = real (ids::detBand) > 0.5f;
        live.link = real (ids::link) > 0.5f; live.audioHP = real (ids::audioHP) > 0.5f;
        live.autoTrim = real (ids::autoTrim) > 0.5f; live.levelMatch = real (ids::levelMatch) > 0.5f;
    }

    String dot() const { return String::fromUTF8 ("\xc2\xb7"); }

    void syncName() { pName->setText (presetName() + (dirty() ? " *" : "")); }

    void syncControls()
    {
        bBypass->setOn (live.bypass);
        hrButton->setText (String (4 + 4 * live.headroomIdx) + " dB");
        // value rows
        const double kIn = real (ids::input) / 3.0, kAt = attackToKnob (real (ids::attack)), kRe = releaseToKnob (real (ids::release)),
                     kOut = real (ids::output) / 4.8 + 5.0;
        values[0]->setParts (String (kIn, 1), dot() + " +" + String (kIn * 3.0, 1) + " dB");
        const double am = real (ids::attack), rm = real (ids::release), od = (kOut - 5.0) * 4.8;
        values[1]->setParts (String (kAt, 1), dot() + " " + (am < 1.0 ? String (roundToInt (am * 1000.0)) + " us" : String (am, 1) + " ms"));
        values[2]->setParts (String (kRe, 1), dot() + " " + (rm < 1000.0 ? String (roundToInt (rm)) + " ms" : String (rm / 1000.0, 2) + " s"));
        values[3]->setParts (String (kOut, 1), dot() + " " + (od >= 0 ? "+" : "") + String (od, 1) + " dB");
        values[4]->setParts (String (roundToInt (real (ids::mix))) + "%", "comp");
        trimBtn->setText (String ("Auto Trim\n") + (live.autoTrim ? (trimActive() ? String (live.trimDb, 1) + " dB" : String ("ON, armed")) : String ("off")));
        trimBtn->setOn (live.autoTrim);
        trimBtn->hot = trimActive();
        matchBtn->setText (String ("Level Match\n") + (live.levelMatch ? String (live.matchDb >= 0 ? "+" : "") + String (live.matchDb, 1) + " dB" : String ("off")));
        matchBtn->setOn (live.levelMatch);
        const bool d = dirty();
        const String shown = presetName() + (d ? " *" : "");
        pName->setText (shown);
    }
    bool trimActive() const { return live.autoTrim && live.trimDb < -0.05f; }

    // ---- cycles ------------------------------------------------------------------------------
    void cycleRatio (bool back) { setReal (ids::ratio, (float) (((int) real (ids::ratio) + (back ? 7 : 1)) % 8)); syncControls(); }
    void cycleDetector (bool back)
    {
        int i = (real (ids::detBand) > 0.5f ? 2 : 0) + (real (ids::detHP) > 0.5f ? 1 : 0) + (real (ids::link) > 0.5f ? 4 : 0);
        i = (i + (back ? 7 : 1)) % 8;
        setReal (ids::detHP, (float) (i & 1)); setReal (ids::detBand, (float) ((i >> 1) & 1)); setReal (ids::link, (float) ((i >> 2) & 1));
    }
    void cycleAudio (bool back)
    {
        static const int seq[6][2] = { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 2, 0 }, { 2, 1 } };
        const int d = (int) real (ids::dist), a = real (ids::audioHP) > 0.5f ? 1 : 0;
        int i = 0;
        for (int k = 0; k < 6; ++k) if (seq[k][0] == d && seq[k][1] == a) i = k;
        i = (i + (back ? 5 : 1)) % 6;
        setReal (ids::dist, (float) seq[i][0]); setReal (ids::audioHP, (float) seq[i][1]);
    }

    // ---- building ----------------------------------------------------------------------------
    PButton* addButton (Component& parent, const String& text, const layout::Rc& r, const String& tip, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
    {
        auto* b = new PButton (ctx, text, px, k);
        owned.add (b);
        parent.addAndMakeVisible (b);
        b->setBounds (roundToInt (r.x), roundToInt (r.y), roundToInt (r.w), roundToInt (r.h));
        b->tip = tip;
        return b;
    }

    void buildHeader()
    {
        pPrev = addButton (*this, String::fromUTF8 ("\xe2\x97\x80"), layout::pPrev, "Previous preset (arrow keys while the name is focused).");
        pName = addButton (*this, presetName(), layout::pName, "Open the preset browser. Double-click to rename.", 16.0f, theme::FontKind::semi);
        pNext = addButton (*this, String::fromUTF8 ("\xe2\x96\xb6"), layout::pNext, "Next preset.");
        pSave = addButton (*this, "Save", layout::pSave, "Save these settings as your own preset. Asks before overwriting.");
        bBypass = addButton (*this, "Bypass", layout::bBypass, "Bypass: hear the dry signal to compare with the processed sound (B). The meters keep moving.");
        bGuide = addButton (*this, "Guide", layout::bGuide, "Short guide and keyboard shortcuts.");
        bSettings = addButton (*this, "Settings", layout::bSettings, "Info bubbles, power saving, colour look.");
        bAbout = addButton (*this, "About & Support", layout::bAbout, "About Purrrstressor, support and where your files live.");
        pPrev->onClick = [this] (const ModifierKeys&) { step (-1); };
        pNext->onClick = [this] (const ModifierKeys&) { step (1); };
        pSave->onClick = [this] (const ModifierKeys&) { modal.openSave (nullptr); };
        bBypass->onClick = [this] (const ModifierKeys&) { toggle (ids::bypass); };
        bGuide->onClick = [this] (const ModifierKeys&) { modal.openGuide(); };
        bSettings->onClick = [this] (const ModifierKeys&) { modal.openSettings(); };
        bAbout->onClick = [this] (const ModifierKeys&) { modal.openAbout(); };
        pName->onClick = [this] (const ModifierKeys&) { nameDelay.start (260); };
        nameDelay.fn = [this] { modal.openBrowser(); };
        pName->onDouble = [this]
        {
            nameDelay.stopTimer();
            const Preset* p = presetStore.find (presetName());
            if (p == nullptr || p->factory) { toast ("Save a copy first, factory presets cannot be renamed"); return; }
            Preset q = *p; modal.openSave (&q);
        };
    }

    void step (int d)
    {
        auto all = presetStore.all();
        int i = 0;
        for (size_t k = 0; k < all.size(); ++k) if (all[k].name == presetName()) i = (int) k;
        i = (i + d + (int) all.size()) % (int) all.size();
        loadPreset (all[(size_t) i]);
    }

    void buildPlate()
    {
        plate.setBounds (roundToInt (layout::top.x), roundToInt (layout::top.y), roundToInt (layout::top.w), roundToInt (layout::top.h));
        addAndMakeVisible (plate);
        const auto place = [] (Component& c, const layout::Rc& r)
        {
            c.setBounds (roundToInt (r.x - layout::top.x), roundToInt (r.y - layout::top.y), roundToInt (r.w), roundToInt (r.h));
        };
        auto hit = [&] (const layout::Hit& h, std::function<void (const ModifierKeys&)> fn)
        {
            auto* a = new HitArea(); owned.add (a);
            plate.addAndMakeVisible (a);
            place (*a, h.r); a->tip = h.tip; a->onClick = std::move (fn);
        };
        for (int i = 0; i < 8; ++i) hit (layout::ratioHits[i], [this, i] (const ModifierKeys&) { setReal (ids::ratio, (float) i); });
        hit (layout::detHits[0], [this] (const ModifierKeys&) { toggle (ids::detHP); });
        hit (layout::detHits[1], [this] (const ModifierKeys&) { toggle (ids::detBand); });
        hit (layout::detHits[2], [this] (const ModifierKeys&) { toggle (ids::link); });
        hit (layout::audHits[0], [this] (const ModifierKeys&) { toggle (ids::audioHP); });
        hit (layout::audHits[1], [this] (const ModifierKeys&) { setReal (ids::dist, (int) real (ids::dist) == 1 ? 0.0f : 1.0f); });
        hit (layout::audHits[2], [this] (const ModifierKeys&) { setReal (ids::dist, (int) real (ids::dist) == 2 ? 0.0f : 2.0f); });
        for (int i = 0; i < 3; ++i)
        {
            auto* b = new PlateButton(); owned.add (b);
            plate.addAndMakeVisible (b);
            place (*b, layout::plateBtns[i].r); b->tip = layout::plateBtns[i].tip;
            b->onClick = [this, i] (const ModifierKeys& m)
            {
                if (i == 0) cycleRatio (m.isShiftDown());
                else if (i == 1) cycleDetector (m.isShiftDown());
                else cycleAudio (m.isShiftDown());
            };
        }
        // indicators, not buttons: tips only
        auto ind = [&] (const layout::Led& l, const char* tip)
        {
            auto* a = new HitArea(); owned.add (a);
            plate.addAndMakeVisible (a);
            a->setBounds (roundToInt (l.cx - 40 - layout::top.x), roundToInt (l.cy - 12 - layout::top.y), 100, 24);
            a->tip = tip; a->setInterceptsMouseClicks (false, false);
            a->setMouseCursor (MouseCursor::NormalCursor);
        };
        ind (layout::thdLeds[0], layout::thdTips[0]);
        ind (layout::thdLeds[1], layout::thdTips[1]);
    }

    void buildDisplays()
    {
        CanvasView* views[3] = { &curve, &filt, &hist };
        const layout::Rc* rects[3] = { &layout::cvCurve, &layout::cvFilt, &layout::cvHist };
        const char* tips[3] = {
            "Input level (across) against output level (up). The curve follows Ratio, Input and Output. The dot is the live signal.",
            "Solid line: what the detector listens to (high-pass and band emphasis). Dashed pink: the audio high-pass.",
            "How much the compressor turned the signal down, over the last six seconds." };
        for (int i = 0; i < 3; ++i)
        {
            addAndMakeVisible (*views[i]);
            views[i]->setBounds (roundToInt (rects[i]->x), roundToInt (rects[i]->y), roundToInt (rects[i]->w), roundToInt (rects[i]->h));
            views[i]->tip = tips[i];
        }
    }

    void buildKnobs()
    {
        struct K { const char* id; const char* name; const char* tip; };
        const K ks[5] = {
            { ids::input, "Input", "Input gain and threshold together. Drive harder for more compression and more colour. 0 to +30 dB." },
            { ids::attack, "Attack", "How fast compression grabs. 0 is fastest (50 microseconds), 10 is slowest (30 ms). Slow it down to keep drum punch." },
            { ids::release, "Release", "How long compression lets go. 0 is fastest (50 ms), 10 is slowest (3.5 s). In 10:1 Opto it can stretch to about 20 s." },
            { ids::output, "Output", "Output level after the compressor. 5 is 0 dB, range -24 to +24 dB." },
            { ids::mix, "Mix", "Blend of the dry signal and the processed signal for parallel compression. Dry on the left, Comp (all processed) on the right." } };
        const int cell[5] = { 0, 1, 2, 3, 5 };
        for (int i = 0; i < 5; ++i)
        {
            auto* k = new KnobView (*proc.apvts.getParameter (ks[i].id), ks[i].name, i == 4);
            knobs.add (k); addAndMakeVisible (k);
            const auto& c = layout::cells[cell[i]];
            k->setBounds (roundToInt (c.knob.x), roundToInt (c.knob.y), 150, 150);
            k->tip = ks[i].tip;
            auto* v = new KnobValue (ctx);
            values.add (v); addAndMakeVisible (v);
            v->setBounds (roundToInt (c.value.x), roundToInt (c.value.y), roundToInt (c.value.w), roundToInt (c.value.h));
            v->tip = ks[i].tip;
            if (i == 4)
            {
                v->mixCell = true;
                v->setBounds (roundToInt (c.value.x + c.value.w * 0.5f - 75), roundToInt (c.value.y), 150, roundToInt (c.value.h));
                v->dryTip = "Click DRY to set the knob fully dry.";
                v->compTip = "Click COMP to set the knob fully processed.";
                v->tip = {};
                auto* kk = k;
                v->onEnd = [kk] (float pct) { kk->setReal (pct); };
            }
        }
        hrButton = addButton (*this, "16 dB", layout::hrButton, "Headroom (HR), as in the hardware manual: higher dB pushes the circuit into more processing and colour, lower dB keeps it cleaner. 4 to 28 dB, 16 is the default. Only matters with Dist 2 or Dist 3. Click to step up, shift-click to step down, double-click to reset.", 18.0f, theme::FontKind::mono);
        hrButton->onClick = [this] (const ModifierKeys& m) { setReal (ids::headroom, (float) (((int) real (ids::headroom) + (m.isShiftDown() ? 6 : 1)) % 7)); syncControls(); };
        hrButton->onDouble = [this] { setReal (ids::headroom, 3.0f); syncControls(); };
        auto* note = new KnobValue (ctx);
        values.add (note); addAndMakeVisible (note);
        const auto& hc = layout::cells[4];
        note->setBounds (roundToInt (hc.value.x + hc.value.w * 0.5f - 90), roundToInt (hc.value.y), 180, roundToInt (hc.value.h));
        note->setParts (String::fromUTF8 ("less colour \xe2\x86\x90 \xe2\x86\x92 more"), {});
        note->plainFont = true;
        note->setInterceptsMouseClicks (false, false);
    }

    void buildMeters()
    {
        addAndMakeVisible (meterIn); addAndMakeVisible (meterOut);
        meterIn.setBounds (roundToInt (layout::cvIn.x), roundToInt (layout::cvIn.y), roundToInt (layout::cvIn.w), roundToInt (layout::cvIn.h));
        meterOut.setBounds (roundToInt (layout::cvOut.x), roundToInt (layout::cvOut.y), roundToInt (layout::cvOut.w), roundToInt (layout::cvOut.h));
        meterIn.tip = meterOut.tip = "Click to clear the clip marker.";
        trimBtn = addButton (*this, "Auto Trim\noff", layout::trimBtn, "Auto Trim: if the output ever reaches 0 dB, the output is turned down by that amount and stays there. It never turns back up by itself. Each time the output reaches 0 dB it turns down by that overshoot plus 0.1 dB. The readout turns pink once it has trimmed. Click twice (off, on) to reset. Off by default.", 12.0f);
        trimBtn->spacing = 0.0f;
        trimBtn->onClick = [this] (const ModifierKeys&) { toggle (ids::autoTrim); proc.resetAutoTrim(); };
        matchBtn = addButton (*this, "Level Match\noff", layout::matchBtn, "Level Match: slowly adjusts the output so the processed sound is as loud as the dry sound, for a fair comparison (louder always sounds better). It overrides the Output knob while on. Turn it off when you are done comparing. Off by default.", 12.0f);
        matchBtn->spacing = 0.0f;
        matchBtn->onClick = [this] (const ModifierKeys&) { toggle (ids::levelMatch); };
    }

    // ---- tips --------------------------------------------------------------------------------
    void trackTip (const MouseEvent& e)
    {
        if (modal.isOpen() || ! ctx.tips) { hideTip(); return; }
        Component* c = e.eventComponent;
        String text; Component* host = nullptr;
        while (c != nullptr && c != this)
        {
            if (auto* kv = dynamic_cast<KnobValue*> (c))
            {
                text = kv->tipAt (e.getEventRelativeTo (c).getPosition());
                if (text.isEmpty()) text = kv->tip;
                host = c; if (text.isNotEmpty()) break;
            }
            else if (auto* tc = dynamic_cast<TipClient*> (c); tc != nullptr && tc->tip.isNotEmpty())
            {
                text = tc->tip; host = c; break;
            }
            c = c->getParentComponent();
        }
        if (host == nullptr || text.isEmpty()) { hideTip(); return; }
        if (host == tipHost && text == tipText) return;
        hideTip();
        tipHost = host; tipText = text;
        tipTimer.start (1000);
    }
    void showTip()
    {
        if (tipHost == nullptr || modal.isOpen()) return;
        bubble.showAt (tipText, getLocalArea (tipHost, tipHost->getLocalBounds()), getWidth(), getHeight());
    }
    void hideTip() { tipTimer.stopTimer(); bubble.setVisible (false); tipHost = nullptr; tipText = {}; }

    // ---- members -----------------------------------------------------------------------------
    PurrrstressorProcessor& proc;
    std::function<void()> onResetSize;
    Ctx ctx;
    Live live;
    presets::Store presetStore;
    std::unique_ptr<Drawable> logo;
    Cheek* cheekL = nullptr; Cheek* cheekR = nullptr;
    OwnedArray<Component> owned;
    PButton *pPrev = nullptr, *pName = nullptr, *pNext = nullptr, *pSave = nullptr, *bBypass = nullptr, *bGuide = nullptr, *bSettings = nullptr, *bAbout = nullptr;
    PButton *hrButton = nullptr, *trimBtn = nullptr, *matchBtn = nullptr;
    CurveView curve; FiltView filt; HistView hist;
    PlateView plate;
    OwnedArray<KnobView> knobs;
    OwnedArray<KnobValue> values;
    MeterView meterIn, meterOut;
    Bubble bubble; Toast toastView;
    Modal modal;
    Delay nameDelay, tipTimer;
    Component* tipHost = nullptr;
    String tipText;
};

// =====================================================================================================
PurrrstressorEditor::PurrrstressorEditor (PurrrstressorProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    face = std::make_unique<Face> (p, [this] { resetSize(); });
    addAndMakeVisible (*face);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    constrainer.setMinimumSize (480, 218);
    constrainer.setMaximumSize (1800, 817);
    setConstrainer (&constrainer);
    setResizable (true, false);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer);
    border->setBorderThickness (BorderSize<int> (5));
    addAndMakeVisible (*border);
    const double sc = jlimit (0.4, 1.5, (double) proc.getSetting (setting::scale, 1.0));
    setSize (roundToInt (kW * sc), roundToInt (kH * sc));
}

PurrrstressorEditor::~PurrrstressorEditor() = default;

void PurrrstressorEditor::resized()
{
    const double s = (double) getWidth() / (double) kW;
    face->setTransform (AffineTransform::scale ((float) s));
    face->setBounds (0, 0, kW, kH);
    border->setBounds (getLocalBounds());
    proc.setSetting (setting::scale, s);
}

void PurrrstressorEditor::resetSize() { setSize (kW, kH); }
void PurrrstressorEditor::openDialogForTesting (const juce::String& n) { face->openForTesting (n); }
