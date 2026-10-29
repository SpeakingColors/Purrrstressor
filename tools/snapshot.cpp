// Renders the real editor offscreen to a PNG so it can be compared with the mockup.
// usage: PurrrSnapshot out.png [scale] [look A|B|C] [dialog none|settings|guide|about|presets|save|export|import|tip] [frames]
//        PurrrSnapshot --selftest
#include "../src/plugin/PluginProcessor.h"
#include "../src/editor/PluginEditor.h"

// Checks that need the plugin classes but no window: knob mappings and saved state round trip.
static void setFixtureValues (PurrrstressorProcessor& a)
{
    auto set = [&a] (const char* id, float real) { auto* p = a.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (real)); };
    set ("input", 21.0f); set ("attack", 0.5f); set ("release", 900.0f); set ("output", -3.0f); set ("ratio", 7); set ("mix", 40.0f);
    set ("detHP", 1); set ("detBand", 1); set ("link", 0); set ("audioHP", 1); set ("dist", 2); set ("headroom", 6);
    set ("autoTrim", 1); set ("levelMatch", 1);
    a.setSetting (setting::look, "B"); a.setSetting (setting::detTap, 1); a.setSetting (setting::preset, "Vocal Bite");
}

// Saved state of state version 1 must keep loading (tests/fixtures/state-v1.xml).
static int fixtureTest (const juce::File& f)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    PurrrstressorProcessor p;
    auto xml = juce::XmlDocument::parse (f);
    check (xml != nullptr, "fixture parses");
    if (xml == nullptr) return 1;
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*xml, mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    auto real = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    check (std::abs (real ("input") - 21.0f) < 0.02f && std::abs (real ("attack") - 0.5f) < 0.01f && std::abs (real ("release") - 900.0f) < 1.0f
               && std::abs (real ("output") + 3.0f) < 0.02f, "knob values from the v1 fixture");
    check ((int) real ("ratio") == 7 && std::abs (real ("mix") - 40.0f) < 0.1f && real ("detHP") > 0.5f && real ("detBand") > 0.5f && real ("link") < 0.5f
               && real ("audioHP") > 0.5f && (int) real ("dist") == 2 && (int) real ("headroom") == 6 && real ("autoTrim") > 0.5f && real ("levelMatch") > 0.5f,
           "switch and choice values from the v1 fixture");
    check (p.getSetting (setting::look, "C").toString() == "B" && (int) p.getSetting (setting::detTap, 0) == 1 && p.getSetting (setting::preset, "").toString() == "Vocal Bite",
           "settings from the v1 fixture");
    return bad;
}

static int selfTest (const juce::File& fixture)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    PurrrstressorProcessor a, b;
    auto set = [] (PurrrstressorProcessor& p, const char* id, float real)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
    };
    auto real = [] (PurrrstressorProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto norm10 = [] (PurrrstressorProcessor& p, const char* id) { return 10.0f * p.apvts.getParameter (id)->getValue(); };
    // defaults: 6:1 and every knob at 5
    check (std::abs (norm10 (a, "input") - 5.0f) < 0.01f, "Input default is knob 5");
    check (std::abs (norm10 (a, "attack") - 5.0f) < 0.01f, "Attack default is knob 5");
    check (std::abs (norm10 (a, "release") - 5.0f) < 0.01f, "Release default is knob 5");
    check (std::abs (norm10 (a, "output") - 5.0f) < 0.01f, "Output default is knob 5");
    check ((int) real (a, "ratio") == 4, "Ratio default is 6:1");
    check (std::abs (real (a, "attack") - 1.2247f) < 0.01f, "Attack 5 is 1.22 ms");
    check (std::abs (real (a, "release") - 418.3f) < 1.0f, "Release 5 is 418 ms");
    a.apvts.getParameter ("attack")->setValueNotifyingHost (1.0f);
    check (std::abs (real (a, "attack") - 30.0f) < 0.001f, "Attack 10 is 30 ms");
    a.apvts.getParameter ("release")->setValueNotifyingHost (1.0f);
    check (std::abs (real (a, "release") - 3500.0f) < 0.1f, "Release 10 is 3.5 s");
    // every ratio, headroom and distortion item survives a set and read (last item included)
    bool all = true;
    for (int i = 0; i < 8; ++i) { set (a, "ratio", (float) i); all = all && (int) real (a, "ratio") == i; }
    check (all, "all 8 ratio items");
    all = true;
    for (int i = 0; i < 7; ++i) { set (a, "headroom", (float) i); all = all && (int) real (a, "headroom") == i; }
    check (all, "all 7 headroom items");
    all = true;
    for (int i = 0; i < 3; ++i) { set (a, "dist", (float) i); all = all && (int) real (a, "dist") == i; }
    check (all, "all 3 distortion items");
    // state round trip including plugin settings
    set (a, "input", 21.0f); set (a, "ratio", 7); set (a, "mix", 40.0f); set (a, "autoTrim", 1.0f); set (a, "headroom", 6);
    a.setSetting (setting::look, "B"); a.setSetting (setting::detTap, 1); a.setSetting (setting::preset, "Vocal Bite");
    juce::MemoryBlock mb; a.getStateInformation (mb);
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (real (b, "input") - 21.0f) < 0.02f && (int) real (b, "ratio") == 7 && std::abs (real (b, "mix") - 40.0f) < 0.1f
               && real (b, "autoTrim") > 0.5f && (int) real (b, "headroom") == 6, "parameters survive a state round trip");
    check (b.getSetting (setting::look, "C").toString() == "B" && (int) b.getSetting (setting::detTap, 0) == 1
               && b.getSetting (setting::preset, "").toString() == "Vocal Bite", "settings survive a state round trip");
    // an editor can be made, resized and deleted
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (b.createEditor());
        ed->setSize (600, 272);
        ed->setSize (1800, 816);
        check (ed->getWidth() == 1800, "editor creates and resizes");
    }
    if (fixture != juce::File()) bad += fixtureTest (fixture);
    std::printf (bad == 0 ? "ALL SELF TESTS PASSED\n" : "SELF TESTS FAILED\n");
    return bad;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--write-fixture")
    {
        PurrrstressorProcessor p; setFixtureValues (p);
        auto state = p.apvts.copyState(); state.setProperty ("stateVersion", 1, nullptr);
        if (auto xml = state.createXml()) juce::File (argv[2]).replaceWithText (xml->toString());
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--selftest")
        return selfTest (argc > 2 ? juce::File (argv[2]) : juce::File()) == 0 ? 0 : 1;
    PurrrstressorProcessor proc;
    proc.prepareToPlay (48000.0, 800);
    if (argc > 3) proc.setSetting (setting::look, juce::String (argv[3]));
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    const double sc = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 1.0;
    ed->setSize (juce::roundToInt (1200 * sc), juce::roundToInt (544 * sc));
    const juce::String dialog = argc > 4 ? juce::String (argv[4]) : juce::String ("none");
    const int frames = argc > 5 ? juce::String (argv[5]).getIntValue() : 30;
    // Feed a drum-and-pad loop in real time so the meters, curve dot and history show something.
    juce::AudioBuffer<float> buf (2, 800);
    juce::MidiBuffer midi;
    juce::Random rnd (7);
    long pos = 0;
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < 800; ++i, ++pos)
        {
            const double t = (double) pos / 48000.0, tk = std::fmod (t, 0.5), th = std::fmod (t + 0.25, 0.5);
            float v = (float) (0.85 * std::exp (-tk * 14.0) * std::sin (6.2831853 * (52.0 + 90.0 * std::exp (-tk * 40.0)) * tk));
            v += (float) (0.22 * std::exp (-th * 45.0) * (rnd.nextFloat() * 2.0f - 1.0f));
            v += 0.12f * (float) std::sin (6.2831853 * 220.0 * t) * (float) (0.6 + 0.4 * std::sin (6.2831853 * 0.25 * t));
            buf.setSample (0, i, v); buf.setSample (1, i, v * 0.97f);
        }
        proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
    }
    if (dialog != "none") { if (auto* pe = dynamic_cast<PurrrstressorEditor*> (ed.get())) pe->openDialogForTesting (dialog); }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    juce::Image img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    juce::File out (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
