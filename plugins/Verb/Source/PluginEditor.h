#pragma once

#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Controls.h"
#include "ui/PresetBar.h"

class VerbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VerbEditor (VerbProcessor&);
    ~VerbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawShape (juce::Graphics&, juce::Rectangle<float>);

    VerbProcessor& proc;
    vs::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<vs::PresetBar> presetBar;
    std::unique_ptr<vs::ChoiceButtons> modes;
    juce::OwnedArray<vs::ParamKnob> knobs;
    juce::Rectangle<int> display;
    juce::String lastPresetName;
    float shownWet = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerbEditor)
};
