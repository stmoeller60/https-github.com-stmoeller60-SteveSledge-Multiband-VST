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
    void updateModeVisibility();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    SteveSledgeCompressorAudioProcessor& processor;
    juce::ToggleButton simpleModeButton { "SIMPLE" };

    juce::Slider compSlider, attackSlider;
    juce::Label compLabel, attackLabel;
    juce::Slider ratioSlider, masterSlider;
    juce::Label ratioLabel, masterLabel;
    juce::Slider inputSlider, thresholdSlider, speedSlider, makeupSlider, ceilingSlider;
    juce::Slider xover1Slider, xover2Slider, xover3Slider;
    juce::Label inputLabel, thresholdLabel, speedLabel, makeupLabel, ceilingLabel;
    juce::Label xover1Label, xover2Label, xover3Label;

    std::unique_ptr<ButtonAttachment> simpleAttachment;
    std::unique_ptr<SliderAttachment> compAttachment, attackAttachment,
                                      ratioAttachment, masterAttachment,
                                      inputAttachment, thresholdAttachment,
                                      speedAttachment, makeupAttachment, ceilingAttachment,
                                      xover1Attachment, xover2Attachment, xover3Attachment;

    std::array<float, 5> meterDb { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    static constexpr int historySize = 120; // 4 seconds at the 30 Hz GUI timer rate
    std::array<std::array<float, historySize>, 5> grHistory {};
    int historyWritePos = 0;
    float outputPeakDb = -100.0f;
    bool lastSimpleMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SteveSledgeCompressorAudioProcessorEditor)
};
