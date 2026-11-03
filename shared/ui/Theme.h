#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EncoreFonts.h"

// The shared ENCORE. look: warm near-black panels, ember-orange accent, paper-white
// text, arc knobs and pill on/off switches. The colours are encoremusic.studio's own
// tokens (web/master/encore-tokens.css), so the plugins, Basement Studio and the web
// tools read as one family. Every plugin in the suite uses this.
namespace vs
{
    namespace theme
    {
        inline const juce::Colour bg     { 0xff0c0a09 };   // --bg-base-default
        inline const juce::Colour panel  { 0xff171412 };   // --bg-surface-default
        inline const juce::Colour edge   { 0xff2b2826 };   // --divider (paper at 10% on the panel)
        inline const juce::Colour text   { 0xfff2efe8 };   // --brand-paper
        inline const juce::Colour soft   { 0xffbdb4a9 };   // --text-secondary
        inline const juce::Colour dim    { 0xff91887d };   // --text-muted
        inline const juce::Colour accent { 0xffff3b00 };   // --brand-primary
        inline const juce::Colour accentHover { 0xffff6c40 };
        inline const juce::Colour track  { 0xff2c2723 };   // --bg-surface-strong
        inline const juce::Colour green  { 0xff7bd3a4 };   // --text-success-default
        inline const juce::Colour red    { 0xffef4444 };   // --system-error
        inline const juce::Colour blue   { 0xff22d3ee };   // --brand-accent
        inline const juce::Colour warn   { 0xffe8b04b };   // --system-warning

        // panels and cards: the site's rounded-panel, scaled to plugin size
        inline constexpr float radius = 12.0f;
    }

    // Text: the site sets DM Sans for words and Space Mono for numbers. Every label
    // goes through these two, so the typefaces are chosen in one place.
    inline juce::FontOptions font (float size, bool bold = false)
    {
        return juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
    }
    inline juce::FontOptions mono (float size, bool bold = false)
    {
        return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain);
    }

    // The top-left title every plugin shows: the ENCORE. wordmark (the period in the
    // brand orange, as on the site) over the plugin's name.
    inline void drawTitle (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& name)
    {
        auto brand = area.removeFromTop (16).toFloat();
        juce::GlyphArrangement ga;
        const auto f = juce::Font (font (12.5f, true)).withExtraKerningFactor (0.08f);
        ga.addLineOfText (f, "ENCORE", brand.getX(), brand.getBottom() - 3.0f);
        g.setColour (theme::text);
        ga.draw (g);
        g.setColour (theme::accent);
        g.setFont (f);
        g.drawSingleLineText (".", (int) std::ceil (ga.getBoundingBox (0, -1, true).getRight() - 0.5f), (int) brand.getBottom() - 3);
        g.setColour (theme::text);
        g.setFont (font (25.0f, true));
        g.drawText (name.toUpperCase(), area.removeFromTop (32), juce::Justification::centredLeft);
    }

    // A section's card: surface colour, a hairline edge, and its name in small caps,
    // orange while the section is on
    inline void drawPanel (juce::Graphics& g, juce::Rectangle<int> a, const juce::String& title = {}, bool on = true)
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (a.toFloat(), theme::radius);
        g.setColour (theme::edge);
        g.drawRoundedRectangle (a.toFloat().reduced (0.5f), theme::radius, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (on ? theme::accent : theme::dim);
            g.setFont (juce::Font (font (12.0f, true)).withExtraKerningFactor (0.06f));
            g.drawText (title, a.reduced (14, 8).removeFromTop (24), juce::Justification::centredLeft);
        }
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

    // The site's two typefaces, built into the plugin (shared/ui/fonts, SIL OFL). JUCE
    // looks every font up through the app's *default* LookAndFeel, not the one a
    // component uses, so while any ENCORE. editor is open this one is the default:
    // then every label, readout and menu (ours and JUCE's own) draws in DM Sans, and
    // anything asking for the monospaced font gets Space Mono. Editors share one
    // instance (SharedResourcePointer), so closing one of two open editors keeps it.
    class Typefaces : public juce::LookAndFeel_V4
    {
    public:
        Typefaces()
        {
            juce::LookAndFeel::setDefaultLookAndFeel (this);
            juce::Typeface::clearTypefaceCache();
        }
        ~Typefaces() override
        {
            if (&juce::LookAndFeel::getDefaultLookAndFeel() == this)
                juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
            juce::Typeface::clearTypefaceCache();
        }

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            const auto name = f.getTypefaceName();
            if (name == juce::Font::getDefaultSansSerifFontName())  return f.isBold() ? sansBold : sans;
            if (name == juce::Font::getDefaultMonospacedFontName()) return f.isBold() ? monoBold : mono;
            return juce::LookAndFeel_V4::getTypefaceForFont (f);
        }

    private:
        static juce::Typeface::Ptr load (const char* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }
        const juce::Typeface::Ptr sans     = load (EncoreFonts::DMSansRegular_ttf,    EncoreFonts::DMSansRegular_ttfSize),
                                  sansBold = load (EncoreFonts::DMSansBold_ttf,       EncoreFonts::DMSansBold_ttfSize),
                                  mono     = load (EncoreFonts::SpaceMonoRegular_ttf, EncoreFonts::SpaceMonoRegular_ttfSize),
                                  monoBold = load (EncoreFonts::SpaceMonoBold_ttf,    EncoreFonts::SpaceMonoBold_ttfSize);
    };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
        juce::SharedResourcePointer<Typefaces> typefaces;   // first, so the fonts are in place before anything draws

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
            setColour (juce::TextButton::textColourOnId, juce::Colours::white);
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
            g.setColour (theme::track);
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
            g.setColour (on ? juce::Colours::white : theme::dim);
            g.setFont (font (11.0f, true));
            g.drawText (on ? "ON" : "OFF", r, juce::Justification::centred);
        }
    };
}
