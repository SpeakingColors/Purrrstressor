#include "PluginProcessor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout PurrrstressorProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const int v = 1; // parameter version

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, v }, "Input",
        NormalisableRange<float> (0.0f, 30.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::attack, v }, "Attack",
        NormalisableRange<float> (0.05f, 30.0f, 0.001f, 0.4f), 10.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::release, v }, "Release",
        NormalisableRange<float> (50.0f, 3500.0f, 0.1f, 0.4f), 200.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, v }, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::ratio, v }, "Ratio",
        StringArray { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1 Opto", "20:1", "Nuke" }, 4));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::detHP, v }, "Detector HP", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::detBand, v }, "Detector Band Emphasis", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::link, v }, "Stereo Link", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::audioHP, v }, "Audio HP", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::dist, v }, "Distortion",
        StringArray { "Off", "Dist 2", "Dist 3" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::headroom, v }, "Headroom",
        StringArray { "4 dB", "8 dB", "12 dB", "16 dB", "20 dB", "24 dB", "28 dB" }, 3));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, v }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, v }, "Bypass", false));

    return { p.begin(), p.end() };
}

PurrrstressorProcessor::PurrrstressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PURRRSTRESSOR", createLayout())
{
    pInput    = apvts.getRawParameterValue (ids::input);
    pAttack   = apvts.getRawParameterValue (ids::attack);
    pRelease  = apvts.getRawParameterValue (ids::release);
    pOutput   = apvts.getRawParameterValue (ids::output);
    pRatio    = apvts.getRawParameterValue (ids::ratio);
    pDetHP    = apvts.getRawParameterValue (ids::detHP);
    pDetBand  = apvts.getRawParameterValue (ids::detBand);
    pLink     = apvts.getRawParameterValue (ids::link);
    pAudioHP  = apvts.getRawParameterValue (ids::audioHP);
    pDist     = apvts.getRawParameterValue (ids::dist);
    pHeadroom = apvts.getRawParameterValue (ids::headroom);
    pMix      = apvts.getRawParameterValue (ids::mix);
    pBypass   = apvts.getRawParameterValue (ids::bypass);
}

purr::Params PurrrstressorProcessor::readParams() const
{
    purr::Params q;
    q.inputDb    = pInput->load();
    q.attackMs   = pAttack->load();
    q.releaseMs  = pRelease->load();
    q.outputDb   = pOutput->load();
    q.ratioIndex = (int) pRatio->load();
    q.detHP      = pDetHP->load() > 0.5f;
    q.detBand    = pDetBand->load() > 0.5f;
    q.link       = pLink->load() > 0.5f;
    q.audioHP    = pAudioHP->load() > 0.5f;
    q.dist       = (purr::DistMode) (int) pDist->load();
    q.headroomDb = 4.0f + 4.0f * pHeadroom->load();
    q.mix        = pMix->load() * 0.01f;
    return q;
}

bool PurrrstressorProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

void PurrrstressorProcessor::prepareToPlay (double sampleRate, int)
{
    core.setParams (readParams());
    core.prepare (sampleRate);
    setLatencySamples (0); // no lookahead and no oversampling yet
}

void PurrrstressorProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (pBypass->load() > 0.5f)
    {
        grMeterDb.store (0.0f);
        return; // plain pass-through (click-free crossfade comes later)
    }

    core.setParams (readParams());

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    core.process (l, r, buffer.getNumSamples());
    grMeterDb.store (core.grDb());
}

AudioProcessorEditor* PurrrstressorProcessor::createEditor()
{
    // Development stand-in only. The real editor is ported from the approved mockup.
    return new GenericAudioProcessorEditor (*this);
}

void PurrrstressorProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PurrrstressorProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml)); // unknown keys are ignored by APVTS
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PurrrstressorProcessor(); }
