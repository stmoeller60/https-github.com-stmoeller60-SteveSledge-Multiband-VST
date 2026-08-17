#include "PluginEditor.h"

SteveSledgeCompressorAudioProcessorEditor::SteveSledgeCompressorAudioProcessorEditor (SteveSledgeCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (760, 500);

    setupSlider (inputSlider, inputLabel, "Input");
    setupSlider (thresholdSlider, thresholdLabel, "Threshold");
    setupSlider (ratioSlider, ratioLabel, "Ratio");
    setupSlider (speedSlider, speedLabel, "Speed");
    setupSlider (makeupSlider, makeupLabel, "Makeup");
    setupSlider (ceilingSlider, ceilingLabel, "Ceiling");

    inputAttachment = std::make_unique<Attachment> (processor.apvts, "input", inputSlider);
    thresholdAttachment = std::make_unique<Attachment> (processor.apvts, "threshold", thresholdSlider);
    ratioAttachment = std::make_unique<Attachment> (processor.apvts, "ratio", ratioSlider);
    speedAttachment = std::make_unique<Attachment> (processor.apvts, "speed", speedSlider);
    makeupAttachment = std::make_unique<Attachment> (processor.apvts, "makeup", makeupSlider);
    ceilingAttachment = std::make_unique<Attachment> (processor.apvts, "ceiling", ceilingSlider);

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

void SteveSledgeCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17191d));
    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawText ("Steve Sledge Multiband Compressor", 20, 12, getWidth() - 40, 30,
                juce::Justification::centred);

    g.setFont (13.0f);
    g.setColour (juce::Colour (0xffb7bcc6));
    g.drawText ("5 ms stereo-linked lookahead brickwall limiter", 20, 42, getWidth() - 40, 20,
                juce::Justification::centred);

    const int meterTop = 92;
    const int meterHeight = 210;
    const int meterWidth = 72;
    const int gap = 30;
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
}

void SteveSledgeCompressorAudioProcessorEditor::resized()
{
    const int y = 380;
    const int sliderW = 112;
    const int sliderH = 95;
    const int gap = 8;
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

void SteveSledgeCompressorAudioProcessorEditor::timerCallback()
{
    for (int i = 0; i < 4; ++i)
        meterDb[(size_t) i] = processor.getBandMeterDb (i);
    meterDb[4] = processor.getLimiterMeterDb();
    repaint();
}
