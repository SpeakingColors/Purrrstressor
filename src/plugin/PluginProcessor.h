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
    std::atomic<float> grMeterDb { 0.0f }; // for the editor's GR meter

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    purr::Params readParams() const;

    purr::Compressor core;
    std::atomic<float>* pInput; std::atomic<float>* pAttack; std::atomic<float>* pRelease;
    std::atomic<float>* pOutput; std::atomic<float>* pRatio; std::atomic<float>* pDetHP;
    std::atomic<float>* pDetBand; std::atomic<float>* pLink; std::atomic<float>* pAudioHP;
    std::atomic<float>* pDist; std::atomic<float>* pHeadroom; std::atomic<float>* pMix;
    std::atomic<float>* pBypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PurrrstressorProcessor)
};
