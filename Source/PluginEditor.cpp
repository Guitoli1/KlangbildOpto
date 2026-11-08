#include "PluginEditor.h"

using namespace vintage;

//==============================================================================
OptoAudioProcessorEditor::OptoAudioProcessorEditor (OptoAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    prKnob   = &addKnob ("pr",       "PEAK REDUCTION", true);
    gainKnob = &addKnob ("gain",     "GAIN",           true);
    emphKnob = &addKnob ("emphasis", "EMPHASIS",       false);
    mixKnob  = &addKnob ("mix",      "MIX",            false);

    setupSwitch (limitSwitch, "limit", "LIMIT", "COMPRESS", lampRed);
    setupSwitch (tubeSwitch,  "tube",  "AN", "AUS", lampAmber);
    meterSwitch.setButtonText ("x");
    meterSwitch.getProperties().set ("onText", "AUSGANG");
    meterSwitch.getProperties().set ("offText", "GR");
    meterSwitch.getProperties().set ("lamp", (int) lampAmber.getARGB());
    addAndMakeVisible (meterSwitch);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, "meter", meterSwitch));

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    addAndMakeVisible (vu);

    rack::styleToggle (limitSwitch);
    rack::styleToggle (tubeSwitch);
    rack::styleToggle (meterSwitch);
    setSize (rackW, rackH);
    startTimerHz (60);
}

OptoAudioProcessorEditor::~OptoAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

OptoAudioProcessorEditor::Knob& OptoAudioProcessorEditor::addKnob (const juce::String& paramID, const juce::String& name, bool big, const juce::String& steps)
{
    auto k = std::make_unique<Knob>();
    k->name = name;
    k->big = big;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k->slider.setMouseDragSensitivity (big ? 320 : 220);
    if (steps.isNotEmpty())
        k->slider.getProperties().set ("stepLabels", steps);
    addAndMakeVisible (k->slider);
    k->attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k->slider);
    if (auto* rp = proc.apvts.getParameter (paramID))
        k->slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void OptoAudioProcessorEditor::setupSwitch (juce::ToggleButton& b, const juce::String& paramID,
                                            const juce::String& onText, const juce::String& offText, juce::Colour lamp)
{
    b.setButtonText ("x");
    b.getProperties().set ("onText", onText);
    b.getProperties().set ("offText", offText);
    b.getProperties().set ("lamp", (int) lamp.getARGB());
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, paramID, b));
}

void OptoAudioProcessorEditor::timerCallback()
{
    constexpr float dt = 1.0f / 60.0f;
    const bool showOutput = meterSwitch.getToggleState();
    if (showOutput)
    {
        vu.setLabel ("AUSGANG");
        vu.setRestAtZeroVu (false);
        vu.update (proc.vuOut.load(), proc.peakOut.exchange (0.0f), dt);
    }
    else
    {
        // Pegelreduktion: Zeiger steht ohne Kompression auf 0 VU und wandert nach links
        proc.peakOut.store (0.0f);
        vu.setLabel ("GR");
        const float gr = proc.grDb.load();
        const float avg = juce::Decibels::decibelsToGain (-gr - 18.0f) / 1.1107f;
        vu.setRestAtZeroVu (true);
        vu.update (avg, 0.0f, dt);
    }

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
void OptoAudioProcessorEditor::renderPanel()
{
    panelImage = rack::renderPanel (rackW, rackH, 17);
    juce::Graphics g (panelImage);
    const char* titles[] = { "KOMPRESSION", "REDUKTION", "PEGEL", "SEITENKETTE", "PARALLEL" };
    for (size_t i = 0; i < sections.size(); ++i)
        rack::drawSection (g, sections[i], rack::u8 (titles[i]), lnf);
    rack::drawBezel (g, vuBezel);
    for (auto& k : knobs)
        rack::drawKnobLabel (g, k->slider, k->name, lnf, k->big ? 14.0f : 12.0f);
    const char* names[] = { "CHARAKTER", "R\xc3\x96HRE", "ANZEIGE" };
    for (size_t i = 0; i < 3; ++i)
        rack::drawLabel (g, rack::u8 (names[i]), switchLabels[i].toFloat(), lnf, 11.0f, juce::Justification::centredLeft);
    rack::drawNameplate (g, nameplate, "OPTO", rack::u8 ("OPTO-KOMPRESSOR \xc2\xb7 R\xc3\x96HRE"), lnf);
    rack::drawLabel (g, "PROGRAMM", presetBox.getBounds().toFloat().withY ((float) presetBox.getY() - 16.0f).withHeight (14.0f), lnf, 11.0f, juce::Justification::centredLeft);
    rack::drawTypeTag (g, rack::u8 ("TYP KO-1  \xc2\xb7  NR. 0604"), tagArea, lnf);
}

void OptoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (panelImage, 0, 0);
}

void OptoAudioProcessorEditor::resized()
{
    const int top = 30, bottom = rackH - 34;
    sections[0] = { 72, top, 240, 262 };
    sections[1] = { 324, top, 460, 262 };
    sections[2] = { 796, top, 234, 262 };
    sections[3] = { 72, top + 282, 473, bottom - top - 282 };
    sections[4] = { 557, top + 282, 473, bottom - top - 282 };

    prKnob->slider.setBounds (juce::Rectangle<int> (220, 220).withCentre ({ sections[0].getCentreX(), top + 132 }));
    gainKnob->slider.setBounds (juce::Rectangle<int> (220, 220).withCentre ({ sections[2].getCentreX(), top + 132 }));
    vuBezel = juce::Rectangle<int> (380, 236).withCentre ({ sections[1].getCentreX(), top + 134 });
    vu.setBounds (vuBezel.reduced (10));

    emphKnob->slider.setBounds (juce::Rectangle<int> (150, 150).withCentre ({ sections[3].getCentreX(), sections[3].getY() + 93 }));
    mixKnob->slider.setBounds (juce::Rectangle<int> (150, 150).withCentre ({ sections[4].getCentreX(), sections[4].getY() + 93 }));

    const int colX = 1046, colW = rackW - 56 - 16 - colX;
    nameplate = { colX, top - 8, colW, 88 };
    presetBox.setBounds (colX, top + 112, colW, 28);
    juce::ToggleButton* sws[] = { &limitSwitch, &tubeSwitch, &meterSwitch };
    for (int i = 0; i < 3; ++i)
    {
        const int y = top + 168 + i * 80;
        switchLabels[(size_t) i] = { colX + 4, y, colW, 14 };
        sws[i]->setBounds (colX + 4, y + 18, 150, 40);
    }
    tagArea = { colX - 60, rackH - 28, colW + 60, 16 };
    renderPanel();
}
