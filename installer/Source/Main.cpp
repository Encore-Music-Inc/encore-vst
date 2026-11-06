// ENCORE. VST Setup: the branded installer for the ENCORE. plugins.
// One window: pick the plugins with their cards' checkboxes, watch them install,
// done. Run it again (or Modify / Uninstall in Windows Settings) to add or remove one.
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"
#include "InstallCore.h"
#include "Wordmark.h"

namespace
{
    namespace theme = vs::theme;
    using setup::Status;

    const juce::String kVersion = ENCORE_VST_VERSION;

    juce::Path letters()
    {
        juce::Path p;
        for (auto* d : setup::wordmark::letters)
            p.addPath (juce::Drawable::parseSVGPath (d));
        return p;
    }

    // Draws ENCORE. (letters in paper white, the square in the brand orange) to fill width w at (x, y)
    void drawWordmark (juce::Graphics& g, float x, float y, float w)
    {
        static const juce::Path p = letters();
        const float s = w / 706.176f;
        const auto t = juce::AffineTransform::translation (-408.308f, -330.5f).scaled (s).translated (x, y);
        g.setColour (theme::text);
        g.fillPath (p, t);
        g.setColour (theme::accent);
        g.fillRect (juce::Rectangle<float> (1083.956f, 501.572f, 30.528f, 30.528f).transformedBy (t));
    }

    void drawTick (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float thick)
    {
        juce::Path t;
        t.startNewSubPath (r.getX() + r.getWidth() * 0.24f, r.getY() + r.getHeight() * 0.53f);
        t.lineTo (r.getX() + r.getWidth() * 0.43f, r.getY() + r.getHeight() * 0.71f);
        t.lineTo (r.getX() + r.getWidth() * 0.77f, r.getY() + r.getHeight() * 0.32f);
        g.setColour (c);
        g.strokePath (t, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    //==========================================================================
    // A rounded button: primary is the orange pill, the other a hairline one
    class Pill : public juce::Component
    {
    public:
        Pill (bool isPrimary) : primary (isPrimary) { setMouseCursor (juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus (false); }

        void setText (const juce::String& t) { text = t; repaint(); }
        std::function<void()> onClick;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const bool on = isEnabled();
            const bool hot = on && isMouseOver();
            if (primary)
            {
                g.setColour (! on ? theme::track : hot ? theme::accentHover : theme::accent);
                g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            }
            else
            {
                g.setColour (hot ? theme::soft : theme::edge.brighter (0.3f));
                g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
            }
            g.setColour (primary ? (on ? juce::Colours::white : theme::dim) : (hot ? theme::text : theme::soft));
            g.setFont (vs::font (15.0f, true));
            g.drawText (text, getLocalBounds(), juce::Justification::centred);
        }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (isEnabled() && getLocalBounds().contains (e.getPosition()) && onClick)
                onClick();
        }
        void enablementChanged() override { setMouseCursor (isEnabled() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }

    private:
        bool primary;
        juce::String text;
    };

    //==========================================================================
    class SetupComponent;

    // One plugin: its screenshot, name, line, and the checkbox (or, while installing, how it went)
    class Card : public juce::Component
    {
    public:
        Card (SetupComponent& o, int i) : owner (o), index (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

    private:
        SetupComponent& owner;
        int index;
    };

    //==========================================================================
    class SetupComponent : public juce::Component, private juce::Timer
    {
    public:
        enum class Page { choose, working, inUse, done };

        struct Args
        {
            bool uninstall = false, noRegister = false, autoRun = false;
            juce::File dest, shots;
        };

        explicit SetupComponent (const Args& a) : args (a)
        {
            setLookAndFeel (&lnf);
            setOpaque (false);
            setWantsKeyboardFocus (true);
            setName ("ENCORE. VST Setup");

            opts.version = kVersion;
            opts.registerWithWindows = ! a.noRegister;
            if (a.dest != juce::File())
                opts.vst3Dir = a.dest;

            payload = setup::platform::resource ("PAYLOAD");
            zip = std::make_unique<juce::ZipFile> (new juce::MemoryInputStream (payload, false), true);

            for (int i = 0; i < setup::numPlugins; ++i)
            {
                const auto shot = setup::platform::resource (setup::plugins[(size_t) i].shot);
                shots[(size_t) i] = juce::ImageFileFormat::loadFrom (shot.getData(), shot.getSize());
                cards[(size_t) i] = std::make_unique<Card> (*this, i);
                addAndMakeVisible (*cards[(size_t) i]);
            }
            readInstalled();
            for (int i = 0; i < setup::numPlugins; ++i)
                keep[(size_t) i] = ! a.uninstall;   // install or change: everything ticked; uninstall: everything goes

            addAndMakeVisible (primary);
            addAndMakeVisible (secondary);
            addAndMakeVisible (all);
            addAndMakeVisible (none);
            all.setText ("Select all");
            none.setText ("None");
            all.onClick = [this] { keep.fill (true); refresh(); };
            none.onClick = [this] { keep.fill (false); refresh(); };
            primary.onClick = [this] { primaryPressed(); };
            secondary.onClick = [this] { if (page != Page::working) juce::JUCEApplication::quit(); };
            refresh();
            setSize (1000, 680);
        }

        ~SetupComponent() override { setLookAndFeel (nullptr); }

        // the state the cards draw from
        Page page = Page::choose;
        setup::Selection keep {};
        std::array<juce::String, setup::numPlugins> installed;
        std::array<Status, setup::numPlugins> status {};
        std::array<juce::Image, setup::numPlugins> shots;
        bool busy() const { return page == Page::working; }
        bool picking() const { return page == Page::choose || page == Page::inUse; }
        bool removingAll() const { return args.uninstall; }   // Uninstall removes every plugin; picking some happens in Modify
        bool isOpen (int i) const { return page == Page::inUse && inUseNames.contains (juce::String ("ENCORE. ") + setup::plugins[(size_t) i].title); }

        void toggle (int i)
        {
            if (! picking() || removingAll())
                return;
            keep[(size_t) i] = ! keep[(size_t) i];
            refresh();
        }

        //======================================================================
        void paint (juce::Graphics& g) override
        {
            const auto all = getLocalBounds().toFloat();
            g.setColour (theme::bg);
            g.fillRoundedRectangle (all, 14.0f);

            // left: the brand, the steps, the small print
            auto left = all.withWidth (320.0f);
            {
                juce::Path p;
                p.addRoundedRectangle (left.getX(), left.getY(), left.getWidth(), left.getHeight(), 14.0f, 14.0f, true, false, true, false);
                g.setColour (theme::panel);
                g.fillPath (p);
                g.setColour (theme::edge);
                g.drawVerticalLine ((int) left.getRight(), 0.0f, all.getBottom());

                // a soft orange glow behind the title
                juce::ColourGradient glow (theme::accent.withAlpha (0.20f), 40.0f, 60.0f, theme::accent.withAlpha (0.0f), 300.0f, 330.0f, true);
                g.setGradientFill (glow);
                g.fillPath (p);
            }

            drawWordmark (g, 40.0f, 54.0f, 150.0f);
            g.setColour (theme::text);
            g.setFont (juce::Font (vs::font (78.0f, true)).withExtraKerningFactor (-0.02f));
            g.drawText ("VST", 34, 92, 260, 90, juce::Justification::centredLeft);
            g.setColour (theme::soft);
            g.setFont (vs::font (15.0f));
            g.drawMultiLineText ("Five plugins for vocals, guitar, reverb, EQ and mastering. The same sound as ENCORE. Basement Studio, in any DAW.",
                                 40, 206, 250, juce::Justification::topLeft, 3.0f);

            // the three steps
            const char* steps[] = { args.uninstall ? "Confirm" : "Choose", args.uninstall ? "Remove" : "Install", "Done" };
            const int current = page == Page::choose ? 0 : page == Page::done ? 2 : 1;
            for (int s = 0; s < 3; ++s)
            {
                const float y = 330.0f + (float) s * 44.0f;
                const auto dot = juce::Rectangle<float> (40.0f, y, 26.0f, 26.0f);
                const bool past = s < current, now = s == current;
                g.setColour (now ? theme::accent : past ? theme::track : juce::Colours::transparentBlack);
                g.fillEllipse (dot);
                g.setColour (now ? theme::accent : theme::edge.brighter (0.4f));
                g.drawEllipse (dot.reduced (0.5f), 1.0f);
                if (past)
                    drawTick (g, dot.reduced (5.0f), theme::text, 2.0f);
                else
                {
                    g.setColour (now ? juce::Colours::white : theme::dim);
                    g.setFont (vs::mono (12.0f, true));
                    g.drawText (juce::String (s + 1), dot, juce::Justification::centred);
                }
                if (s < 2)
                {
                    g.setColour (theme::edge.brighter (0.2f));
                    g.drawVerticalLine (53, y + 29.0f, y + 41.0f);
                }
                g.setColour (now ? theme::text : theme::dim);
                g.setFont (vs::font (15.0f, now));
                g.drawText (steps[s], 80, (int) y, 200, 26, juce::Justification::centredLeft);
            }

            g.setColour (theme::dim);
            g.setFont (vs::mono (12.0f));
            g.drawText ("VERSION " + kVersion, 40, 560, 260, 18, juce::Justification::centredLeft);
            g.setFont (vs::font (13.0f));
            g.drawText ("VST3, Windows 10 and 11, 64-bit", 40, 584, 260, 18, juce::Justification::centredLeft);
            g.drawText ("Free. No account needed.", 40, 604, 260, 18, juce::Justification::centredLeft);
            g.setColour (theme::soft);
            g.drawText ("encoremusic.studio", 40, 630, 260, 18, juce::Justification::centredLeft);

            // right: the window buttons, heading, words
            drawWindowButtons (g);
            g.setColour (theme::text);
            g.setFont (vs::font (30.0f, true));
            g.drawText (heading(), 360, 46, 600, 40, juce::Justification::centredLeft);
            g.setColour (theme::soft);
            g.setFont (vs::font (15.0f));
            g.drawMultiLineText (subline(), 360, 106, 600, juce::Justification::topLeft, 2.0f);

            // footer: where it goes, or the progress
            g.setColour (theme::edge);
            g.drawHorizontalLine (606, 321.0f, all.getRight());
            if (page == Page::working || page == Page::done)
            {
                const auto bar = juce::Rectangle<float> (360.0f, 634.0f, 330.0f, 6.0f);
                g.setColour (theme::track);
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (theme::accent);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) progress()), 3.0f);
                g.setColour (theme::dim);
                g.setFont (vs::font (12.5f));
                g.drawText (juce::String ((int) std::round (progress() * 100.0)) + "%", 700, 626, 60, 22, juce::Justification::centredLeft);
            }
            else if (page == Page::choose)
            {
                g.setColour (theme::dim);
                g.setFont (vs::font (12.5f));
                g.drawText ((args.uninstall ? "Removes from " : "Installs to ") + opts.vst3Dir.getFullPathName(), 364, 648, 300, 18, juce::Justification::centredLeft);
            }
        }

        void resized() override
        {
            for (int i = 0; i < setup::numPlugins; ++i)
                cards[(size_t) i]->setBounds (360, 156 + i * 88, 600, 80);
            primary.setBounds (790, 624, 170, 44);
            secondary.setBounds (670, 624, 108, 44);
            all.setBounds (360, 616, 96, 26);
            none.setBounds (464, 616, 66, 26);
        }

        //======================================================================
        void mouseDown (const juce::MouseEvent& e) override
        {
            pressed = buttonAt (e.getPosition());
            if (pressed == 0)
                dragger.startDraggingComponent (this, e);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (pressed == 0)
                dragger.dragComponent (this, e, nullptr);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const int b = buttonAt (e.getPosition());
            if (b != pressed || b == 0)
                return;
            if (b == 1)
            {
                if (auto* peer = getPeer())
                    peer->setMinimised (true);
            }
            else if (! busy())
                juce::JUCEApplication::quit();
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = buttonAt (e.getPosition());
            if (h != hover) { hover = h; repaint (getWidth() - 100, 0, 100, 44); }
        }
        void mouseExit (const juce::MouseEvent&) override { if (hover) { hover = 0; repaint(); } }

        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::escapeKey && ! busy()) { juce::JUCEApplication::quit(); return true; }
            if (k == juce::KeyPress::returnKey && primary.isEnabled()) { primaryPressed(); return true; }
            return false;
        }

        // For checking the look: every page drawn to a PNG (the dev build's --shots)
        void saveShots (const juce::File& dir)
        {
            dir.createDirectory();
            const auto save = [&] (const char* name)
            {
                refresh();
                juce::Image img (juce::Image::ARGB, getWidth(), getHeight(), true);
                {
                    juce::Graphics g (img);
                    g.fillAll (juce::Colours::transparentBlack);   // the rounded corners stay see-through, for the web page
                    paintEntireComponent (g, true);
                }
                juce::FileOutputStream out (dir.getChildFile (juce::String (name) + ".png"));
                out.setPosition (0);
                out.truncate();
                juce::PNGImageFormat().writeImageToStream (img, out);
            };
            const auto saved = installed;
            installed[0] = kVersion; installed[2] = "0.3.0"; installed[4] = "0.3.0";
            keep = { true, true, true, true, false };
            save ("1-choose");
            page = Page::working;
            status = { Status::installed, Status::installed, Status::working, Status::waiting, Status::waiting };
            save ("2-working");
            page = Page::inUse;
            inUseNames = { "ENCORE. VOX" };
            save ("3-in-use");
            page = Page::done;
            status = { Status::installed, Status::installed, Status::installed, Status::installed, Status::removed };
            save ("4-done");
            installed = saved;
        }

        juce::String statusText (int i, juce::Colour& c) const
        {
            const auto& v = installed[(size_t) i];
            if (isOpen (i)) { c = theme::red; return "OPEN IN A DAW"; }
            if (picking())
            {
                c = theme::dim;
                if (keep[(size_t) i] && v.isEmpty()) { c = theme::blue; return "NEW"; }
                if (keep[(size_t) i] && v == kVersion) { c = theme::green; return "INSTALLED"; }
                if (keep[(size_t) i]) { c = theme::accentHover; return "UPDATE " + (v == "?" ? juce::String() : v + " > ") + kVersion; }
                if (v.isNotEmpty()) { c = theme::red; return "WILL BE REMOVED"; }
                return "NOT INSTALLED";
            }
            switch (status[(size_t) i])
            {
                case Status::waiting:   c = theme::dim;   return "WAITING";
                case Status::working:   c = theme::accentHover; return keep[(size_t) i] ? "INSTALLING" : "REMOVING";
                case Status::installed: c = theme::green; return "INSTALLED " + kVersion;
                case Status::removed:   c = theme::soft;  return "REMOVED";
                case Status::unchanged: c = theme::dim;   return "NOT INSTALLED";
                case Status::inUse:     c = theme::red;   return "OPEN IN A DAW";
                case Status::failed:    c = theme::red;   return "COULDN'T WRITE IT";
            }
            return {};
        }

        void pressPrimary() { primaryPressed(); }

        float spinnerAngle() const { return (float) (juce::Time::getMillisecondCounterHiRes() * 0.006); }

    private:
        Args args;
        vs::LookAndFeel lnf;
        setup::Options opts;
        juce::MemoryBlock payload;
        std::unique_ptr<juce::ZipFile> zip;
        std::array<std::unique_ptr<Card>, setup::numPlugins> cards;
        Pill primary { true }, secondary { false }, all { false }, none { false };
        juce::ComponentDragger dragger;
        juce::StringArray inUseNames;
        int pressed = 0, hover = 0;

        void readInstalled()
        {
            for (int i = 0; i < setup::numPlugins; ++i)
                installed[(size_t) i] = setup::installedVersion (opts.vst3Dir, setup::plugins[(size_t) i]);
        }

        int count (bool wanted) const
        {
            int n = 0;
            for (int i = 0; i < setup::numPlugins; ++i)
                n += wanted ? (keep[(size_t) i] ? 1 : 0) : (! keep[(size_t) i] && installed[(size_t) i].isNotEmpty() ? 1 : 0);
            return n;
        }

        juce::String heading() const
        {
            switch (page)
            {
                case Page::choose:  return args.uninstall ? "Remove ENCORE. VST" : "Pick your plugins";
                case Page::working: return count (true) == 0 ? "Removing" : "Installing";
                case Page::inUse:   return "Close your DAW first";
                case Page::done:    break;
            }
            for (auto s : status)
                if (s == Status::inUse || s == Status::failed)
                    return "Almost there";
            return count (true) == 0 ? "All removed" : "You're set";
        }

        juce::String subline() const
        {
            switch (page)
            {
                case Page::choose:
                    return args.uninstall ? "This removes all ENCORE. VST plugins from this PC. Your presets in Documents\\ENCORE stay. "
                                            "To remove only some, open Settings, Apps, ENCORE. VST and choose Modify instead."
                                          : "Untick any you don't want. Run this again any time to add or remove one.";
                case Page::working: return "This takes a few seconds.";
                case Page::inUse:
                    return inUseNames.joinIntoString (", ") + (inUseNames.size() == 1 ? " is" : " are")
                         + " open in a DAW, so Windows can't change " + (inUseNames.size() == 1 ? "it" : "them") + ". Close the DAW and press Try again"
                         + (args.uninstall ? juce::String (".") : ", or untick " + juce::String (inUseNames.size() == 1 ? "it." : "them."));
                case Page::done: break;
            }
            for (auto s : status)
                if (s == Status::inUse || s == Status::failed)
                    return "Some plugins couldn't be changed (see below). Close your DAW and run this again.";
            if (count (true) == 0)
                return "ENCORE. VST is removed. Your presets in Documents\\ENCORE stay, in case you come back.";
            return "Open your DAW and scan for new plugins. In Cubase: Studio > VST Plug-in Manager, then Rescan. Most other DAWs scan when they start.";
        }

        double progress() const
        {
            int done = 0;
            for (auto s : status)
                done += (s == Status::waiting || s == Status::working) ? 0 : 1;
            return done / (double) setup::numPlugins;
        }

        void refresh()
        {
            const bool choosing = page == Page::choose;
            all.setVisible (choosing && ! args.uninstall);
            none.setVisible (choosing && ! args.uninstall);
            secondary.setVisible (choosing || page == Page::inUse);
            secondary.setText ("Cancel");
            primary.setEnabled (true);

            if (choosing)
            {
                const int add = count (true), remove = count (false);
                if (add == 0 && remove == 0)      { primary.setText (args.uninstall ? "Nothing to remove" : "Pick a plugin"); primary.setEnabled (false); }
                else if (args.uninstall)          primary.setText ("Remove all");
                else if (add == 0)                primary.setText ("Remove " + juce::String (remove));
                else                              primary.setText ("Install " + juce::String (add) + (add == 1 ? " plugin" : " plugins"));
            }
            else if (page == Page::working)       { primary.setText (count (true) == 0 ? "Removing" : "Installing"); primary.setEnabled (false); }
            else if (page == Page::inUse)         primary.setText ("Try again");
            else                                  primary.setText ("Finish");

            for (auto& c : cards)
                c->setMouseCursor (picking() && ! removingAll() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }

        void primaryPressed()
        {
            if (page == Page::done) { juce::JUCEApplication::quit(); return; }
            if (page == Page::working) return;

            inUseNames = setup::pluginsInUse (opts, keep);
            if (! inUseNames.isEmpty()) { page = Page::inUse; refresh(); return; }

            page = Page::working;
            status.fill (Status::waiting);
            refresh();
            startTimerHz (40);

            const auto selection = keep;
            const auto o = opts;
            juce::Component::SafePointer<SetupComponent> safe (this);
            juce::Thread::launch ([this, safe, selection, o]
            {
                setup::apply (*zip, o, selection, [safe] (int i, Status s)
                {
                    juce::MessageManager::callAsync ([safe, i, s] { if (safe != nullptr) { safe->status[(size_t) i] = s; safe->repaint(); } });
                });
                juce::Thread::sleep (350);   // let the last tick be seen
                juce::MessageManager::callAsync ([safe]
                {
                    if (safe == nullptr) return;
                    safe->stopTimer();
                    safe->readInstalled();
                    safe->page = Page::done;
                    safe->refresh();
                    if (safe->args.autoRun)
                    {
                        for (int i = 0; i < setup::numPlugins; ++i)
                            std::printf ("%s %d\n", setup::plugins[(size_t) i].bundle, (int) safe->status[(size_t) i]);
                        juce::JUCEApplication::quit();
                    }
                });
            });
        }

        void timerCallback() override
        {
            for (int i = 0; i < setup::numPlugins; ++i)
                if (status[(size_t) i] == Status::working)
                    cards[(size_t) i]->repaint();
        }

        // 1 = minimise, 2 = close, 0 = neither
        int buttonAt (juce::Point<int> p) const
        {
            if (p.y > 44) return 0;
            if (p.x >= getWidth() - 50) return 2;
            if (p.x >= getWidth() - 96) return 1;
            return 0;
        }

        void drawWindowButtons (juce::Graphics& g)
        {
            const auto close = juce::Rectangle<float> ((float) getWidth() - 50.0f, 0.0f, 50.0f, 44.0f);
            const auto mini = close.translated (-46.0f, 0.0f);
            if (hover == 2 && ! busy())
            {
                juce::Path p;
                p.addRoundedRectangle (close.getX(), close.getY(), close.getWidth(), close.getHeight(), 14.0f, 14.0f, false, true, false, false);
                g.setColour (theme::red);
                g.fillPath (p);
            }
            if (hover == 1)
            {
                g.setColour (theme::track);
                g.fillRect (mini);
            }
            g.setColour (hover == 2 && ! busy() ? juce::Colours::white : busy() ? theme::track : theme::soft);
            const auto c = close.getCentre();
            g.drawLine (c.x - 6, c.y - 6, c.x + 6, c.y + 6, 1.4f);
            g.drawLine (c.x - 6, c.y + 6, c.x + 6, c.y - 6, 1.4f);
            g.setColour (theme::soft);
            const auto m = mini.getCentre();
            g.drawLine (m.x - 6, m.y, m.x + 6, m.y, 1.4f);
        }
    };

    //==========================================================================
    void Card::paint (juce::Graphics& g)
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool choosing = owner.picking();
        const bool on = owner.keep[(size_t) index];
        const bool hot = choosing && ! owner.removingAll() && isMouseOver();
        const auto& p = setup::plugins[(size_t) index];

        g.setColour (hot ? theme::panel.brighter (0.05f) : theme::panel);
        g.fillRoundedRectangle (r, theme::radius);
        g.setColour (owner.isOpen (index) ? theme::red : choosing && on ? theme::accent.withAlpha (0.85f) : theme::edge);
        g.drawRoundedRectangle (r, theme::radius, choosing && on ? 1.5f : 1.0f);

        // the plugin's own window, small
        const auto thumb = juce::Rectangle<float> (10.0f, 10.0f, 100.0f, r.getHeight() - 20.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            juce::Path clip;
            clip.addRoundedRectangle (thumb, 6.0f);
            g.reduceClipRegion (clip);
            g.setColour (theme::bg);
            g.fillRect (thumb);
            const auto& img = owner.shots[(size_t) index];
            if (img.isValid())
            {
                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                g.setOpacity (choosing && ! on ? 0.45f : 1.0f);
                g.drawImage (img, thumb, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop | juce::RectanglePlacement::fillDestination);
            }
        }
        g.setColour (theme::edge);
        g.drawRoundedRectangle (thumb, 6.0f, 1.0f);

        // name, then what happens to it
        const float x = 126.0f;
        g.setColour (choosing && ! on ? theme::soft : theme::text);
        g.setFont (juce::Font (vs::font (17.0f, true)).withExtraKerningFactor (0.03f));
        g.drawText (p.title, (int) x, 12, 160, 22, juce::Justification::centredLeft);
        const float nameW = juce::GlyphArrangement::getStringWidth (juce::Font (vs::font (17.0f, true)).withExtraKerningFactor (0.03f), p.title);
        juce::Colour c;
        const auto st = owner.statusText (index, c);
        g.setColour (c);
        g.setFont (vs::mono (10.5f, true));
        g.drawText (st, (int) (x + nameW + 12.0f), 14, 260, 20, juce::Justification::centredLeft);

        g.setColour (theme::soft);
        g.setFont (vs::font (13.5f));
        g.drawText (p.blurb, (int) x, 40, (int) r.getWidth() - 186, 20, juce::Justification::centredLeft, true);

        // right: the checkbox, or how it went
        const auto box = juce::Rectangle<float> (r.getRight() - 46.0f, r.getCentreY() - 13.0f, 26.0f, 26.0f);
        if (choosing && owner.removingAll())
            return;   // Uninstall: a plain list of what goes, no checkboxes
        if (choosing)
        {
            if (on)
            {
                g.setColour (hot ? theme::accentHover : theme::accent);
                g.fillRoundedRectangle (box, 7.0f);
                drawTick (g, box.reduced (3.0f), juce::Colours::white, 2.6f);
            }
            else
            {
                g.setColour (hot ? theme::soft : theme::dim);
                g.drawRoundedRectangle (box.reduced (0.75f), 7.0f, 1.5f);
            }
            return;
        }

        const auto s = owner.status[(size_t) index];
        if (s == Status::working)
        {
            juce::Path arc;
            const auto a = owner.spinnerAngle();
            arc.addCentredArc (box.getCentreX(), box.getCentreY(), 11.0f, 11.0f, a, 0.0f, juce::MathConstants<float>::pi * 1.4f, true);
            g.setColour (theme::accent);
            g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (s == Status::installed)
        {
            g.setColour (theme::green);
            g.fillEllipse (box);
            drawTick (g, box.reduced (3.5f), theme::bg, 2.6f);
        }
        else if (s == Status::removed || s == Status::unchanged)
        {
            g.setColour (theme::dim);
            g.drawEllipse (box.reduced (0.75f), 1.5f);
            g.drawLine (box.getCentreX() - 6.0f, box.getCentreY(), box.getCentreX() + 6.0f, box.getCentreY(), 1.8f);
        }
        else if (s == Status::inUse || s == Status::failed)
        {
            g.setColour (theme::red);
            g.fillEllipse (box);
            g.setColour (juce::Colours::white);
            g.setFont (vs::font (16.0f, true));
            g.drawText ("!", box, juce::Justification::centred);
        }
        else
        {
            g.setColour (theme::track);
            g.drawEllipse (box.reduced (0.75f), 1.5f);
        }
    }

    void Card::mouseUp (const juce::MouseEvent& e)
    {
        if (getLocalBounds().contains (e.getPosition()))
            owner.toggle (index);
    }

    //==========================================================================
    class SetupApp : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override    { return "ENCORE. VST Setup"; }
        const juce::String getApplicationVersion() override { return kVersion; }
        bool moreThanOneInstanceAllowed() override          { return false; }

        void initialise (const juce::String&) override
        {
           #if ENCORE_SETUP_DEV
            juce::SystemStats::setApplicationCrashHandler ([] (void*) { std::fputs (juce::SystemStats::getStackBacktrace().toRawUTF8(), stderr); std::fflush (stderr); });
           #endif
            const auto params = getCommandLineParameterArray();
            SetupComponent::Args a;
            for (int i = 0; i < params.size(); ++i)
            {
                const auto& p = params[i];
                if (p == "--uninstall") a.uninstall = true;
               #if ENCORE_SETUP_DEV
                else if (p == "--no-register") a.noRegister = true;
                else if (p == "--auto") a.autoRun = true;   // press the main button, print how it went, quit
                else if (p == "--dest" && i + 1 < params.size()) a.dest = juce::File (params[++i].unquoted());
                else if (p == "--shots" && i + 1 < params.size()) a.shots = juce::File (params[++i].unquoted());
               #endif
            }

            window = std::make_unique<SetupComponent> (a);
            if (a.shots != juce::File())
            {
                window->saveShots (a.shots);
                quit();
                return;
            }
            window->addToDesktop (juce::ComponentPeer::windowAppearsOnTaskbar | juce::ComponentPeer::windowHasDropShadow
                                  | juce::ComponentPeer::windowIsSemiTransparent);
            window->centreWithSize (window->getWidth(), window->getHeight());
            window->setVisible (true);
            window->toFront (true);
            window->grabKeyboardFocus();
           #if ENCORE_SETUP_DEV
            if (a.autoRun)
                juce::MessageManager::callAsync ([w = juce::Component::SafePointer<SetupComponent> (window.get())] { if (w != nullptr) w->pressPrimary(); });
           #endif
        }

        void shutdown() override { window = nullptr; }

        void systemRequestedQuit() override
        {
            if (window == nullptr || ! window->busy())
                quit();
        }

    private:
        std::unique_ptr<SetupComponent> window;
    };
}

START_JUCE_APPLICATION (SetupApp)
