#pragma once

#include "PluginProcessor.h"
#include "EqDisplay.h"
#include "ui/Theme.h"
#include "ui/Controls.h"
#include "ui/PresetBar.h"

class EqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EqEditor (EqProcessor&);
    ~EqEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (bool smart);
    void selectBand (int b);
    void selectSmart (int s);
    void assignResonance (const EqDisplay::Resonance&);
    void layoutBottom();
    juce::String bandChipText (int b) const;

    EqProcessor& proc;
    vs::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<vs::PresetBar> presetBar;
    juce::TextButton manualTab { "MANUAL" }, smartTab { "SMART" };
    EqDisplay display;
    bool smart = false;

    // Manual tab
    juce::OwnedArray<juce::TextButton> bandChips;
    std::unique_ptr<vs::ParamToggle> bandOn;
    std::unique_ptr<vs::ParamChoice> bandType, bandSlope;
    std::unique_ptr<vs::ParamKnob> bandFreq, bandGain, bandQ;
    std::unique_ptr<vs::ParamKnob> output;

    // Smart tab
    std::unique_ptr<vs::ParamToggle> carveOn;
    juce::OwnedArray<vs::ParamKnob> carveKnobs;
    juce::Label carveStatus;
    juce::ToggleButton findToggle;
    juce::OwnedArray<juce::TextButton> resButtons;
    juce::OwnedArray<juce::TextButton> smartChips;
    std::unique_ptr<vs::ParamToggle> smartOn;
    std::unique_ptr<vs::ParamKnob> smartFreq, smartQ, smartRange, smartThresh;

    juce::Rectangle<int> bottom, carveArea, finderArea, smartArea, bandArea;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqEditor)
};
