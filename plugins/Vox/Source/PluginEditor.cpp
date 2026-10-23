#include "PluginEditor.h"

namespace colours
{
    const juce::Colour bg     { 0xff15171c };
    const juce::Colour panel  { 0xff1f232b };
    const juce::Colour edge   { 0xff2c313b };
    const juce::Colour text   { 0xffe6e8ec };
    const juce::Colour dim    { 0xff8a919e };
    const juce::Colour accent { 0xffe8a33d };
    const juce::Colour track  { 0xff343a46 };
    const juce::Colour green  { 0xff5cc98a };
    const juce::Colour red    { 0xffe0605a };
}

//==============================================================================
VocalSuiteLookAndFeel::VocalSuiteLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::dim);
    setColour (juce::ComboBox::backgroundColourId, colours::track);
    setColour (juce::ComboBox::outlineColourId, colours::edge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
}

void VocalSuiteLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = 4.0f;

    juce::Path back;
    back.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (back, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar knobs (EQ) fill from the middle, others from the left
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 && std::abs (slider.getMinimum() + slider.getMaximum()) < 0.001;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (slider.isEnabled() ? colours::accent : colours::dim);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - thick * 2.5f;
    g.setColour (colours::panel.brighter (0.15f));
    g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -inner, 3.0f, inner * 0.55f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void VocalSuiteLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::accent : colours::track);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    g.setColour (on ? colours::bg : colours::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
}

//==============================================================================
VocalSuiteEditor::VocalSuiteEditor (VocalSuiteProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    addSection ("RIDE",     0, "ride_on",  { "ride_target", "ride_range", "ride_speed" });
    addSection ("CLEAN",    0, "clean_on", { "hpf", "gate", "deess", "deess_freq" });
    addSection ("DYNAMICS", 0, "dyn_on",   { "comp", "comp_style" });
    addSection ("OUTPUT",   0, nullptr,    { "output" });
    addSection ("TONE",     1, "tone_on",  { "drive", "low", "body", "presence", "air" });
    addSection ("SPACE",    1, "space_on", { "verb", "verb_size", "delay", "delay_time", "delay_fb" });
    addSection ("TUNE",     2, "tune_on",  { "key", "scale", "tune_speed", "tune_amount" });
    addSection ("HARMONY",  2, "harm_on",  { "h1_int", "h1_level", "h1_pan", "h2_int", "h2_level", "h2_pan", "formant", "humanize" });

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (1120, 660);
    startTimerHz (30);
}

VocalSuiteEditor::~VocalSuiteEditor()
{
    setLookAndFeel (nullptr);
}

void VocalSuiteEditor::addSection (const juce::String& title, int row, const char* toggleId, std::initializer_list<const char*> ids)
{
    auto s = std::make_unique<Section>();
    s->title = title;
    s->row = row;

    if (toggleId != nullptr)
    {
        s->toggle = std::make_unique<juce::ToggleButton>();
        addAndMakeVisible (*s->toggle);
        s->toggleAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, toggleId, *s->toggle);
    }

    for (auto* id : ids)
    {
        Control c;
        auto* param = proc.apvts.getParameter (id);

        c.label = std::make_unique<juce::Label> ("", param->getName (20));
        c.label->setJustificationType (juce::Justification::centred);
        c.label->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (*c.label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            c.combo = std::make_unique<juce::ComboBox>();
            c.combo->addItemList (choice->choices, 1);
            addAndMakeVisible (*c.combo);
            c.comboAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, *c.combo);
        }
        else
        {
            c.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            c.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
            addAndMakeVisible (*c.slider);
            c.sliderAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, *c.slider);
            // double-click a knob to reset it
            c.slider->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        }

        s->controls.push_back (std::move (c));
    }

    sections.push_back (std::move (s));
}

void VocalSuiteEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (54);

    header.removeFromLeft (220); // title, drawn in paint()
    presetBox.setBounds (header.removeFromLeft (190).withSizeKeepingCentre (190, 28));
    header.removeFromLeft (20);
    meterArea = header;

    r.removeFromTop (8);
    const int gap = 10;
    const int rowH = (r.getHeight() - gap * 2) / 3;

    for (int row = 0; row < 3; ++row)
    {
        auto rowArea = r.removeFromTop (rowH);
        r.removeFromTop (gap);

        int total = 0, count = 0;
        for (auto& s : sections)
            if (s->row == row) { total += (int) s->controls.size(); ++count; }

        const int usable = rowArea.getWidth() - gap * (count - 1);
        for (auto& s : sections)
        {
            if (s->row != row) continue;
            const int w = usable * (int) s->controls.size() / total;
            s->area = rowArea.removeFromLeft (w);
            rowArea.removeFromLeft (gap);

            auto inner = s->area.reduced (8);
            auto top = inner.removeFromTop (24);
            if (s->toggle != nullptr)
                s->toggle->setBounds (top.removeFromRight (44));

            inner.removeFromTop (6);
            const int cw = inner.getWidth() / (int) s->controls.size();
            for (auto& c : s->controls)
            {
                auto cell = inner.removeFromLeft (cw);
                c.label->setBounds (cell.removeFromBottom (18));
                if (c.slider != nullptr)
                    c.slider->setBounds (cell);
                else
                    c.combo->setBounds (cell.withSizeKeepingCentre (juce::jmin (cw - 8, 110), 26).translated (0, -10));
            }
        }
    }
}

void VocalSuiteEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    auto title = getLocalBounds().reduced (14).removeFromTop (54).removeFromLeft (220);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VOX", title.removeFromTop (32), juce::Justification::bottomLeft);
    g.setColour (colours::accent);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("SICK & SAD", title, juce::Justification::topLeft);

    for (auto& s : sections)
    {
        const bool on = s->toggle == nullptr || s->toggle->getToggleState();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (s->area.toFloat(), 8.0f);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (s->area.toFloat().reduced (0.5f), 8.0f, 1.0f);

        g.setColour (on ? colours::accent : colours::dim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (s->title, s->area.reduced (12, 8).removeFromTop (24), juce::Justification::centredLeft);
    }

    // Live note readout: what the singer is hitting and how far Tune moves it
    auto m = meterArea;
    {
        auto noteArea = m.removeFromLeft (120).withSizeKeepingCentre (120, 40);
        g.setColour (colours::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("NOTE", noteArea.removeFromTop (18), juce::Justification::centredLeft);

        juce::String txt = "--";
        if (proc.isPitchActive() && shownNote > 0.0f)
        {
            const int nearest = juce::roundToInt (shownNote);
            const int cents = juce::roundToInt ((shownNote - (float) nearest) * 100.0f);
            txt = vs::scales::keyNames[((nearest % 12) + 12) % 12] + juce::String (nearest / 12 - 1)
                  + "  " + (cents >= 0 ? "+" : "") + juce::String (cents) + "c";
        }
        else if (! proc.isPitchActive())
        {
            txt = "Tune/Harmony off";
        }
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (proc.isPitchActive() ? 15.0f : 11.0f, juce::Font::bold));
        g.drawText (txt, noteArea, juce::Justification::centredLeft);
        m.removeFromLeft (12);
    }

    const int mw = (m.getWidth() - 3 * 12) / 4;
    drawMeter (g, m.removeFromLeft (mw), "RIDER", mRide, -12, 12, true, colours::accent);   m.removeFromLeft (12);
    drawMeter (g, m.removeFromLeft (mw), "DE-ESS", -mDeess, -15, 0, false, colours::red);   m.removeFromLeft (12);
    drawMeter (g, m.removeFromLeft (mw), "COMP", -mComp, -20, 0, false, colours::red);      m.removeFromLeft (12);
    drawMeter (g, m.removeFromLeft (mw), "OUT", mOut, -60, 0, false, colours::green);
}

void VocalSuiteEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& name,
                                  float value, float lo, float hi, bool centred, juce::Colour colour)
{
    r = r.withSizeKeepingCentre (r.getWidth(), 40);
    auto top = r.removeFromTop (18);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (name, top, juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.drawText ((value > 0.05f ? "+" : "") + juce::String (value, 1) + " dB", top, juce::Justification::centredRight);

    auto bar = r.removeFromTop (10).toFloat();
    g.setColour (colours::track);
    g.fillRoundedRectangle (bar, 3.0f);

    const auto toX = [&] (float v) { return bar.getX() + bar.getWidth() * (juce::jlimit (lo, hi, v) - lo) / (hi - lo); };
    // GR meters grow from the right (0 dB) leftward; others from the left / centre
    const float from = centred ? toX (0.0f) : (hi == 0.0f && name != "OUT" ? bar.getRight() : bar.getX());
    const float to = toX (value);
    g.setColour (colour);
    g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (from, to), bar.getY(), juce::jmax (from, to), bar.getBottom()), 3.0f);
}

void VocalSuiteEditor::timerCallback()
{
    // Fast attack, slow fall so the meters are readable
    auto ballistics = [] (float& shown, float target, bool upIsAttack)
    {
        const bool attack = upIsAttack ? target > shown : target < shown;
        shown += (attack ? 0.6f : 0.12f) * (target - shown);
    };

    ballistics (mIn, proc.inputDb.load(), true);
    ballistics (mOut, proc.outputDb.load(), true);
    ballistics (mDeess, proc.deEssDb().load(), true);
    ballistics (mComp, proc.compDb().load(), true);
    mRide += 0.3f * (proc.riderDb().load() - mRide);

    // hold the last note briefly through gaps so the readout doesn't flicker
    const float note = proc.detectedMidi().load();
    if (note > 0.0f) { shownNote = note; noteHold = 10; }
    else if (noteHold > 0 && --noteHold == 0) shownNote = -1.0f;

    // Keep section titles in sync with their on/off switch
    repaint();
}
