#pragma once

#include <JuceHeader.h>
#include "DspCore.h"
#include <array>
#include <atomic>
#include <vector>

class SteveSledgeCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    SteveSledgeCompressorAudioProcessor();
    ~SteveSledgeCompressorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? "Default" : juce::String{}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getBandMeterDb (int band) const;
    float getLimiterMeterDb() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void prepareLimiter (double sampleRate);
    void processLimiter (juce::AudioBuffer<float>& buffer, float ceilingDb);
    void applyOutputGain (juce::AudioBuffer<float>& buffer, float gainDb);

    std::array<SteveSledgeDspCore, 2> cores;

    static constexpr double lookaheadMs = 5.0;
    static constexpr double limiterReleaseMs = 50.0;
    int lookaheadSamples = 0;
    int limiterBufferSize = 1;
    int limiterPos = 0;
    double currentSampleRate = 48000.0;
    float limiterGain = 1.0f;
    std::array<std::vector<float>, 2> delayBuffer;
    std::vector<float> gainPlan;

    std::array<std::atomic<float>, 4> bandMeterDb { 0.0f, 0.0f, 0.0f, 0.0f };
    std::atomic<float> limiterMeterDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteveSledgeCompressorAudioProcessor)
};
