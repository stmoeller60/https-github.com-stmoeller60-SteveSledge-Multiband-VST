#include "PluginEditor.h"

SteveSledgeCompressorAudioProcessorEditor::SteveSledgeCompressorAudioProcessorEditor (SteveSledgeCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1000, 760);
    addAndMakeVisible (simpleModeButton);
    simpleModeButton.setClickingTogglesState (true);
    setupSlider (compSlider, compLabel, "COMP  0...10"); setupSlider (attackSlider, attackLabel, "ATTACK  Fast...Slow");
    setupSlider (ratioSlider, ratioLabel, "RATIO"); setupSlider (masterSlider, masterLabel, "MASTER  post limiter");
    setupSlider (inputSlider, inputLabel, "Input"); setupSlider (thresholdSlider, thresholdLabel, "Threshold");
    setupSlider (speedSlider, speedLabel, "Speed"); setupSlider (makeupSlider, makeupLabel, "Makeup");
    setupSlider (ceilingSlider, ceilingLabel, "Ceiling"); setupSlider (xover1Slider, xover1Label, "XOVER 1");
    setupSlider (xover2Slider, xover2Label, "XOVER 2"); setupSlider (xover3Slider, xover3Label, "XOVER 3");
    simpleAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "simple", simpleModeButton);
    compAttachment = std::make_unique<SliderAttachment> (processor.apvts, "comp", compSlider); attackAttachment = std::make_unique<SliderAttachment> (processor.apvts, "attack", attackSlider);
    ratioAttachment = std::make_unique<SliderAttachment> (processor.apvts, "ratio", ratioSlider); masterAttachment = std::make_unique<SliderAttachment> (processor.apvts, "master", masterSlider);
    inputAttachment = std::make_unique<SliderAttachment> (processor.apvts, "input", inputSlider); thresholdAttachment = std::make_unique<SliderAttachment> (processor.apvts, "threshold", thresholdSlider);
    speedAttachment = std::make_unique<SliderAttachment> (processor.apvts, "speed", speedSlider); makeupAttachment = std::make_unique<SliderAttachment> (processor.apvts, "makeup", makeupSlider);
    ceilingAttachment = std::make_unique<SliderAttachment> (processor.apvts, "ceiling", ceilingSlider); xover1Attachment = std::make_unique<SliderAttachment> (processor.apvts, "xover1", xover1Slider);
    xover2Attachment = std::make_unique<SliderAttachment> (processor.apvts, "xover2", xover2Slider); xover3Attachment = std::make_unique<SliderAttachment> (processor.apvts, "xover3", xover3Slider);
    lastSimpleMode = simpleModeButton.getToggleState();
    simpleModeButton.onClick = [this] { const bool s = simpleModeButton.getToggleState(); if (lastSimpleMode && !s) processor.syncAdvancedFromSimple(); lastSimpleMode=s; updateModeVisibility(); resized(); repaint(); };
    updateModeVisibility(); startTimerHz (30);
}

void SteveSledgeCompressorAudioProcessorEditor::setupSlider (juce::Slider& s, juce::Label& l, const juce::String& n)
{ s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::TextBoxBelow,false,90,20); addAndMakeVisible(s); l.setText(n,juce::dontSendNotification); l.setJustificationType(juce::Justification::centred); addAndMakeVisible(l); }

void SteveSledgeCompressorAudioProcessorEditor::updateModeVisibility()
{
    const bool s=simpleModeButton.getToggleState(); compSlider.setVisible(s); compLabel.setVisible(s); attackSlider.setVisible(s); attackLabel.setVisible(s);
    ratioSlider.setVisible(true); ratioLabel.setVisible(true); masterSlider.setVisible(true); masterLabel.setVisible(true);
    inputSlider.setVisible(!s); inputLabel.setVisible(!s); thresholdSlider.setVisible(!s); thresholdLabel.setVisible(!s); speedSlider.setVisible(!s); speedLabel.setVisible(!s); makeupSlider.setVisible(!s); makeupLabel.setVisible(!s); ceilingSlider.setVisible(!s); ceilingLabel.setVisible(!s);
    xover1Slider.setVisible(!s); xover1Label.setVisible(!s); xover2Slider.setVisible(!s); xover2Label.setVisible(!s); xover3Slider.setVisible(!s); xover3Label.setVisible(!s);
}

void SteveSledgeCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff17191d)); g.setColour(juce::Colours::white); g.setFont(23.0f); g.drawText("Steve Sledge Multiband Compressor",20,12,getWidth()-40,30,juce::Justification::centred);
    g.setFont(13.0f); g.setColour(juce::Colour(0xffb7bcc6));
    g.drawText(simpleModeButton.getToggleState()?"SIMPLE: COMP couples Threshold + Makeup | ATTACK: 0 = Fast, 5 = Normal, 10 = Slow | Limiter fixed at -0.5 dBFS":"ADVANCED: full DSP access | 4 compressor bands / 3 adjustable crossovers | 5 ms stereo-linked lookahead limiter",20,44,getWidth()-40,20,juce::Justification::centred);

    // Four-second scrolling gain-reduction history. This is GUI-only; DSP is untouched.
    auto graph = juce::Rectangle<float>(75.0f,82.0f,(float)getWidth()-150.0f,185.0f);
    g.setColour(juce::Colour(0xff20242a)); g.fillRoundedRectangle(graph,6.0f); g.setColour(juce::Colour(0xff3a4049)); g.drawRoundedRectangle(graph,6.0f,1.0f);
    g.setFont(10.5f); g.setColour(juce::Colour(0xff8e949f));
    for(int db=0;db<=24;db+=6){ float y=graph.getY()+graph.getHeight()*((float)db/24.0f); g.drawHorizontalLine((int)y,graph.getX(),graph.getRight()); g.drawText(db==0?"0 dB":"-"+juce::String(db),25,(int)y-7,45,14,juce::Justification::centredRight); }
    const std::array<juce::Colour,5> colours { juce::Colour(0xff55b7d9),juce::Colour(0xff73d39b),juce::Colour(0xffd9b455),juce::Colour(0xffb487d9),juce::Colour(0xfff29d49) };
    for(int b=0;b<5;++b){ juce::Path p; for(int n=0;n<historySize;++n){ int idx=(historyWritePos+n)%historySize; float db=juce::jlimit(0.0f,24.0f,grHistory[(size_t)b][(size_t)idx]); float x=graph.getX()+graph.getWidth()*(float)n/(float)(historySize-1); float y=graph.getY()+graph.getHeight()*db/24.0f; if(n==0)p.startNewSubPath(x,y);else p.lineTo(x,y);} g.setColour(colours[(size_t)b]); g.strokePath(p,juce::PathStrokeType(b==4?2.0f:1.5f)); }
    g.setFont(11.0f); for(int b=0;b<5;++b){ g.setColour(colours[(size_t)b]); g.drawText(b<4?"Band "+juce::String(b+1):"Limiter",90+b*115,272,105,16,juce::Justification::centred); }
    g.setColour(juce::Colour(0xff777d88)); g.drawText("Gain Reduction History  |  last 4 seconds",10,64,getWidth()-20,16,juce::Justification::centred);
    g.setColour(outputPeakDb>-0.1f?juce::Colour(0xffff6666):juce::Colour(0xffa9d9e8)); g.setFont(12.0f); g.drawText("Post-Master Peak: "+juce::String(outputPeakDb,1)+" dBFS",getWidth()-280,64,255,16,juce::Justification::centredRight);

    // Existing instantaneous meters retained below the history display.
    const int mt=310,mh=125,mw=70,gap=42,total=5*mw+4*gap,sx=(getWidth()-total)/2; static const char* names[]={"Band 1","Band 2","Band 3","Band 4","Limiter"};
    for(int i=0;i<5;++i){ int x=sx+i*(mw+gap); auto a=juce::Rectangle<float>((float)x,(float)mt,(float)mw,(float)mh); g.setColour(juce::Colour(0xff2a2e35));g.fillRoundedRectangle(a,5); float db=juce::jlimit(0.0f,24.0f,meterDb[(size_t)i]); auto f=a.withY(a.getBottom()-a.getHeight()*db/24.0f).withHeight(a.getHeight()*db/24.0f);g.setColour(colours[(size_t)i]);g.fillRoundedRectangle(f,4);g.setColour(juce::Colours::white);g.setFont(12);g.drawText(names[i],x-8,mt+mh+5,mw+16,16,juce::Justification::centred);g.drawText(juce::String(db,1)+" dB",x-8,mt+mh+21,mw+16,16,juce::Justification::centred); }
    g.setColour(juce::Colour(0xff8e949f));g.setFont(11.5f);g.drawText("Crossovers define the four bands. History display is diagnostic only and does not alter the audio DSP.",20,getHeight()-22,getWidth()-40,17,juce::Justification::centred);
}

void SteveSledgeCompressorAudioProcessorEditor::resized()
{
    simpleModeButton.setBounds(getWidth()-125,18,102,25); const int sw=150,sh=92,gap=24,r1=500,r2=625,total=5*sw+4*gap,x0=(getWidth()-total)/2;
    if(simpleModeButton.getToggleState()){ const int y=535,w=165,h=105,gp=28,t=4*w+3*gp,xx=(getWidth()-t)/2; std::array<juce::Slider*,4>s{&compSlider,&ratioSlider,&attackSlider,&masterSlider};std::array<juce::Label*,4>l{&compLabel,&ratioLabel,&attackLabel,&masterLabel};for(int i=0;i<4;++i){int x=xx+i*(w+gp);l[i]->setBounds(x,y-22,w,20);s[i]->setBounds(x,y,w,h);} }
    else { std::array<juce::Slider*,5>a{&inputSlider,&thresholdSlider,&ratioSlider,&speedSlider,&makeupSlider},b{&xover1Slider,&xover2Slider,&xover3Slider,&ceilingSlider,&masterSlider};std::array<juce::Label*,5>al{&inputLabel,&thresholdLabel,&ratioLabel,&speedLabel,&makeupLabel},bl{&xover1Label,&xover2Label,&xover3Label,&ceilingLabel,&masterLabel};for(int i=0;i<5;++i){int x=x0+i*(sw+gap);al[i]->setBounds(x,r1-20,sw,18);a[i]->setBounds(x,r1,sw,sh);bl[i]->setBounds(x,r2-20,sw,18);b[i]->setBounds(x,r2,sw,sh);} }
}

void SteveSledgeCompressorAudioProcessorEditor::timerCallback()
{
    for(int i=0;i<4;++i) meterDb[(size_t)i]=processor.getBandMeterDb(i); meterDb[4]=processor.getLimiterMeterDb(); outputPeakDb=processor.getOutputPeakDb();
    for(int i=0;i<5;++i) grHistory[(size_t)i][(size_t)historyWritePos]=meterDb[(size_t)i]; historyWritePos=(historyWritePos+1)%historySize;
    const bool s=simpleModeButton.getToggleState(); if(s!=lastSimpleMode){if(lastSimpleMode&&!s)processor.syncAdvancedFromSimple();lastSimpleMode=s;updateModeVisibility();resized();} repaint();
}
