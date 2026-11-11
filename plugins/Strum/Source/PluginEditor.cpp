#include "PluginEditor.h"
#include "dsp/Meters.h"

namespace theme = vs::theme;

namespace
{
    // Every colour here comes straight from vs::theme and knobs/switches from
    // ui/Controls.h, so restyling Theme.h restyles Strum. Theme.h has no font
    // helper yet, so all of this editor's text goes through this one function:
    // the restyle only has to change (or redirect) it here.
    juce::FontOptions uiFont (float size, bool bold = false)
    {
        return vs::font (size, bold);
    }
}

//==============================================================================
void BodyCurveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::bg);
    g.fillRoundedRectangle (r, 6.0f);
    const auto plot = r.reduced (34.0f, 10.0f).withTrimmedBottom (14.0f);

    const auto xFor = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    const auto yFor = [&] (float db) { return plot.getCentreY() - plot.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, db / 12.0f); };

    g.setFont (uiFont (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xFor (f);
        g.setColour (theme::edge);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (theme::dim);
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 2.0f, 40.0f, 12.0f), juce::Justification::centred);
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yFor (db);
        g.setColour (db == 0.0f ? theme::track : theme::edge);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (theme::dim);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (r.getX(), y - 6.0f, 30.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    const auto curve = proc.getActiveCurve();
    const float amount = proc.apvts.getRawParameterValue ("body_amount")->load();
    const bool on = proc.apvts.getRawParameterValue ("body_on")->load() > 0.5f;

    auto pathFor = [&] (float scale)
    {
        juce::Path p;
        for (int i = 0; i <= 200; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 200.0f);
            const float x = xFor (f), y = yFor (scale * curve.at (f));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    // full curve faint, what's applied (x Amount) bright
    g.setColour (theme::track);
    g.strokePath (pathFor (1.0f), juce::PathStrokeType (1.2f));
    g.setColour (on ? theme::accent : theme::dim);
    g.strokePath (pathFor (amount), juce::PathStrokeType (2.2f));

    g.setColour (theme::dim);
    g.setFont (uiFont (11.0f, true));
    g.drawText (proc.isUsingLearned() ? "LEARNED CURVE (mic vs DI)" : "GENERIC PICKUP CURVE (not learned)",
                plot.reduced (6.0f, 2.0f).toNearestInt(), juce::Justification::topLeft);
}

//==============================================================================
StrumEditor::StrumEditor (StrumProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    presetBar = std::make_unique<vs::PresetBar> (proc.presetManager, proc.getFactoryPresetNames(),
                                                 [this] (int i) { proc.applyPreset (i); });
    addAndMakeVisible (*presetBar);
    lastPresetName = proc.presetManager.getCurrentName();

    // Body
    bodyToggle = std::make_unique<vs::ParamToggle> (proc.apvts, "body_on");
    bodyCurve = std::make_unique<vs::ParamChoice> (proc.apvts, "body_curve", "Curve");
    bodyAmount = std::make_unique<vs::ParamKnob> (proc.apvts, "body_amount", "Amount");
    bodyCurve->combo.setTooltip ("Generic: a typical pickup fix, not made from your guitar. Learned: the curve Learn measured from your mic.");
    bodyAmount->slider.setTooltip ("How much of the curve to apply. 100% matches the mic as closely as Learn measured.");
    for (auto* c : std::initializer_list<juce::Component*> { bodyToggle.get(), bodyCurve.get(), bodyAmount.get(), &curveView })
        addAndMakeVisible (c);

    learnBtn.onClick = [this] { proc.toggleLearn(); };
    learnBtn.setTooltip ("Put the DI on this track and send the mic of the same take to Strum's side-chain. "
                         "Press Learn and play for about 30 seconds (both together). Press again to finish early (5 s minimum).");
    addAndMakeVisible (learnBtn);
    forgetBtn.onClick = [this] { proc.forgetLearned(); };
    forgetBtn.setTooltip ("Throw away the learned curve and go back to the generic one");
    addAndMakeVisible (forgetBtn);

    compareBtn.setClickingTogglesState (false);
    compareBtn.onClick = [this]
    {
        auto* param = proc.apvts.getParameter ("compare");
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
    };
    compareBtn.setTooltip ("Hear the untouched guitar at the same loudness, so you're judging the sound, not the volume.");
    addAndMakeVisible (compareBtn);

    auto dbText = [] (float v) { return v > 0.05f ? "-" + juce::String (v, 1) + " dB" : juce::String ("0.0 dB"); };

    addSection ("BOOM", "boom_on", "boom", "Amount",
                "Tames low-end build-up from a mic near the soundhole. Finds the guitar's main low resonance by itself and only cuts it while it's booming.",
                [this, dbText]
                {
                    const float hz = proc.boomHz();
                    return (hz > 0.0f ? "Found " + juce::String (juce::roundToInt (hz)) + " Hz" : juce::String ("Listening for boom"))
                           + "   " + dbText (proc.boomCutDb());
                });
    addSection ("SQUEAK", "squeak_on", "squeak", "Amount",
                "Turns down finger squeaks on slides and chord changes (sudden noise up high with no new note under it). Strums and picked notes are left alone.",
                [this, dbText] { return "Cutting  " + dbText (proc.squeakCutDb()); });
    addSection ("PICK", "pick_on", "pick", "Amount",
                "Softens hard pick attacks (2-6 kHz, only at the start of notes). The ringing part of the note isn't touched.",
                [this, dbText] { return "Cutting  " + dbText (proc.pickCutDb()); });
    addSection ("DOUBLE", "double_on", "width", "Width",
                "Fakes a second take for width. Safe in mono: the copy cancels out in mono, so nothing combs.",
                [] { return juce::String ("Mono-safe"); });
    addSection ("OUTPUT", nullptr, "output", "Output", "Output level", [] { return juce::String(); });

    setSize (1000, 600);
    timerCallback(); // button texts right away, not one tick later
    startTimerHz (30);
}

StrumEditor::~StrumEditor() { setLookAndFeel (nullptr); }

void StrumEditor::addSection (const juce::String& title, const char* toggleId, const char* knobId, const juce::String& knobLabel,
                              const juce::String& tip, std::function<juce::String()> status)
{
    auto s = std::make_unique<Section>();
    s->title = title;
    if (toggleId != nullptr)
    {
        s->toggle = std::make_unique<vs::ParamToggle> (proc.apvts, toggleId);
        addAndMakeVisible (*s->toggle);
    }
    s->knob = std::make_unique<vs::ParamKnob> (proc.apvts, knobId, knobLabel);
    s->knob->slider.setTooltip (tip);
    addAndMakeVisible (*s->knob);
    s->status = std::move (status);
    sections.push_back (std::move (s));
}

void StrumEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (54);
    header.removeFromLeft (150); // title, drawn in paint()
    presetBar->setBounds (header.removeFromLeft (350).withSizeKeepingCentre (350, 28));
    header.removeFromLeft (20);
    compareBtn.setBounds (header.removeFromLeft (200).withSizeKeepingCentre (200, 30));
    header.removeFromLeft (16);
    meterArea = header;

    r.removeFromTop (8);
    bodyArea = r.removeFromTop (290);
    r.removeFromTop (10);

    {
        auto inner = bodyArea.reduced (12, 8);
        auto left = inner.removeFromLeft (300);
        inner.removeFromLeft (12);
        curveView.setBounds (inner);

        auto top = left.removeFromTop (24);
        bodyToggle->setBounds (top.removeFromRight (44));
        left.removeFromTop (8);
        auto ctl = left.removeFromTop (110);
        bodyCurve->setBounds (ctl.removeFromLeft (150).reduced (4, 20));
        bodyAmount->setBounds (ctl);
        left.removeFromTop (8);
        auto btns = left.removeFromTop (36);
        forgetBtn.setBounds (btns.removeFromRight (80));
        btns.removeFromRight (8);
        learnBtn.setBounds (btns);
        left.removeFromTop (8);
        progressArea = left.removeFromTop (8);
        left.removeFromTop (6);
        bodyStatusArea = left;
    }

    const int gap = 10;
    const int count = (int) sections.size();
    const int w = (r.getWidth() - gap * (count - 1)) / count;
    for (auto& s : sections)
    {
        s->area = r.removeFromLeft (w);
        r.removeFromLeft (gap);
        auto inner = s->area.reduced (8);
        auto top = inner.removeFromTop (24);
        if (s->toggle != nullptr) s->toggle->setBounds (top.removeFromRight (44));
        s->statusArea = inner.removeFromBottom (18);
        inner.removeFromTop (4);
        s->knob->setBounds (inner.withSizeKeepingCentre (juce::jmin (inner.getWidth(), 130), inner.getHeight()));
    }
}

juce::String StrumEditor::bodyStatus() const
{
    const auto& L = proc.getLearner();
    const bool sc = proc.sidechainLive.load();
    if (L.isLearning())
    {
        if (L.getStatus() == vs::BodyLearner::noMic || ! sc)
            return "No side-chain yet. Send the mic track to Strum's side-chain input, then keep playing.";
        if (proc.sidechainDb.load() < -65.0f)
            return "The side-chain is silent. Check the mic track's send to Strum.";
        return "Listening. Keep playing, DI and mic together. Press Learn again to finish early.";
    }
    if (L.getStatus() == vs::BodyLearner::tooShort)
        return "Not enough playing to learn from (it needs 5 s of DI and mic together). Try again.";

    const auto& c = proc.getLearned();
    if (proc.isUsingLearned())
        return "Learned from " + juce::String (juce::roundToInt (c.seconds)) + " s of playing. The mic was "
               + juce::String (std::abs (c.levelDb), 1) + " dB " + (c.levelDb >= 0 ? "louder" : "quieter")
               + " than the DI (that level difference isn't applied). Save a preset to reuse this guitar.";
    if (c.valid)
        return "Using the generic curve. Switch Curve to Learned to use what Learn measured.";
    if ((int) proc.apvts.getRawParameterValue ("body_curve")->load() == 1)
        return "Nothing learned yet, so the generic curve is playing. Press Learn with a mic on the side-chain.";
    return "Generic pickup curve: a typical fix, not made from your guitar. For a real match, Learn from a mic.";
}

void StrumEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    auto title = getLocalBounds().reduced (14).removeFromTop (54).removeFromLeft (150);
    vs::drawTitle (g, title, "STRUM");

    auto panel = [&] (juce::Rectangle<int> a, const juce::String& t, bool on)
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (a.toFloat(), vs::theme::radius);
        g.setColour (theme::edge);
        g.drawRoundedRectangle (a.toFloat().reduced (0.5f), vs::theme::radius, 1.0f);
        g.setColour (on ? theme::accent : theme::dim);
        g.setFont (uiFont (13.0f, true));
        g.drawText (t, a.reduced (12, 8).removeFromTop (24), juce::Justification::centredLeft);
    };

    panel (bodyArea, "BODY   (pickup to mic)", bodyToggle->button.getToggleState());

    // Learn progress + status
    {
        const auto& L = proc.getLearner();
        auto bar = progressArea.toFloat();
        g.setColour (theme::track);
        g.fillRoundedRectangle (bar, 3.0f);
        const float prog = L.isLearning() ? L.getProgress() : (proc.getLearned().valid ? 1.0f : 0.0f);
        g.setColour (L.isLearning() ? theme::accent : theme::green);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * prog), 3.0f);

        g.setColour (theme::dim);
        g.setFont (uiFont (12.0f));
        g.drawFittedText (bodyStatus(), bodyStatusArea, juce::Justification::topLeft, 4, 1.0f);
    }

    for (auto& s : sections)
    {
        const bool on = s->toggle == nullptr || s->toggle->button.getToggleState();
        panel (s->area, s->title, on);
        g.setColour (on ? theme::text : theme::dim);
        g.setFont (uiFont (11.0f));
        g.drawText (s->status(), s->statusArea, juce::Justification::centred);
    }

    auto m = meterArea;
    const int mw = (m.getWidth() - 12) / 2;
    drawMeter (g, m.removeFromLeft (mw), "IN", mIn);
    m.removeFromLeft (12);
    drawMeter (g, m.removeFromLeft (mw), "OUT", mOut);
}

void StrumEditor::drawMeter (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& name, float value)
{
    r = r.withSizeKeepingCentre (r.getWidth(), 40);
    auto top = r.removeFromTop (18);
    g.setColour (theme::dim);
    g.setFont (uiFont (11.0f, true));
    g.drawText (name, top, juce::Justification::centredLeft);
    g.setColour (theme::text);
    g.drawText (value > -99.0f ? juce::String (value, 1) + " dB" : juce::String ("--"), top, juce::Justification::centredRight);

    auto bar = r.removeFromTop (10).toFloat();
    g.setColour (theme::track);
    g.fillRoundedRectangle (bar, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (value + 60.0f) / 60.0f);
    g.setColour (value > -0.5f ? theme::red : theme::green);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
}

void StrumEditor::timerCallback()
{
    vs::meters::step (mIn, proc.inputDb.load(), vs::meters::level);
    vs::meters::step (mOut, proc.outputDb.load(), vs::meters::level);

    const auto& L = proc.getLearner();
    if (L.isLearning())
        learnBtn.setButtonText ("LEARNING  " + juce::String (juce::roundToInt (L.getProgress() * vs::BodyLearner::targetSeconds))
                                + " / " + juce::String ((int) vs::BodyLearner::targetSeconds) + " s   (click to finish)");
    else
        learnBtn.setButtonText (proc.getLearned().valid ? "LEARN AGAIN" : "LEARN FROM MIC");
    learnBtn.setToggleState (L.isLearning(), juce::dontSendNotification);
    forgetBtn.setEnabled (proc.getLearned().valid);

    const bool original = proc.apvts.getRawParameterValue ("compare")->load() > 0.5f;
    compareBtn.setButtonText (original ? "HEARING: ORIGINAL" : "HEARING: STRUM");
    compareBtn.setToggleState (! original, juce::dontSendNotification);

    if (proc.presetManager.getCurrentName() != lastPresetName)
    {
        lastPresetName = proc.presetManager.getCurrentName();
        presetBar->refresh();
    }
    repaint();
}
