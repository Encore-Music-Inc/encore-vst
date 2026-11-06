#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/PresetBar.h"
#include "ui/Theme.h"

class VocalSuiteEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalSuiteEditor (VocalSuiteProcessor&);
    ~VocalSuiteEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Control
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<APVTS::SliderAttachment> sliderAtt;
        std::unique_ptr<APVTS::ComboBoxAttachment> comboAtt;
    };

    struct Section
    {
        juce::String title;
        int row = 0;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::vector<Control> controls;
        std::unique_ptr<APVTS::ButtonAttachment> toggleAtt;
        juce::Rectangle<int> area;
    };

    void addSection (const juce::String& title, int row, const char* toggleId, std::initializer_list<const char*> ids);
    void timerCallback() override;
    void drawMeter (juce::Graphics&, juce::Rectangle<int>, const juce::String& name, float value, float lo, float hi, bool centred, juce::Colour);

    VocalSuiteProcessor& proc;
    vs::LookAndFeel lnf;
    std::vector<std::unique_ptr<Section>> sections;
    std::unique_ptr<vs::PresetBar> presetBar;
    juce::String lastPresetName;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Rectangle<int> meterArea;

    float mIn = -100, mRide = 0, mDeess = 0, mComp = 0, mOut = -100;
    float shownNote = -1.0f;
    int noteHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSuiteEditor)
};
