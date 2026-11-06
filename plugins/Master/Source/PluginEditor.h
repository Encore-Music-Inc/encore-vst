#pragma once

#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Controls.h"
#include "ui/PresetBar.h"

class MasterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MasterEditor (MasterProcessor&);
    ~MasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setAdvanced (bool show);
    void drawMeters (juce::Graphics&, juce::Rectangle<int>);

    MasterProcessor& proc;
    vs::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<vs::PresetBar> presetBar;
    std::unique_ptr<vs::ChoiceButtons> styles, targets;
    std::unique_ptr<vs::ParamKnob> input, intensity;
    juce::TextButton listenBtn, compareBtn, advancedBtn { "ADVANCED" };
    juce::OwnedArray<vs::ParamKnob> advKnobs;
    bool advanced = false;

    juce::Rectangle<int> blurbArea, meterArea, advArea, peakArea;
    juce::String lastPresetName;
    float shownGr = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterEditor)
};
