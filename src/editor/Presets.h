#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Presets use the faceplate units of the mockup: knobs 0..10, Mix 0..100, ratio 0..7,
// dist 0..2, headroom 0..6, switches 0/1. Unknown keys are ignored, out of range values refuse the preset.
struct Preset
{
    juce::String name, type = "Bus";
    juce::StringArray tags;
    bool factory = false;
    double v[15] { 5, 5, 5, 5, 100, 4, 0, 0, 1, 0, 0, 3, 0, 0, 0 };
};

namespace presets
{
inline constexpr int kCount = 15;
inline const char* const keys[kCount] = { "input", "attack", "release", "output", "mix", "ratio", "detHP", "detBand",
                                          "link", "audioHP", "dist", "headroom", "bypass", "autoTrim", "levelMatch" };
inline const double lo[kCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
inline const double hi[kCount] = { 10, 10, 10, 10, 100, 7, 1, 1, 1, 1, 2, 6, 1, 1, 1 };
inline const char* const types[] = { "Vocal", "Bass", "Drum", "Keys", "Guitar", "Bus", "FX" };
inline constexpr int kNumTypes = 7;
inline const char* const ratioLabels[8] = { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "Nuke" };

const std::vector<Preset>& factory();
juce::File userFolder();

class Store
{
public:
    Store();
    void reload();                                    // read the user folder
    std::vector<Preset> all() const;                  // factory first, then yours
    const std::vector<Preset>& user() const { return mine; }
    const Preset* find (const juce::String& name) const;
    bool save (const Preset& p, const juce::String& replaceName = {}); // writes the file, replaces a same-named one
    bool remove (const juce::String& name);
    juce::String exportJson (const std::vector<Preset>& list) const;
    int importJson (const juce::String& text);        // returns count, or -1 when invalid
private:
    std::vector<Preset> mine;
};

bool valid (const juce::var& presetObject);
Preset fromVar (const juce::var& o);
juce::var toVar (const Preset& p);
} // namespace presets
