#pragma once

#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Controls.h"
#include "ui/PresetBar.h"

// Draws the Body curve: what Body is doing now (curve x Amount) over the
// full curve, on a 20 Hz - 20 kHz / +-12 dB grid.
class BodyCurveView : public juce::Component
{
public:
    explicit BodyCurveView (StrumProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    StrumProcessor& proc;
};

class StrumEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StrumEditor (StrumProcessor&);
    ~StrumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section
    {
        juce::String title;
        std::unique_ptr<vs::ParamToggle> toggle;
        std::unique_ptr<vs::ParamKnob> knob;
        juce::Rectangle<int> area, statusArea;
        std::function<juce::String()> status;
    };

    void addSection (const juce::String& title, const char* toggleId, const char* knobId, const juce::String& knobLabel,
                     const juce::String& tip, std::function<juce::String()> status);
    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<int>, const juce::String& name, float value);
    juce::String bodyStatus() const;

    StrumProcessor& proc;
    vs::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<vs::PresetBar> presetBar;
    juce::String lastPresetName;

    // Body
    std::unique_ptr<vs::ParamToggle> bodyToggle;
    std::unique_ptr<vs::ParamChoice> bodyCurve;
    std::unique_ptr<vs::ParamKnob> bodyAmount;
    juce::TextButton learnBtn, forgetBtn { "Forget" }, compareBtn;
    BodyCurveView curveView { proc };
    juce::Rectangle<int> bodyArea, bodyStatusArea, progressArea;

    std::vector<std::unique_ptr<Section>> sections;
    juce::Rectangle<int> meterArea;
    float mIn = -100.0f, mOut = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrumEditor)
};
