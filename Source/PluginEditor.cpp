#include "PluginEditor.h"

SteveSledgeCompressorAudioProcessorEditor::SteveSledgeCompressorAudioProcessorEditor (SteveSledgeCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (820, 540);

    addAndMakeVisible (simpleModeButton);
    simpleModeButton.setClickingTogglesState (true);

    setupSlider (compSlider, compLabel, "COMP");
    setupSlider (attackSlider, attackLabel, "ATTACK");
    setupSlider (levelSlider, levelLabel, "LEVEL");

    setupSlider (inputSlider, inputLabel, "Input");
    setupSlider (thresholdSlider, thresholdLabel, "Threshold");
    setupSlider (ratioSlider, ratioLabel, "RATIO");
    setupSlider (speedSlider, speedLabel, "Speed");
    setupSlider (makeupSlider, makeupLabel, "Makeup");
    setupSlider (ceilingSlider, ceilingLabel, "Ceiling");

    simpleAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "simple", simpleModeButton);
    compAttachment = std::make_unique<SliderAttachment> (processor.apvts, "comp", compSlider);
    attackAttachment = std::make_unique<SliderAttachment> (processor.apvts, "attack", attackSlider);
    levelAttachment = std::make_unique<SliderAttachment> (processor.apvts, "level", levelSlider);

    inputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "input", inputSlider);
    thresholdAttachment = std::make_unique<SliderAttachment> (processor.apvts, "threshold", thresholdSlider);
    ratioAttachment = std::make_unique<SliderAttachment> (processor.apvts, "ratio", ratioSlider);
    speedAttachment = std::make_unique<SliderAttachment> (processor.apvts, "speed", speedSlider);
    makeupAttachment = std::make_unique<SliderAttachment> (processor.apvts, "makeup", makeupSlider);
    ceilingAttachment = std::make_unique<SliderAttachment> (processor.apvts, "ceiling", ceilingSlider);

    simpleModeButton.onClick = [this]
    {
        updateModeVisibility();
        resized();
        repaint();
    };

    lastSimpleMode = simpleModeButton.getToggleState();
    updateModeVisibility();
    startTimerHz (30);
}

void SteveSledgeCompressorAudioProcessorEditor::setupSlider (juce::Slider& slider,
                                                              juce::Label& label,
                                                              const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 20);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void SteveSledgeCompressorAudioProcessorEditor::updateModeVisibility()
{
    const bool simple = simpleModeButton.getToggleState();

    compSlider.setVisible (simple); compLabel.setVisible (simple);
    attackSlider.setVisible (simple); attackLabel.setVisible (simple);
    levelSlider.setVisible (simple); levelLabel.setVisible (simple);

    inputSlider.setVisible (! simple); inputLabel.setVisible (! simple);
    thresholdSlider.setVisible (! simple); thresholdLabel.setVisible (! simple);
    speedSlider.setVisible (! simple); speedLabel.setVisible (! simple);
    makeupSlider.setVisible (! simple); makeupLabel.setVisible (! simple);
    ceilingSlider.setVisible (! simple); ceilingLabel.setVisible (! simple);

    // Ratio is intentionally shared: in Simple mode it is the direct musical RATIO control.
    ratioSlider.setVisible (true);
    ratioLabel.setVisible (true);
}

void SteveSledgeCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17191d));
    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawText ("Steve Sledge Multiband Compressor", 20, 12, getWidth() - 40, 30,
                juce::Justification::centred);

    g.setFont (13.0f);
    g.setColour (juce::Colour (0xffb7bcc6));
    const auto subtitle = simpleModeButton.getToggleState()
        ? "Simple: COMP macro + direct RATIO + logarithmic ATTACK + LEVEL"
        : "Advanced: full parameter access | 5 ms stereo-linked lookahead limiter";
    g.drawText (subtitle, 20, 42, getWidth() - 40, 20, juce::Justification::centred);

    const int meterTop = 96;
    const int meterHeight = 220;
    const int meterWidth = 76;
    const int gap = 34;
    const int totalWidth = 5 * meterWidth + 4 * gap;
    const int startX = (getWidth() - totalWidth) / 2;
    static const char* names[] = { "Band 1", "Band 2", "Band 3", "Band 4", "Limiter" };

    for (int i = 0; i < 5; ++i)
    {
        const int x = startX + i * (meterWidth + gap);
        auto meterArea = juce::Rectangle<float> ((float) x, (float) meterTop, (float) meterWidth, (float) meterHeight);

        g.setColour (juce::Colour (0xff2a2e35));
        g.fillRoundedRectangle (meterArea, 5.0f);

        const float db = juce::jlimit (0.0f, 24.0f, meterDb[(size_t) i]);
        const float norm = db / 24.0f;
        auto filled = meterArea.withY (meterArea.getBottom() - meterArea.getHeight() * norm)
                               .withHeight (meterArea.getHeight() * norm);
        g.setColour (i == 4 ? juce::Colour (0xfff29d49) : juce::Colour (0xff55b7d9));
        g.fillRoundedRectangle (filled, 4.0f);

        g.setColour (juce::Colours::white);
        g.setFont (13.0f);
        g.drawText (names[i], x - 8, meterTop + meterHeight + 7, meterWidth + 16, 18,
                    juce::Justification::centred);
        g.drawText (juce::String (db, 1) + " dB", x - 8, meterTop + meterHeight + 27, meterWidth + 16, 18,
                    juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xff777d88));
    g.setFont (11.0f);
    g.drawText ("Gain Reduction (0 ... 24 dB)", 10, meterTop - 20, getWidth() - 20, 16,
                juce::Justification::centred);

    if (simpleModeButton.getToggleState())
    {
        g.setColour (juce::Colour (0xff8e949f));
        g.setFont (11.0f);
        g.drawText ("Simple mode: limiter fixed at -0.5 dBFS. COMP automatically couples Threshold + Makeup.",
                    20, getHeight() - 24, getWidth() - 40, 16, juce::Justification::centred);
    }
}

void SteveSledgeCompressorAudioProcessorEditor::resized()
{
    simpleModeButton.setBounds (getWidth() - 112, 18, 92, 24);

    const int y = 405;
    const int sliderH = 100;

    if (simpleModeButton.getToggleState())
    {
        const int sliderW = 140;
        const int gap = 18;
        const int totalW = 4 * sliderW + 3 * gap;
        const int x0 = (getWidth() - totalW) / 2;

        std::array<juce::Slider*, 4> sliders { &compSlider, &ratioSlider, &attackSlider, &levelSlider };
        std::array<juce::Label*, 4> labels { &compLabel, &ratioLabel, &attackLabel, &levelLabel };
        for (int i = 0; i < 4; ++i)
        {
            const int x = x0 + i * (sliderW + gap);
            labels[(size_t) i]->setBounds (x, y - 22, sliderW, 20);
            sliders[(size_t) i]->setBounds (x, y, sliderW, sliderH);
        }
    }
    else
    {
        const int sliderW = 116;
        const int gap = 9;
        const int totalW = 6 * sliderW + 5 * gap;
        const int x0 = (getWidth() - totalW) / 2;

        std::array<juce::Slider*, 6> sliders { &inputSlider, &thresholdSlider, &ratioSlider,
                                               &speedSlider, &makeupSlider, &ceilingSlider };
        std::array<juce::Label*, 6> labels { &inputLabel, &thresholdLabel, &ratioLabel,
                                             &speedLabel, &makeupLabel, &ceilingLabel };
        for (int i = 0; i < 6; ++i)
        {
            const int x = x0 + i * (sliderW + gap);
            labels[(size_t) i]->setBounds (x, y - 22, sliderW, 20);
            sliders[(size_t) i]->setBounds (x, y, sliderW, sliderH);
        }
    }
}

void SteveSledgeCompressorAudioProcessorEditor::timerCallback()
{
    for (int i = 0; i < 4; ++i)
        meterDb[(size_t) i] = processor.getBandMeterDb (i);
    meterDb[4] = processor.getLimiterMeterDb();

    const bool simple = simpleModeButton.getToggleState();
    if (simple != lastSimpleMode)
    {
        lastSimpleMode = simple;
        updateModeVisibility();
        resized();
    }

    repaint();
}
