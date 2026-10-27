#include "PluginEditor.h"

namespace theme = vs::theme;

namespace
{
    juce::String shortHz (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 0 : 1) + "k" : juce::String (juce::roundToInt (f));
    }
    const char* typeShort[] { "Bell", "Lo Shelf", "Hi Shelf", "Lo Cut", "Hi Cut", "Notch" };
}

EqEditor::EqEditor (EqProcessor& p)
    : AudioProcessorEditor (p), proc (p), display (p)
{
    setLookAndFeel (&lnf);

    presetBar = std::make_unique<vs::PresetBar> (proc.presetManager, proc.getFactoryPresetNames(),
                                                 [this] (int i) { proc.applyPreset (i); });
    addAndMakeVisible (*presetBar);
    lastPresetName = proc.presetManager.getCurrentName();

    for (auto* t : { &manualTab, &smartTab })
    {
        t->setClickingTogglesState (false);
        addAndMakeVisible (t);
    }
    manualTab.onClick = [this] { showTab (false); };
    smartTab.onClick = [this] { showTab (true); };
    smartTab.setTooltip ("Carve (duck this track around a side-chained one) and Smart bands (cut only when a frequency spikes)");

    addAndMakeVisible (display);
    display.onSelectBand = [this] (int b) { selectBand (b); };
    display.onSelectSmart = [this] (int s) { selectSmart (s); };
    display.onResonanceClicked = [this] (const EqDisplay::Resonance& r) { assignResonance (r); };

    // Manual
    for (int b = 0; b < EqProcessor::numBands; ++b)
    {
        auto* chip = bandChips.add (new juce::TextButton());
        chip->setColour (juce::TextButton::buttonOnColourId, EqDisplay::bandColour (b));
        chip->onClick = [this, b] { selectBand (b); };
        addChildComponent (chip);
    }
    output = std::make_unique<vs::ParamKnob> (proc.apvts, "output");
    addChildComponent (*output);

    // Smart
    carveOn = std::make_unique<vs::ParamToggle> (proc.apvts, "carve_on");
    addChildComponent (*carveOn);
    for (auto* id : { "carve_depth", "carve_low", "carve_high", "carve_speed" })
    {
        auto* k = carveKnobs.add (new vs::ParamKnob (proc.apvts, id, juce::String (id) == "carve_low" ? "Low" : juce::String (id) == "carve_high" ? "High" : juce::String()));
        addChildComponent (k);
    }
    carveStatus.setFont (juce::FontOptions (11.0f));
    carveStatus.setJustificationType (juce::Justification::topLeft);
    addChildComponent (carveStatus);

    findToggle.setButtonText ("Find");
    findToggle.setToggleState (true, juce::dontSendNotification);
    findToggle.onClick = [this] { display.findResonances = findToggle.getToggleState(); if (! display.findResonances) display.resonances.clear(); };
    addChildComponent (findToggle);
    for (int i = 0; i < 5; ++i)
    {
        auto* b = resButtons.add (new juce::TextButton());
        b->onClick = [this, i] { if (i < (int) display.resonances.size()) assignResonance (display.resonances[(size_t) i]); };
        b->setTooltip ("Turn this into a Smart band that only cuts when it rings");
        addChildComponent (b);
    }
    for (int s = 0; s < EqProcessor::numSmart; ++s)
    {
        auto* chip = smartChips.add (new juce::TextButton ("S" + juce::String (s + 1)));
        chip->setColour (juce::TextButton::buttonOnColourId, theme::red);
        chip->onClick = [this, s] { selectSmart (s); };
        addChildComponent (chip);
    }

    setSize (1100, 700);
    selectBand (1);
    selectSmart (0);
    showTab (false);
    startTimerHz (30);
}

EqEditor::~EqEditor() { setLookAndFeel (nullptr); }

void EqEditor::showTab (bool s)
{
    smart = s;
    display.smartView = s;
    manualTab.setToggleState (! s, juce::dontSendNotification);
    smartTab.setToggleState (s, juce::dontSendNotification);

    for (auto* c : bandChips) c->setVisible (! s);
    for (juce::Component* c : { (juce::Component*) bandOn.get(), (juce::Component*) bandType.get(), (juce::Component*) bandSlope.get(),
                                (juce::Component*) bandFreq.get(), (juce::Component*) bandGain.get(), (juce::Component*) bandQ.get(),
                                (juce::Component*) output.get() })
        if (c != nullptr) c->setVisible (! s);

    for (juce::Component* c : { (juce::Component*) carveOn.get(), (juce::Component*) &carveStatus, (juce::Component*) &findToggle,
                                (juce::Component*) smartOn.get(), (juce::Component*) smartFreq.get(), (juce::Component*) smartQ.get(),
                                (juce::Component*) smartRange.get(), (juce::Component*) smartThresh.get() })
        if (c != nullptr) c->setVisible (s);
    for (auto* k : carveKnobs) k->setVisible (s);
    for (auto* c : smartChips) c->setVisible (s);
    for (auto* b : resButtons) b->setVisible (false); // shown by the timer when there are results

    repaint();
}

void EqEditor::selectBand (int b)
{
    display.selectedBand = b;
    for (int i = 0; i < bandChips.size(); ++i) bandChips[i]->setToggleState (i == b, juce::dontSendNotification);

    const auto pre = "b" + juce::String (b + 1) + "_";
    bandOn = std::make_unique<vs::ParamToggle> (proc.apvts, pre + "on");
    bandType = std::make_unique<vs::ParamChoice> (proc.apvts, pre + "type", "Type");
    bandSlope = std::make_unique<vs::ParamChoice> (proc.apvts, pre + "slope", "Slope");
    bandFreq = std::make_unique<vs::ParamKnob> (proc.apvts, pre + "freq", "Freq");
    bandGain = std::make_unique<vs::ParamKnob> (proc.apvts, pre + "gain", "Gain");
    bandQ = std::make_unique<vs::ParamKnob> (proc.apvts, pre + "q", "Q / Width");
    for (juce::Component* c : { (juce::Component*) bandOn.get(), (juce::Component*) bandType.get(), (juce::Component*) bandSlope.get(),
                                (juce::Component*) bandFreq.get(), (juce::Component*) bandGain.get(), (juce::Component*) bandQ.get() })
        addChildComponent (c);
    showTab (smart);
    layoutBottom();
}

void EqEditor::selectSmart (int s)
{
    display.selectedSmart = s;
    for (int i = 0; i < smartChips.size(); ++i) smartChips[i]->setToggleState (i == s, juce::dontSendNotification);

    const auto pre = "s" + juce::String (s + 1) + "_";
    smartOn = std::make_unique<vs::ParamToggle> (proc.apvts, pre + "on");
    smartFreq = std::make_unique<vs::ParamKnob> (proc.apvts, pre + "freq", "Freq");
    smartQ = std::make_unique<vs::ParamKnob> (proc.apvts, pre + "q", "Q / Width");
    smartRange = std::make_unique<vs::ParamKnob> (proc.apvts, pre + "range", "Max Cut");
    smartThresh = std::make_unique<vs::ParamKnob> (proc.apvts, pre + "thresh", "Threshold");
    smartThresh->slider.setTooltip ("How loud this frequency has to get, relative to everything else, before it's cut. Lower = cuts more often.");
    for (juce::Component* c : { (juce::Component*) smartOn.get(), (juce::Component*) smartFreq.get(), (juce::Component*) smartQ.get(),
                                (juce::Component*) smartRange.get(), (juce::Component*) smartThresh.get() })
        addChildComponent (c);
    showTab (smart);
    layoutBottom();
}

// Clicking a found resonance: put it in a free smart band (or the selected
// one if all are in use), narrow, cutting only above its usual level.
void EqEditor::assignResonance (const EqDisplay::Resonance& r)
{
    int target = -1;
    for (int s = 0; s < EqProcessor::numSmart && target < 0; ++s)
        if (! proc.readSmart (s).on) target = s;
    if (target < 0) target = display.selectedSmart;

    auto set = [this] (const juce::String& pid, float v)
    {
        auto* param = proc.apvts.getParameter (pid);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (v));
        param->endChangeGesture();
    };
    const auto pre = "s" + juce::String (target + 1) + "_";
    set (pre + "freq", r.freq);
    set (pre + "q", 5.0f);
    set (pre + "range", juce::jlimit (3.0f, 9.0f, r.prominenceDb));
    // threshold sits below the ring's usual level in proportion to how much it
    // sticks out, so a steady ring is clearly tamed and louder moments more so
    set (pre + "thresh", juce::jlimit (-40.0f, 0.0f, r.relDb - 0.5f * r.prominenceDb));
    set (pre + "on", 1.0f);
    selectSmart (target);
}

void EqEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (54);
    header.removeFromLeft (150);
    presetBar->setBounds (header.removeFromLeft (350).withSizeKeepingCentre (350, 28));
    auto tabs = header.removeFromRight (240).withSizeKeepingCentre (240, 30);
    manualTab.setBounds (tabs.removeFromLeft (120).reduced (2, 0));
    smartTab.setBounds (tabs.reduced (2, 0));

    r.removeFromTop (8);
    display.setBounds (r.removeFromTop (380));
    r.removeFromTop (10);
    bottom = r;
    layoutBottom();
}

void EqEditor::layoutBottom()
{
    if (bottom.isEmpty()) return;
    auto r = bottom;

    // Manual: chip row, then the selected band's controls
    {
        auto m = r;
        auto chips = m.removeFromTop (30);
        const int cw = chips.getWidth() / EqProcessor::numBands;
        for (auto* c : bandChips) c->setBounds (chips.removeFromLeft (cw).reduced (3, 0));
        m.removeFromTop (10);
        bandArea = m;
        auto ctl = m.reduced (12, 8);
        ctl.removeFromTop (22);
        if (output != nullptr) output->setBounds (ctl.removeFromRight (110));
        ctl.removeFromRight (20);
        if (bandOn != nullptr)
        {
            bandOn->setBounds (ctl.removeFromLeft (60).withSizeKeepingCentre (48, 24));
            const int w = ctl.getWidth() / 5;
            bandType->setBounds (ctl.removeFromLeft (w));
            bandFreq->setBounds (ctl.removeFromLeft (w));
            bandGain->setBounds (ctl.removeFromLeft (w));
            bandQ->setBounds (ctl.removeFromLeft (w));
            bandSlope->setBounds (ctl.removeFromLeft (w));
        }
    }

    // Smart: Carve | Finder | Smart bands
    {
        auto s = r;
        carveArea = s.removeFromLeft (390);
        s.removeFromLeft (10);
        finderArea = s.removeFromLeft (190);
        s.removeFromLeft (10);
        smartArea = s;

        auto c = carveArea.reduced (12, 8);
        auto top = c.removeFromTop (24);
        carveOn->setBounds (top.removeFromRight (48));
        carveStatus.setBounds (c.removeFromTop (34));
        const int kw = c.getWidth() / 4;
        for (auto* k : carveKnobs) k->setBounds (c.removeFromLeft (kw));

        auto f = finderArea.reduced (12, 8);
        auto ftop = f.removeFromTop (24);
        findToggle.setBounds (ftop.removeFromRight (70));
        f.removeFromTop (4);
        for (auto* b : resButtons) { b->setBounds (f.removeFromTop (24)); f.removeFromTop (4); }

        auto sm = smartArea.reduced (12, 8);
        auto stop = sm.removeFromTop (24);
        if (smartOn != nullptr) smartOn->setBounds (stop.removeFromRight (48));
        stop.removeFromRight (10);
        stop.removeFromLeft (110);
        const int chw = stop.getWidth() / EqProcessor::numSmart;
        for (auto* ch : smartChips) ch->setBounds (stop.removeFromLeft (chw).reduced (3, 0));
        sm.removeFromTop (8);
        if (smartFreq != nullptr)
        {
            const int w = sm.getWidth() / 4;
            smartFreq->setBounds (sm.removeFromLeft (w));
            smartQ->setBounds (sm.removeFromLeft (w));
            smartRange->setBounds (sm.removeFromLeft (w));
            smartThresh->setBounds (sm.removeFromLeft (w));
        }
    }
}

void EqEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    auto title = getLocalBounds().reduced (14).removeFromTop (54).removeFromLeft (150);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("EQUALIZE", title.removeFromTop (32), juce::Justification::bottomLeft);
    g.setColour (theme::accent);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("ENCORE:", title, juce::Justification::topLeft);

    auto panel = [&] (juce::Rectangle<int> a, const juce::String& t, bool on)
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (a.toFloat(), 8.0f);
        g.setColour (theme::edge);
        g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (on ? theme::accent : theme::dim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (t, a.reduced (12, 8).removeFromTop (24), juce::Justification::centredLeft);
    };

    if (! smart)
    {
        const int b = display.selectedBand;
        panel (bandArea, "BAND " + juce::String (b + 1), proc.readBand (b).on);
    }
    else
    {
        panel (carveArea, "CARVE", proc.readCarve().on);
        panel (finderArea, "RESONANCES", findToggle.getToggleState());
        panel (smartArea, "SMART BANDS", true);

        if (display.resonances.empty() && findToggle.getToggleState())
        {
            g.setColour (theme::dim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawFittedText ("Play the track. Ringing or boxy frequencies show up here and as + markers.",
                              finderArea.reduced (12, 8).withTrimmedTop (30), juce::Justification::topLeft, 4);
        }

        // live cut meters under the S chips
        for (int s = 0; s < EqProcessor::numSmart; ++s)
        {
            const auto chip = smartChips[s]->getBounds().toFloat();
            const float cut = proc.smartCutDb[s].load();
            const float range = std::max (1.0f, proc.readSmart (s).rangeDb);
            auto bar = juce::Rectangle<float> (chip.getX() + 3.0f, chip.getBottom() + 2.0f, chip.getWidth() - 6.0f, 3.0f);
            g.setColour (theme::track);
            g.fillRect (bar);
            g.setColour (theme::red);
            g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, cut / range)));
        }
    }
}

juce::String EqEditor::bandChipText (int b) const
{
    const auto st = proc.readBand (b);
    return juce::String (b + 1) + "  " + typeShort[juce::jlimit (0, 5, st.type)] + " " + shortHz (st.freq);
}

void EqEditor::timerCallback()
{
    display.tick();

    for (int b = 0; b < bandChips.size(); ++b)
    {
        bandChips[b]->setButtonText (bandChipText (b));
        bandChips[b]->setAlpha (proc.readBand (b).on ? 1.0f : 0.55f);
    }

    if (! smart)
    {
        const int type = proc.readBand (display.selectedBand).type;
        if (bandGain != nullptr) bandGain->setEnabled (type <= vs::EqBand::highShelf);
        if (bandSlope != nullptr) bandSlope->setEnabled (type == vs::EqBand::lowCut || type == vs::EqBand::highCut);
    }
    else
    {
        const bool sc = proc.sidechainLive.load();
        carveStatus.setText (sc ? "Side-chain connected. This track dips wherever the side-chained track is loud."
                                : "No side-chain yet: in Cubase, click the side-chain button at the top of this plugin, then route your vocal to it.",
                             juce::dontSendNotification);
        carveStatus.setColour (juce::Label::textColourId, sc ? theme::green : theme::dim);

        for (int i = 0; i < resButtons.size(); ++i)
        {
            const bool has = display.findResonances && i < (int) display.resonances.size();
            resButtons[i]->setVisible (has);
            if (has)
            {
                const auto& res = display.resonances[(size_t) i];
                resButtons[i]->setButtonText ("+  " + shortHz (res.freq) + "Hz   " + juce::String (res.prominenceDb, 1) + " dB");
            }
        }
    }

    if (proc.presetManager.getCurrentName() != lastPresetName)
    {
        lastPresetName = proc.presetManager.getCurrentName();
        presetBar->refresh();
    }
    repaint (bottom);
}
