#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

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
    currentSampleRate = sampleRate;
    for (auto& c : cores)
        c.prepare (sampleRate);

    prepareLimiter (sampleRate);
}

void SteveSledgeCompressorAudioProcessor::prepareLimiter (double sampleRate)
{
    lookaheadSamples = std::max (1, (int) std::lround (sampleRate * lookaheadMs * 0.001));
    limiterBufferSize = lookaheadSamples + 1;
    limiterPos = 0;
    limiterGain = 1.0f;

    for (auto& b : delayBuffer)
        b.assign ((size_t) limiterBufferSize, 0.0f);
    gainPlan.assign ((size_t) limiterBufferSize, 1.0f);

    setLatencySamples (lookaheadSamples);
    limiterMeterDb.store (0.0f, std::memory_order_relaxed);
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

    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = cores[(size_t) ch].processSample (data[i], p);
    }

    for (int band = 0; band < 4; ++band)
    {
        float gr = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            gr = std::max (gr, cores[(size_t) ch].getBandGainReductionDb ((size_t) band));
        bandMeterDb[(size_t) band].store (gr, std::memory_order_relaxed);
    }

    processLimiter (buffer, apvts.getRawParameterValue ("ceiling")->load());
}

void SteveSledgeCompressorAudioProcessor::processLimiter (juce::AudioBuffer<float>& buffer, float ceilingDb)
{
    const int channels = juce::jmin (buffer.getNumChannels(), 2);
    if (channels <= 0 || lookaheadSamples <= 0)
        return;

    const float ceiling = juce::Decibels::decibelsToGain (ceilingDb);
    const float releaseCoeff = std::exp (-1.0f / (float) (currentSampleRate * limiterReleaseMs * 0.001));
    const int nSamples = buffer.getNumSamples();

    for (int i = 0; i < nSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float x = buffer.getSample (ch, i);
            delayBuffer[(size_t) ch][(size_t) limiterPos] = x;
            peak = std::max (peak, std::abs (x));
        }

        const float requiredGain = peak > ceiling
            ? ceiling / std::max (peak, 1.0e-20f)
            : 1.0f;

        if (requiredGain < 1.0f)
        {
            for (int d = 1; d <= lookaheadSamples; ++d)
            {
                const float phase = (float) d / (float) lookaheadSamples;
                const float smooth = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * phase);
                const float planned = 1.0f + (requiredGain - 1.0f) * smooth;
                const int idx = (limiterPos + d) % limiterBufferSize;
                gainPlan[(size_t) idx] = std::min (gainPlan[(size_t) idx], planned);
            }
        }

        const float target = gainPlan[(size_t) limiterPos];
        if (target < limiterGain)
            limiterGain = target;
        else
            limiterGain = releaseCoeff * limiterGain + (1.0f - releaseCoeff) * target;

        for (int ch = 0; ch < channels; ++ch)
        {
            const int readPos = (limiterPos + 1) % limiterBufferSize;
            const float delayed = delayBuffer[(size_t) ch][(size_t) readPos];
            buffer.setSample (ch, i, delayed * limiterGain);
        }

        gainPlan[(size_t) limiterPos] = 1.0f;
        limiterPos = (limiterPos + 1) % limiterBufferSize;
    }

    limiterMeterDb.store (-juce::Decibels::gainToDecibels (std::max (limiterGain, 1.0e-9f)),
                          std::memory_order_relaxed);
}

float SteveSledgeCompressorAudioProcessor::getBandMeterDb (int band) const
{
    if (band < 0 || band >= 4) return 0.0f;
    return bandMeterDb[(size_t) band].load (std::memory_order_relaxed);
}

float SteveSledgeCompressorAudioProcessor::getLimiterMeterDb() const
{
    return limiterMeterDb.load (std::memory_order_relaxed);
}

juce::AudioProcessorEditor* SteveSledgeCompressorAudioProcessor::createEditor()
{
    return new SteveSledgeCompressorAudioProcessorEditor (*this);
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
