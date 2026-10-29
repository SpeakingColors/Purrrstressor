#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/Purrrstressor.h"
#include <atomic>

// Stable parameter IDs. Never rename these (saved projects use them).
namespace ids
{
    inline constexpr const char* input = "input";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* output = "output";
    inline constexpr const char* ratio = "ratio";
    inline constexpr const char* detHP = "detHP";
    inline constexpr const char* detBand = "detBand";
    inline constexpr const char* link = "link";
    inline constexpr const char* audioHP = "audioHP";
    inline constexpr const char* dist = "dist";
    inline constexpr const char* headroom = "headroom";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* autoTrim = "autoTrim";
    inline constexpr const char* levelMatch = "levelMatch";
}

// Plugin-wide settings kept in the saved state (not presets, not automatable).
namespace setting
{
    inline constexpr const char* detTap = "detTap";   // 0 input (stable), 1 after distortion
    inline constexpr const char* look = "look";       // "A", "B", "C"
    inline constexpr const char* tips = "tips";       // info bubbles
    inline constexpr const char* power = "power";     // power saving redraw
    inline constexpr const char* scale = "scale";     // window scale, 1.0 = 100 %
    inline constexpr const char* preset = "preset";   // name of the loaded preset
}

class PurrrstressorProcessor : public juce::AudioProcessor
{
public:
    PurrrstressorProcessor();
    ~PurrrstressorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Purrrstressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter data for the editor (written on the audio thread, read by the editor timer).
    std::atomic<float> grMeterDb { 0.0f };   // gain reduction, positive dB (active in bypass too, like the hardware)
    std::atomic<float> grPeakDb { 0.0f };    // largest gain reduction since the editor last read it (exchange to 0)
    std::atomic<float> inPeak { 0.0f };      // linear peak since the editor last read it (exchange to 0), input
    std::atomic<float> outPeak { 0.0f };     // same for the output
    std::atomic<float> thdRatio { 0.0f };    // 0.01 = 1 %
    std::atomic<bool>  redline { false };
    std::atomic<float> trimDb { 0.0f };      // Auto Trim, 0 or negative
    std::atomic<float> matchDb { 0.0f };     // Level Match gain

    // Settings stored in the state tree.
    juce::var getSetting (const char* key, const juce::var& fallback) const;
    void setSetting (const char* key, const juce::var& v);
    void resetAutoTrim() { resetTrimRequested.store (true); }
    static void raiseTo (std::atomic<float>& a, float v) { float c = a.load(); while (v > c && ! a.compare_exchange_weak (c, v)) {} }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    purr::Params readParams() const;

    purr::Compressor core;
    juce::AudioBuffer<float> dryCopy;
    float bypassMix = 0.0f; // 0 processed, 1 dry (ramped over about 10 ms)
    std::atomic<bool> resetTrimRequested { false };
    int maxChunk = 512;
    std::atomic<int> detTapRt { 0 }; // audio-thread copy of the detector tap setting
    std::atomic<float>* pInput; std::atomic<float>* pAttack; std::atomic<float>* pRelease;
    std::atomic<float>* pOutput; std::atomic<float>* pRatio; std::atomic<float>* pDetHP;
    std::atomic<float>* pDetBand; std::atomic<float>* pLink; std::atomic<float>* pAudioHP;
    std::atomic<float>* pDist; std::atomic<float>* pHeadroom; std::atomic<float>* pMix;
    std::atomic<float>* pBypass; std::atomic<float>* pAutoTrim; std::atomic<float>* pLevelMatch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurrrstressorProcessor)
};
