#include "PluginProcessor.h"

SteveSledgeCompressorAudioProcessor::SteveSledgeCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SteveSledgeCompressorAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<APF> (juce::ParameterID { "input", 1 }, "Input Gain",
                                      Range { -20.0f, 20.0f, 0.1f }, 0.0f, "dB"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "threshold", 1 }, "Threshold",
                                      Range { -40.0f, -15.0f, 0.1f }, -29.0f, "dBFS"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "ratio", 1 }, "Ratio",
                                      Range { 1.0f, 10.0f, 0.1f }, 4.0f, ":1"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "speed", 1 }, "Speed",
                                      Range { 0.1f, 10.0f, 0.01f }, 1.0f, "x"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "makeup", 1 }, "Makeup",
                                      Range { 0.0f, 20.0f, 0.1f }, 15.0f, "dB"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "ceiling", 1 }, "Limiter Ceiling",
                                      Range { -20.0f, -0.1f, 0.1f }, -0.1f, "dBFS"));
    return layout;
}

void SteveSledgeCompressorAudioProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& c : cores)
        c.prepare (sampleRate);
}

bool SteveSledgeCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SteveSledgeCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    SteveSledgeDspCore::Params p;
    p.inputGainDb = apvts.getRawParameterValue ("input")->load();
    p.thresholdDb = apvts.getRawParameterValue ("threshold")->load();
    p.ratio       = apvts.getRawParameterValue ("ratio")->load();
    p.speed       = apvts.getRawParameterValue ("speed")->load();
    p.makeupDb    = apvts.getRawParameterValue ("makeup")->load();
    p.ceilingDb   = apvts.getRawParameterValue ("ceiling")->load();

    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = cores[(size_t) ch].processSample (data[i], p);
    }
}

juce::AudioProcessorEditor* SteveSledgeCompressorAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void SteveSledgeCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SteveSledgeCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SteveSledgeCompressorAudioProcessor();
}
