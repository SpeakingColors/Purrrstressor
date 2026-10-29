#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"

class Face;

// Plugin window. The faceplate is a fixed 1200 x 544 drawing that is scaled to the window size;
// the window keeps its proportions and can be dragged from any edge or corner.
class PurrrstressorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PurrrstressorEditor (PurrrstressorProcessor&);
    ~PurrrstressorEditor() override;
    void resized() override;
    void resetSize();
    void openDialogForTesting (const juce::String& name); // used by tools/snapshot.cpp

    static constexpr int kW = 1200, kH = 544;

private:
    PurrrstressorProcessor& proc;
    std::unique_ptr<Face> face;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> border;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurrrstressorEditor)
};
