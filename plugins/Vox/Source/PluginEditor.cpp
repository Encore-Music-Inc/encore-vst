#include "PluginEditor.h"

namespace colours = vs::theme;

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

    presetBar = std::make_unique<vs::PresetBar> (proc.presetManager, proc.getFactoryPresetNames(),
                                                 [this] (int i) { proc.applyPreset (i); });
    addAndMakeVisible (*presetBar);
    lastPresetName = proc.presetManager.getCurrentName();

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
            vs::styleKnob (*c.slider);
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

    header.removeFromLeft (150); // title, drawn in paint()
    presetBar->setBounds (header.removeFromLeft (350).withSizeKeepingCentre (350, 28));
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

    auto title = getLocalBounds().reduced (14).removeFromTop (54).removeFromLeft (150);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VOX", title.removeFromTop (32), juce::Justification::bottomLeft);
    g.setColour (colours::accent);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("ENCORE:", title, juce::Justification::topLeft);

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

    // the project or another instance may have changed the preset
    if (proc.presetManager.getCurrentName() != lastPresetName)
    {
        lastPresetName = proc.presetManager.getCurrentName();
        presetBar->refresh();
    }

    // hold the last note briefly through gaps so the readout doesn't flicker
    const float note = proc.detectedMidi().load();
    if (note > 0.0f) { shownNote = note; noteHold = 10; }
    else if (noteHold > 0 && --noteHold == 0) shownNote = -1.0f;

    // Keep section titles in sync with their on/off switch
    repaint();
}
