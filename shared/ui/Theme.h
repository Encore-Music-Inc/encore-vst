#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The shared Encore look: dark panels, amber accent, arc knobs and pill
// on/off switches. Every plugin in the suite uses this so they read as one family.
namespace vs
{
    namespace theme
    {
        inline const juce::Colour bg     { 0xff15171c };
        inline const juce::Colour panel  { 0xff1f232b };
        inline const juce::Colour edge   { 0xff2c313b };
        inline const juce::Colour text   { 0xffe6e8ec };
        inline const juce::Colour dim    { 0xff8a919e };
        inline const juce::Colour accent { 0xffe8a33d };
        inline const juce::Colour track  { 0xff343a46 };
        inline const juce::Colour green  { 0xff5cc98a };
        inline const juce::Colour red    { 0xffe0605a };
        inline const juce::Colour blue   { 0xff5aa7e0 };
    }

    // A knob's value readout copies its colours once, when it's created,
    // which can be before the knob is inside a themed window. Setting them on
    // the slider itself makes the readout match no matter the order.
    inline void styleKnob (juce::Slider& s)
    {
        s.setColour (juce::Slider::textBoxTextColourId, theme::text);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxHighlightColourId, theme::accent.withAlpha (0.4f));
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::Slider::textBoxTextColourId, theme::text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Label::textColourId, theme::dim);
            setColour (juce::ComboBox::backgroundColourId, theme::track);
            setColour (juce::ComboBox::outlineColourId, theme::edge);
            setColour (juce::ComboBox::textColourId, theme::text);
            setColour (juce::ComboBox::arrowColourId, theme::accent);
            setColour (juce::PopupMenu::backgroundColourId, theme::panel);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.3f));
            setColour (juce::PopupMenu::textColourId, theme::text);
            setColour (juce::PopupMenu::highlightedTextColourId, theme::text);
            setColour (juce::TextButton::buttonColourId, theme::track);
            setColour (juce::TextButton::textColourOffId, theme::text);
            setColour (juce::AlertWindow::backgroundColourId, theme::panel);
            setColour (juce::AlertWindow::textColourId, theme::text);
            setColour (juce::AlertWindow::outlineColourId, theme::accent);
            setColour (juce::TextEditor::backgroundColourId, theme::bg);
            setColour (juce::TextEditor::textColourId, theme::text);
            setColour (juce::TextEditor::outlineColourId, theme::edge);
            setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
            setColour (juce::TooltipWindow::backgroundColourId, theme::panel);
            setColour (juce::TooltipWindow::textColourId, theme::text);
            setColour (juce::TextButton::buttonOnColourId, theme::accent);
            setColour (juce::TextButton::textColourOnId, theme::bg);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);
            const float thick = 4.0f;

            juce::Path back;
            back.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f, startAngle, endAngle, true);
            g.setColour (theme::track);
            g.strokePath (back, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Bipolar knobs (EQ) fill from the middle, others from the left
            const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 && std::abs (slider.getMinimum() + slider.getMaximum()) < 0.001;
            const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

            juce::Path value;
            value.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (slider.isEnabled() ? theme::accent : theme::dim);
            g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float inner = radius - thick * 2.5f;
            g.setColour (theme::panel.brighter (0.15f));
            g.fillEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f);

            juce::Path pointer;
            pointer.addRoundedRectangle (-1.5f, -inner, 3.0f, inner * 0.55f, 1.5f);
            g.setColour (theme::text);
            g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
        {
            const auto r = b.getLocalBounds().toFloat().reduced (2.0f);
            const bool on = b.getToggleState();
            g.setColour (on ? theme::accent : theme::track);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            if (highlighted)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            }
            g.setColour (on ? theme::bg : theme::dim);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
        }
    };
}
