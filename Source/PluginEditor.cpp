#include "PluginEditor.h"

SteveSledgeCompressorAudioProcessorEditor::SteveSledgeCompressorAudioProcessorEditor (SteveSledgeCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1000, 650);

    addAndMakeVisible (simpleModeButton);
    simpleModeButton.setClickingTogglesState (true);

    setupSlider (compSlider, compLabel, "COMP  0...10");
    setupSlider (attackSlider, attackLabel, "ATTACK  Fast...Slow");

    setupSlider (ratioSlider, ratioLabel, "RATIO");
    setupSlider (masterSlider, masterLabel, "MASTER  post limiter");

    setupSlider (inputSlider, inputLabel, "Input");
    setupSlider (thresholdSlider, thresholdLabel, "Threshold");
    setupSlider (speedSlider, speedLabel, "Speed");
    setupSlider (makeupSlider, makeupLabel, "Makeup");
    setupSlider (ceilingSlider, ceilingLabel, "Ceiling");
    setupSlider (xover1Slider, xover1Label, "XOVER 1");
    setupSlider (xover2Slider, xover2Label, "XOVER 2");
    setupSlider (xover3Slider, xover3Label, "XOVER 3");

    simpleAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "simple", simpleModeButton);
    compAttachment = std::make_unique<SliderAttachment> (processor.apvts, "comp", compSlider);
    attackAttachment = std::make_unique<SliderAttachment> (processor.apvts, "attack", attackSlider);
    ratioAttachment = std::make_unique<SliderAttachment> (processor.apvts, "ratio", ratioSlider);
    masterAttachment = std::make_unique<SliderAttachment> (processor.apvts, "master", masterSlider);

    inputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "input", inputSlider);
    thresholdAttachment = std::make_unique<SliderAttachment> (processor.apvts, "threshold", thresholdSlider);
    speedAttachment = std::make_unique<SliderAttachment> (processor.apvts, "speed", speedSlider);
    makeupAttachment = std::make_unique<SliderAttachment> (processor.apvts, "makeup", makeupSlider);
    ceilingAttachment = std::make_unique<SliderAttachment> (processor.apvts, "ceiling", ceilingSlider);
    xover1Attachment = std::make_unique<SliderAttachment> (processor.apvts, "xover1", xover1Slider);
    xover2Attachment = std::make_unique<SliderAttachment> (processor.apvts, "xover2", xover2Slider);
    xover3Attachment = std::make_unique<SliderAttachment> (processor.apvts, "xover3", xover3Slider);

    lastSimpleMode = simpleModeButton.getToggleState();

    simpleModeButton.onClick = [this]
    {
        const bool nowSimple = simpleModeButton.getToggleState();
        if (lastSimpleMode && ! nowSimple)
            processor.syncAdvancedFromSimple();

        lastSimpleMode = nowSimple;
        updateModeVisibility();
        resized();
        repaint();
    };

    updateModeVisibility();
    startTimerHz (30);
}

void SteveSledgeCompressorAudioProcessorEditor::setupSlider (juce::Slider& slider,
                                                              juce::Label& label,
                                                              const juce::String& name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
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

    // Ratio and Master are true shared parameters, so they are visible in both modes.
    ratioSlider.setVisible (true); ratioLabel.setVisible (true);
    masterSlider.setVisible (true); masterLabel.setVisible (true);

    inputSlider.setVisible (! simple); inputLabel.setVisible (! simple);
    thresholdSlider.setVisible (! simple); thresholdLabel.setVisible (! simple);
    speedSlider.setVisible (! simple); speedLabel.setVisible (! simple);
    makeupSlider.setVisible (! simple); makeupLabel.setVisible (! simple);
    ceilingSlider.setVisible (! simple); ceilingLabel.setVisible (! simple);
    xover1Slider.setVisible (! simple); xover1Label.setVisible (! simple);
    xover2Slider.setVisible (! simple); xover2Label.setVisible (! simple);
    xover3Slider.setVisible (! simple); xover3Label.setVisible (! simple);
}

void SteveSledgeCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17191d));
    g.setColour (juce::Colours::white);
    g.setFont (23.0f);
    g.drawText ("Steve Sledge Multiband Compressor", 20, 12, getWidth() - 40, 30,
                juce::Justification::centred);

    g.setFont (13.0f);
    g.setColour (juce::Colour (0xffb7bcc6));
    const auto subtitle = simpleModeButton.getToggleState()
        ? "SIMPLE: COMP couples Threshold + Makeup | ATTACK: 0 = Fast, 5 = Normal, 10 = Slow | Limiter fixed at -0.5 dBFS"
        : "ADVANCED: full DSP access | 4 compressor bands / 3 adjustable crossovers | 5 ms stereo-linked lookahead limiter";
    g.drawText (subtitle, 20, 44, getWidth() - 40, 20, juce::Justification::centred);

    const int meterTop = 94;
    const int meterHeight = 215;
    const int meterWidth = 78;
    const int gap = 42;
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
    g.drawText ("Gain Reduction  |  four bands + limiter", 10, meterTop - 20, getWidth() - 20, 16,
                juce::Justification::centred);

    g.setColour (juce::Colour (0xff8e949f));
    g.setFont (11.5f);
    if (simpleModeButton.getToggleState())
    {
        g.drawText ("COMP: 0 = almost uncompressed, 10 = strong compression     |     ATTACK: 0 = fast, 10 = slow     |     MASTER is after the limiter",
                    20, getHeight() - 25, getWidth() - 40, 17, juce::Justification::centred);
    }
    else
    {
        g.drawText ("Crossovers define the four bands. Leaving SIMPLE automatically writes its actual Threshold, Makeup, Speed, Ceiling and crossover values into ADVANCED.",
                    20, getHeight() - 25, getWidth() - 40, 17, juce::Justification::centred);
    }
}

void SteveSledgeCompressorAudioProcessorEditor::resized()
{
    simpleModeButton.setBounds (getWidth() - 125, 18, 102, 25);

    if (simpleModeButton.getToggleState())
    {
        const int y = 430;
        const int sliderW = 165;
        const int sliderH = 110;
        const int gap = 28;
        const int totalW = 4 * sliderW + 3 * gap;
        const int x0 = (getWidth() - totalW) / 2;

        std::array<juce::Slider*, 4> sliders { &compSlider, &ratioSlider, &attackSlider, &masterSlider };
        std::array<juce::Label*, 4> labels { &compLabel, &ratioLabel, &attackLabel, &masterLabel };
        for (int i = 0; i < 4; ++i)
        {
            const int x = x0 + i * (sliderW + gap);
            labels[(size_t) i]->setBounds (x, y - 24, sliderW, 22);
            sliders[(size_t) i]->setBounds (x, y, sliderW, sliderH);
        }
    }
    else
    {
        // Two compact rows keep all Advanced controls readable.
        const int sliderW = 150;
        const int sliderH = 100;
        const int gap = 24;
        const int row1Y = 385;
        const int row2Y = 515;

        std::array<juce::Slider*, 5> row1 { &inputSlider, &thresholdSlider, &ratioSlider, &speedSlider, &makeupSlider };
        std::array<juce::Label*, 5> row1Labels { &inputLabel, &thresholdLabel, &ratioLabel, &speedLabel, &makeupLabel };
        std::array<juce::Slider*, 5> row2 { &xover1Slider, &xover2Slider, &xover3Slider, &ceilingSlider, &masterSlider };
        std::array<juce::Label*, 5> row2Labels { &xover1Label, &xover2Label, &xover3Label, &ceilingLabel, &masterLabel };

        const int totalW = 5 * sliderW + 4 * gap;
        const int x0 = (getWidth() - totalW) / 2;

        for (int i = 0; i < 5; ++i)
        {
            const int x = x0 + i * (sliderW + gap);
            row1Labels[(size_t) i]->setBounds (x, row1Y - 22, sliderW, 20);
            row1[(size_t) i]->setBounds (x, row1Y, sliderW, sliderH);
            row2Labels[(size_t) i]->setBounds (x, row2Y - 22, sliderW, 20);
            row2[(size_t) i]->setBounds (x, row2Y, sliderW, sliderH);
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
        // Handles mode changes caused by host automation/preset recall as well as UI clicks.
        if (lastSimpleMode && ! simple)
            processor.syncAdvancedFromSimple();

        lastSimpleMode = simple;
        updateModeVisibility();
        resized();
    }

    repaint();
}
