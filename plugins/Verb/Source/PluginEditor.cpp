#include "PluginEditor.h"
#include "dsp/Meters.h"

namespace theme = vs::theme;

VerbEditor::VerbEditor (VerbProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    presetBar = std::make_unique<vs::PresetBar> (proc.presetManager, proc.getFactoryPresetNames(),
                                                 [this] (int i) { proc.applyPreset (i); });
    addAndMakeVisible (*presetBar);
    lastPresetName = proc.presetManager.getCurrentName();

    modes = std::make_unique<vs::ChoiceButtons> (proc.apvts, "mode");
    addAndMakeVisible (*modes);

    for (auto* id : { "mix", "predelay", "decay", "size", "shimmer", "tone", "lowcut", "width", "mod", "duck" })
        addAndMakeVisible (knobs.add (new vs::ParamKnob (proc.apvts, id)));

    knobs[4]->slider.setTooltip ("Octave-up shimmer in the tail. Made for Huge mode, works in all of them.");
    knobs[8]->slider.setTooltip ("Slow pitch movement inside the tail. More = lusher, less = cleaner.");
    knobs[9]->slider.setTooltip ("Turns the reverb down while the dry sound plays, up in the gaps.");

    setSize (820, 500);
    startTimerHz (30);
}

VerbEditor::~VerbEditor() { setLookAndFeel (nullptr); }

void VerbEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (54);
    header.removeFromLeft (150);
    presetBar->setBounds (header.removeFromLeft (350).withSizeKeepingCentre (350, 28));

    r.removeFromTop (8);
    modes->setBounds (r.removeFromTop (34));
    r.removeFromTop (10);
    display = r.removeFromTop (130);
    r.removeFromTop (10);

    const int rowH = r.getHeight() / 2;
    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = r.removeFromTop (rowH).reduced (0, 4);
        const int w = rowArea.getWidth() / 5;
        for (int k = 0; k < 5; ++k)
            knobs[row * 5 + k]->setBounds (rowArea.removeFromLeft (w).reduced (6, 0));
    }
}

void VerbEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    auto title = getLocalBounds().reduced (14).removeFromTop (54).removeFromLeft (150);
    vs::drawTitle (g, title, "VERB");

    g.setColour (theme::panel);
    g.fillRoundedRectangle (display.toFloat(), vs::theme::radius);
    g.setColour (theme::edge);
    g.drawRoundedRectangle (display.toFloat().reduced (0.5f), vs::theme::radius, 1.0f);
    drawShape (g, display.toFloat().reduced (14.0f, 12.0f));
}

// A picture of the reverb you've dialled in: the pre-delay gap, early
// reflections (in the modes that have them, from the engine), then the decay to -60 dB.
void VerbEditor::drawShape (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto value = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const int mode = (int) value ("mode");
    const float pre = value ("predelay") / 1000.0f, decay = value ("decay");
    const float total = std::max (1.0f, (pre + decay) * 1.15f);

    auto meter = r.removeFromRight (14.0f);
    r.removeFromRight (12.0f);
    auto label = r.removeFromTop (16.0f);

    g.setColour (theme::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    static const char* names[] { "ROOM", "CHAMBER", "PLATE", "HALL", "HUGE", "SPRING" };
    g.drawText (juce::String (names[juce::jlimit (0, 5, mode)]) + "   " + juce::String (decay, decay < 10 ? 2 : 1) + " s decay   "
                    + juce::String (juce::roundToInt (pre * 1000.0f)) + " ms pre-delay",
                label, juce::Justification::centredLeft);
    g.drawText (juce::String (total, 1) + " s", label, juce::Justification::centredRight);

    auto toX = [&] (float t) { return r.getX() + r.getWidth() * t / total; };
    auto toY = [&] (float db) { return r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 60.0f); };

    // grid
    g.setColour (theme::edge);
    for (float db : { -20.0f, -40.0f })
        g.drawHorizontalLine ((int) toY (db), r.getX(), r.getRight());

    // decay envelope
    juce::Path tail;
    tail.startNewSubPath (toX (pre), r.getBottom());
    tail.lineTo (toX (pre), toY (mode == 5 ? -3.0f : -6.0f));
    const int steps = 80;
    for (int i = 1; i <= steps; ++i)
    {
        const float t = decay * (float) i / (float) steps;
        tail.lineTo (toX (pre + t), toY (-6.0f - 54.0f * t / decay));
    }
    tail.lineTo (toX (pre + decay), r.getBottom());
    tail.closeSubPath();
    g.setColour (theme::accent.withAlpha (0.25f));
    g.fillPath (tail);
    g.setColour (theme::accent);
    g.strokePath (tail, juce::PathStrokeType (1.5f));

    // early reflections, as the engine plays them (each line at its level)
    const auto er = vs::ReverbEngine::earlyReflections (mode, value ("size"));
    if (er.level > 0.0f)
    {
        g.setColour (theme::text.withAlpha (0.7f));
        for (const auto& t : er.taps)
        {
            const float x = toX (pre + t.ms / 1000.0f);
            g.drawVerticalLine ((int) x, toY (vs::ampToDb (er.level * std::max (t.gainL, t.gainR))), r.getBottom());
        }
    }

    // wet output meter
    g.setColour (theme::track);
    g.fillRoundedRectangle (meter, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (shownWet + 60.0f) / 60.0f);
    g.setColour (theme::green);
    g.fillRoundedRectangle (meter.withTop (meter.getBottom() - meter.getHeight() * frac), 3.0f);
}

void VerbEditor::timerCallback()
{
    const float w = proc.wetDb.load();
    vs::meters::step (shownWet, w, vs::meters::level);

    if (proc.presetManager.getCurrentName() != lastPresetName)
    {
        lastPresetName = proc.presetManager.getCurrentName();
        presetBar->refresh();
    }
    repaint (display);
}
