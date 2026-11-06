#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// Parameter-bound controls in the Encore style. Each one owns its label
// and its attachment, so an editor only has to create and position them.
namespace vs
{
    using APVTS = juce::AudioProcessorValueTreeState;

    class ParamKnob : public juce::Component
    {
    public:
        ParamKnob (APVTS& state, const juce::String& id, const juce::String& labelText = {})
            : slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow)
        {
            auto* param = state.getParameter (id);
            jassert (param != nullptr);
            label.setText (labelText.isNotEmpty() ? labelText : param->getName (24), juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::FontOptions (12.0f));
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
            styleKnob (slider);
            addAndMakeVisible (slider);
            addAndMakeVisible (label);
            attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
            slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromBottom (18));
            slider.setBounds (r);
        }

        juce::Slider slider;
        juce::Label label;

    private:
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    class ParamChoice : public juce::Component
    {
    public:
        ParamChoice (APVTS& state, const juce::String& id, const juce::String& labelText = {})
        {
            auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id));
            jassert (param != nullptr);
            combo.addItemList (param->choices, 1);
            label.setText (labelText.isNotEmpty() ? labelText : param->getName (24), juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (combo);
            addAndMakeVisible (label);
            attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, combo);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromBottom (18));
            combo.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 6, 120), 26));
        }

        juce::ComboBox combo;
        juce::Label label;

    private:
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    class ParamToggle : public juce::Component
    {
    public:
        ParamToggle (APVTS& state, const juce::String& id)
        {
            addAndMakeVisible (button);
            attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, button);
        }

        void resized() override { button.setBounds (getLocalBounds()); }

        juce::ToggleButton button;

    private:
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    // A row of mutually exclusive buttons driving a choice parameter (e.g.
    // reverb modes, EQ tabs). Clicking a button sets the parameter; the
    // parameter changing (automation, presets) updates the buttons.
    class ChoiceButtons : public juce::Component, private juce::AudioProcessorParameter::Listener, private juce::AsyncUpdater
    {
    public:
        ChoiceButtons (APVTS& state, const juce::String& id)
            : param (dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        {
            jassert (param != nullptr);
            for (int i = 0; i < param->choices.size(); ++i)
            {
                auto* b = buttons.add (new juce::TextButton (param->choices[i]));
                b->setClickingTogglesState (false);
                b->onClick = [this, i]
                {
                    param->beginChangeGesture();
                    param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
                    param->endChangeGesture();
                };
                addAndMakeVisible (b);
            }
            param->addListener (this);
            update();
        }

        ~ChoiceButtons() override
        {
            param->removeListener (this);
            cancelPendingUpdate();
        }

        void setRows (int r) { rows = juce::jmax (1, r); resized(); }

        void resized() override
        {
            const int perRow = (buttons.size() + rows - 1) / rows;
            const int h = getHeight() / rows, w = getWidth() / juce::jmax (1, perRow);
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setBounds (juce::Rectangle<int> ((i % perRow) * w, (i / perRow) * h, w, h).reduced (2, rows > 1 ? 2 : 0));
        }

    private:
        int rows = 1;
        void parameterValueChanged (int, float) override { triggerAsyncUpdate(); }
        void parameterGestureChanged (int, bool) override {}
        void handleAsyncUpdate() override { update(); }

        void update()
        {
            for (int i = 0; i < buttons.size(); ++i)
                buttons[i]->setToggleState (i == param->getIndex(), juce::dontSendNotification);
        }

        juce::AudioParameterChoice* param;
        juce::OwnedArray<juce::TextButton> buttons;
    };
}
