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
    using APB = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Musical/Simple layer.
    layout.add (std::make_unique<APB> (juce::ParameterID { "simple", 1 }, "Simple Mode", false));
    layout.add (std::make_unique<APF> (juce::ParameterID { "comp", 1 }, "Comp",
                                      Range { 0.0f, 10.0f, 0.1f }, 5.0f, ""));
    layout.add (std::make_unique<APF> (juce::ParameterID { "attack", 1 }, "Attack",
                                      Range { 0.0f, 10.0f, 0.1f }, 5.0f, ""));

    // Kept for backwards-compatible state loading; no longer shown or used.
    layout.add (std::make_unique<APF> (juce::ParameterID { "level", 1 }, "Legacy Level",
                                      Range { -20.0f, 10.0f, 0.1f }, 0.0f, "dB"));

    // Shared/Advanced layer.
    layout.add (std::make_unique<APF> (juce::ParameterID { "master", 1 }, "Master",
                                      Range { -20.0f, 10.0f, 0.1f }, 0.0f, "dB"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "input", 1 }, "Input Gain",
                                      Range { -20.0f, 20.0f, 0.1f }, 0.0f, "dB"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "threshold", 1 }, "Threshold",
                                      Range { -50.0f, -5.0f, 0.1f }, -29.0f, "dBFS"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "ratio", 1 }, "Ratio",
                                      Range { 1.0f, 10.0f, 0.1f }, 4.0f, ":1"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "speed", 1 }, "Speed",
                                      Range { 0.1f, 10.0f, 0.01f }, 1.0f, "x"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "makeup", 1 }, "Makeup",
                                      Range { 0.0f, 20.0f, 0.1f }, 15.0f, "dB"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "ceiling", 1 }, "Limiter Ceiling",
                                      Range { -20.0f, -0.1f, 0.1f }, -0.5f, "dBFS"));

    // Three crossovers = four compressor bands.
    layout.add (std::make_unique<APF> (juce::ParameterID { "xover1", 1 }, "Crossover 1",
                                      Range { 80.0f, 500.0f, 1.0f, 0.40f }, 250.0f, "Hz"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "xover2", 1 }, "Crossover 2",
                                      Range { 500.0f, 1500.0f, 1.0f, 0.45f }, 800.0f, "Hz"));
    layout.add (std::make_unique<APF> (juce::ParameterID { "xover3", 1 }, "Crossover 3",
                                      Range { 1500.0f, 6000.0f, 1.0f, 0.45f }, 2500.0f, "Hz"));
    return layout;
}

SteveSledgeDspCore::Params SteveSledgeCompressorAudioProcessor::makeSimpleParams() const
{
    SteveSledgeDspCore::Params p;
    const float ratio = apvts.getRawParameterValue ("ratio")->load();

    // COMP 0...10: 0 = essentially no compression on normal guitar levels,
    // 10 = deep compression. The curve gives more resolution in the useful middle range.
    const float c = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("comp")->load() * 0.1f);
    const float shapedComp = std::pow (c, 1.15f);
    p.thresholdDb = -5.0f - 35.0f * shapedComp;
    p.ratio = ratio;
    p.inputGainDb = 0.0f;

    // Automatic musical makeup estimate around a representative guitar level.
    constexpr float referenceGuitarDb = -14.0f;
    const float staticGr = referenceGuitarDb > p.thresholdDb
        ? (referenceGuitarDb - p.thresholdDb) * (1.0f - 1.0f / std::max (p.ratio, 1.0f))
        : 0.0f;
    p.makeupDb = juce::jlimit (0.0f, 20.0f, staticGr * 0.70f);

    // ATTACK 0...10: 0 = FAST, 5 = NORMAL, 10 = SLOW.
    // Internally Speed is inverse to time, therefore the mapping is reversed.
    const float a = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("attack")->load() * 0.1f);
    p.speed = std::pow (10.0f, 1.0f - 2.0f * a); // 10x ... 1x ... 0.1x

    // Fixed guitar-oriented crossovers in Simple mode.
    p.xover1Hz = 250.0f;
    p.xover2Hz = 800.0f;
    p.xover3Hz = 2500.0f;
    return p;
}

void SteveSledgeCompressorAudioProcessor::setParameterValue (const juce::String& id, float value)
{
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void SteveSledgeCompressorAudioProcessor::syncAdvancedFromSimple()
{
    const auto p = makeSimpleParams();
    setParameterValue ("input", p.inputGainDb);
    setParameterValue ("threshold", p.thresholdDb);
    setParameterValue ("speed", p.speed);
    setParameterValue ("makeup", p.makeupDb);
    setParameterValue ("ceiling", -0.5f);
    setParameterValue ("xover1", p.xover1Hz);
    setParameterValue ("xover2", p.xover2Hz);
    setParameterValue ("xover3", p.xover3Hz);
}

void SteveSledgeCompressorAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    for (auto& c : cores)
        c.prepare (sampleRate);
    prepareLimiter (sampleRate);
    outputPeakLinear.store (0.0f, std::memory_order_relaxed);
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

    const bool simpleMode = apvts.getRawParameterValue ("simple")->load() >= 0.5f;
    SteveSledgeDspCore::Params p;
    float ceilingDb = apvts.getRawParameterValue ("ceiling")->load();

    if (simpleMode)
    {
        p = makeSimpleParams();
        ceilingDb = -0.5f;
    }
    else
    {
        p.inputGainDb = apvts.getRawParameterValue ("input")->load();
        p.thresholdDb = apvts.getRawParameterValue ("threshold")->load();
        p.ratio       = apvts.getRawParameterValue ("ratio")->load();
        p.speed       = apvts.getRawParameterValue ("speed")->load();
        p.makeupDb    = apvts.getRawParameterValue ("makeup")->load();
        p.xover1Hz    = apvts.getRawParameterValue ("xover1")->load();
        p.xover2Hz    = apvts.getRawParameterValue ("xover2")->load();
        p.xover3Hz    = apvts.getRawParameterValue ("xover3")->load();
    }

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

    processLimiter (buffer, ceilingDb);

    // MASTER is deliberately after the limiter and never changes compression/limiting behaviour.
    applyOutputGain (buffer, apvts.getRawParameterValue ("master")->load());

    // Diagnostic meter: capture the highest sample peak after MASTER until the UI reads it.
    float blockPeak = 0.0f;
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            blockPeak = std::max (blockPeak, std::abs (data[i]));
    }

    float stored = outputPeakLinear.load (std::memory_order_relaxed);
    while (blockPeak > stored
           && ! outputPeakLinear.compare_exchange_weak (stored, blockPeak,
                                                        std::memory_order_relaxed,
                                                        std::memory_order_relaxed))
    {
    }
}

void SteveSledgeCompressorAudioProcessor::applyOutputGain (juce::AudioBuffer<float>& buffer, float gainDb)
{
    buffer.applyGain (juce::Decibels::decibelsToGain (gainDb));
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

        const int readPos = (limiterPos + 1) % limiterBufferSize;
        const float requiredGain = peak > ceiling
            ? ceiling / std::max (peak, 1.0e-20f)
            : 1.0f;

        if (requiredGain < 1.0f)
        {
            for (int d = 0; d <= lookaheadSamples; ++d)
            {
                const float phase = (float) d / (float) lookaheadSamples;
                const float smooth = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * phase);
                const float planned = 1.0f + (requiredGain - 1.0f) * smooth;
                const int idx = (readPos + d) % limiterBufferSize;
                gainPlan[(size_t) idx] = std::min (gainPlan[(size_t) idx], planned);
            }
        }

        const float target = gainPlan[(size_t) readPos];
        if (target < limiterGain)
            limiterGain = target;
        else
            limiterGain = releaseCoeff * limiterGain + (1.0f - releaseCoeff) * target;

        for (int ch = 0; ch < channels; ++ch)
        {
            const float delayed = delayBuffer[(size_t) ch][(size_t) readPos];
            buffer.setSample (ch, i, delayed * limiterGain);
        }

        gainPlan[(size_t) readPos] = 1.0f;
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

float SteveSledgeCompressorAudioProcessor::getOutputPeakDb()
{
    const float peak = outputPeakLinear.exchange (0.0f, std::memory_order_relaxed);
    return juce::Decibels::gainToDecibels (std::max (peak, 1.0e-5f), -100.0f);
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
