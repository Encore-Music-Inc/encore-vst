#include "PluginEditor.h"

namespace theme = vs::theme;

MasterEditor::MasterEditor (MasterProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    presetBar = std::make_unique<vs::PresetBar> (proc.presetManager, proc.getFactoryPresetNames(),
                                                 [this] (int i) { proc.applyPreset (i); });
    addAndMakeVisible (*presetBar);
    lastPresetName = proc.presetManager.getCurrentName();

    styles = std::make_unique<vs::ChoiceButtons> (proc.apvts, "style");
    targets = std::make_unique<vs::ChoiceButtons> (proc.apvts, "target");
    styles->setRows (1);
    addAndMakeVisible (*styles);
    addAndMakeVisible (*targets);

    input = std::make_unique<vs::ParamKnob> (proc.apvts, "input");
    intensity = std::make_unique<vs::ParamKnob> (proc.apvts, "intensity");
    input->slider.setTooltip ("Turn the mix up or down going in. Usually leave at 0: Listen sets the level for you.");
    intensity->slider.setTooltip ("How much of the style to apply. Loudness stays at the target either way.");
    addAndMakeVisible (*input);
    addAndMakeVisible (*intensity);

    listenBtn.onClick = [this] { proc.requestLearn(); };
    listenBtn.setTooltip ("Play the loudest part of the song (the chorus) for about 20 seconds. Master learns its level and tone, then locks.");
    addAndMakeVisible (listenBtn);

    compareBtn.setClickingTogglesState (false);
    compareBtn.onClick = [this]
    {
        auto* param = proc.apvts.getParameter ("compare");
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
    };
    compareBtn.setTooltip ("Hear the original mix at the same loudness, so you're judging the sound, not the volume.");
    addAndMakeVisible (compareBtn);

    advancedBtn.onClick = [this] { setAdvanced (! advanced); };
    addAndMakeVisible (advancedBtn);
    for (auto* id : { "tone_low", "tone_lowmid", "tone_mid", "tone_pres", "tone_air", "glue", "width", "sat", "ceiling" })
        addChildComponent (advKnobs.add (new vs::ParamKnob (proc.apvts, id)));

    setSize (900, 560);
    startTimerHz (30);
}

MasterEditor::~MasterEditor() { setLookAndFeel (nullptr); }

void MasterEditor::setAdvanced (bool show)
{
    advanced = show;
    advancedBtn.setToggleState (show, juce::dontSendNotification);
    for (auto* k : advKnobs) k->setVisible (show);
    setSize (900, show ? 740 : 560);
}

void MasterEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (54);
    header.removeFromLeft (170);
    presetBar->setBounds (header.removeFromLeft (350).withSizeKeepingCentre (350, 28));
    advancedBtn.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 28));

    r.removeFromTop (8);
    auto styleArea = r.removeFromTop (76);
    // two rows of four style buttons
    {
        auto row1 = styleArea.removeFromTop (36), row2 = styleArea.withTrimmedTop (4);
        styles->setBounds (row1.getUnion (row2));
    }
    blurbArea = r.removeFromTop (24);
    r.removeFromTop (8);

    auto main = r.removeFromTop (300);
    meterArea = main.removeFromRight (300);
    main.removeFromRight (12);

    auto left = main;
    listenBtn.setBounds (left.removeFromTop (64));
    left.removeFromTop (10);
    compareBtn.setBounds (left.removeFromTop (40));
    left.removeFromTop (14);
    targets->setBounds (left.removeFromTop (32));
    left.removeFromTop (8);
    const int kw = left.getWidth() / 2;
    input->setBounds (left.removeFromLeft (kw).reduced (20, 0));
    intensity->setBounds (left.reduced (20, 0));

    r.removeFromTop (12);
    advArea = r;
    if (advanced)
    {
        auto a = advArea.reduced (10, 30);
        const int w = a.getWidth() / advKnobs.size();
        for (auto* k : advKnobs) k->setBounds (a.removeFromLeft (w).reduced (2, 0));
    }
}

void MasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    auto title = getLocalBounds().reduced (14).removeFromTop (54).removeFromLeft (170);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("MASTER", title.removeFromTop (32), juce::Justification::bottomLeft);
    g.setColour (theme::accent);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("ENCORE:", title, juce::Justification::topLeft);

    const int st = (int) proc.apvts.getRawParameterValue ("style")->load();
    g.setColour (theme::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (juce::String (vs::MasterChain::style (st).name) + ":  " + vs::MasterChain::style (st).blurb, blurbArea, juce::Justification::centred);

    drawMeters (g, meterArea);

    if (advanced)
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (advArea.toFloat(), 8.0f);
        g.setColour (theme::accent);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("ADVANCED   (tone trims add to what Master chose; the bars show the total)", advArea.reduced (12, 8).removeFromTop (20),
                    juce::Justification::centredLeft);

        // live total tone moves under the five tone knobs
        for (int b = 0; b < vs::MasterChain::numTone; ++b)
        {
            const auto k = advKnobs[b]->getBounds().toFloat();
            const float db = proc.toneNow[b].load();
            auto bar = juce::Rectangle<float> (k.getCentreX() - 30.0f, k.getBottom() + 6.0f, 60.0f, 6.0f);
            g.setColour (theme::track);
            g.fillRoundedRectangle (bar, 3.0f);
            const float frac = juce::jlimit (-1.0f, 1.0f, db / 8.0f);
            g.setColour (db >= 0 ? theme::accent : theme::blue);
            const float cx = bar.getCentreX();
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (cx, cx + frac * 30.0f), bar.getY(), std::max (cx, cx + frac * 30.0f), bar.getBottom()));
            g.setColour (theme::dim);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText ((db > 0.05f ? "+" : "") + juce::String (db, 1), bar.translated (0, 8).withHeight (12), juce::Justification::centred);
        }
    }
}

void MasterEditor::drawMeters (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    auto r = area.reduced (16, 12);

    auto big = [&] (const juce::String& label, float lufs, juce::Colour c, float h)
    {
        auto row = r.removeFromTop ((int) h);
        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (label, row.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (juce::FontOptions (h - 22.0f, juce::Font::bold));
        g.drawText (lufs > -70.0f ? juce::String (lufs, 1) : juce::String ("--"), row, juce::Justification::centredLeft);
        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("LUFS", row, juce::Justification::bottomRight);
    };

    const float target = std::array<float, 3> { -14.0f, -11.0f, -9.0f }[(size_t) juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("target")->load())];
    const float st = proc.outShortTerm.load();
    const juce::Colour stCol = st < -70.0f ? theme::dim : std::abs (st - target) < 1.5f ? theme::green : theme::text;
    big ("LOUDNESS NOW (short-term)", st, stCol, 64.0f);
    r.removeFromTop (6);
    big ("WHOLE SONG SO FAR (integrated)", proc.outIntegrated(), theme::text, 52.0f);
    r.removeFromTop (8);

    // true peak (click to reset)
    peakArea = r.removeFromTop (34);
    {
        auto row = peakArea;
        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("TRUE PEAK (click to reset)", row.removeFromTop (16), juce::Justification::centredLeft);
        const float tp = proc.truePeakHoldDb.load();
        const float ceil = proc.apvts.getRawParameterValue ("ceiling")->load();
        g.setColour (tp > ceil + 0.1f ? theme::red : theme::text);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (tp > -70.0f ? juce::String (tp, 2) + " dBTP" : juce::String ("--"), row, juce::Justification::centredLeft);
    }
    r.removeFromTop (8);

    // limiter activity
    {
        auto row = r.removeFromTop (34);
        g.setColour (theme::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("LIMITER", row.removeFromTop (16), juce::Justification::centredLeft);
        g.drawText (juce::String (shownGr, 1) + " dB", row.withTrimmedTop (-16).removeFromTop (16), juce::Justification::centredRight);
        auto bar = row.toFloat().withHeight (8.0f).translated (0, 4.0f);
        g.setColour (theme::track);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (shownGr < -6.0f ? theme::red : theme::accent);
        g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * juce::jlimit (0.0f, 1.0f, -shownGr / 12.0f)), 3.0f);
    }

    g.setColour (theme::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Mix going in: " + (proc.inShortTerm.load() > -70.0f ? juce::String (proc.inShortTerm.load(), 1) + " LUFS" : juce::String ("--")),
                r.removeFromTop (18), juce::Justification::centredLeft);
}

void MasterEditor::mouseDown (const juce::MouseEvent& e)
{
    if (peakArea.contains (e.getPosition())) proc.resetPeakHold();
}

void MasterEditor::timerCallback()
{
    // Listen button tells you what's going on
    if (proc.learning.load())
        listenBtn.setButtonText ("LISTENING...  " + juce::String (juce::roundToInt (proc.learnProgress.load() * 100.0f))
                                 + "%   (play the loudest part)");
    else if (proc.settling.load())
        listenBtn.setButtonText ("ADJUSTING LOUDNESS...");
    else if (proc.learnedValid.load())
        listenBtn.setButtonText ("LEARNED THIS SONG   (click to listen again)");
    else
        listenBtn.setButtonText ("LISTEN   (press, then play the chorus)");
    listenBtn.setToggleState (proc.learning.load(), juce::dontSendNotification);

    const bool original = proc.apvts.getRawParameterValue ("compare")->load() > 0.5f;
    compareBtn.setButtonText (original ? "HEARING: ORIGINAL   (click for mastered)" : "HEARING: MASTERED   (click for original)");
    compareBtn.setToggleState (! original, juce::dontSendNotification);

    if (proc.learnedChanged.exchange (false)) proc.syncLearnedToState();

    const float gr = proc.limiterGrDb.load();
    shownGr += (gr < shownGr ? 0.6f : 0.08f) * (gr - shownGr);

    if (proc.presetManager.getCurrentName() != lastPresetName)
    {
        lastPresetName = proc.presetManager.getCurrentName();
        presetBar->refresh();
    }
    repaint (meterArea);
    repaint (blurbArea);
    if (advanced) repaint (advArea);
}
