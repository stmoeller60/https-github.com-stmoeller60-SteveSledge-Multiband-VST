#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>

class SteveSledgeCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit SteveSledgeCompressorAudioProcessorEditor (SteveSledgeCompressorAudioProcessor&);
    ~SteveSledgeCompressorAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupSlider (juce::Slider& slider, juce::Label& label, const juce::String& name);

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    SteveSledgeCompressorAudioProcessor& processor;

    juce::Slider inputSlider, thresholdSlider, ratioSlider, speedSlider, makeupSlider, ceilingSlider;
    juce::Label inputLabel, thresholdLabel, ratioLabel, speedLabel, makeupLabel, ceilingLabel;

    std::unique_ptr<Attachment> inputAttachment, thresholdAttachment, ratioAttachment,
                                speedAttachment, makeupAttachment, ceilingAttachment;

    std::array<float, 5> meterDb { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteveSledgeCompressorAudioProcessorEditor)
};
