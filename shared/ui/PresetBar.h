#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"

namespace vs
{
    // Preset menu + Save / Delete / Folder buttons, shared by every plugin.
    // Factory presets come from the plugin; user presets from PresetManager.
    class PresetBar : public juce::Component
    {
    public:
        PresetBar (PresetManager& manager, juce::StringArray factoryNames, std::function<void (int)> applyFactory)
            : pm (manager), factory (std::move (factoryNames)), onFactory (std::move (applyFactory))
        {
            combo.beforePopup = [this] { refresh(); };
            combo.onChange = [this] { picked(); };
            combo.setTextWhenNothingSelected ("Choose a preset");

            saveBtn.onClick = [this] { askToSave(); };
            deleteBtn.onClick = [this] { askToDelete(); };
            folderBtn.onClick = [this] { pm.getFolder().startAsProcess(); };
            saveBtn.setTooltip ("Save the current settings as your own preset");
            deleteBtn.setTooltip ("Delete the selected user preset");
            folderBtn.setTooltip ("Open the presets folder (to back up or share .snspreset files)");

            for (auto* c : std::initializer_list<juce::Component*> { &combo, &saveBtn, &deleteBtn, &folderBtn })
                addAndMakeVisible (c);

            refresh();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            folderBtn.setBounds (r.removeFromRight (54).reduced (2, 0));
            deleteBtn.setBounds (r.removeFromRight (54).reduced (2, 0));
            saveBtn.setBounds (r.removeFromRight (50).reduced (2, 0));
            combo.setBounds (r.reduced (0, 0).withTrimmedRight (4));
        }

        // Rebuild the menu (presets may have been added by another instance)
        void refresh()
        {
            users = pm.getUserPresets();
            combo.clear (juce::dontSendNotification);

            combo.addSectionHeading ("Factory");
            for (int i = 0; i < factory.size(); ++i)
                combo.addItem (factory[i], i + 1);

            combo.addSectionHeading ("Your presets");
            if (users.isEmpty())
            {
                combo.addItem ("(none yet: click Save)", noneId);
                combo.setItemEnabled (noneId, false);
            }
            for (int i = 0; i < users.size(); ++i)
                combo.addItem (users[i], userBase + i);

            const auto current = pm.getCurrentName();
            const int idx = pm.isUser() ? users.indexOf (current) : factory.indexOf (current);
            if (idx >= 0)
                combo.setSelectedId (pm.isUser() ? userBase + idx : idx + 1, juce::dontSendNotification);
            else
                combo.setText (current, juce::dontSendNotification);

            deleteBtn.setEnabled (pm.isUser() && idx >= 0);
        }

    private:
        struct RefreshingCombo : juce::ComboBox
        {
            std::function<void()> beforePopup;
            void showPopup() override
            {
                if (beforePopup) beforePopup();
                juce::ComboBox::showPopup();
            }
        };

        static constexpr int userBase = 1000, noneId = 999;

        void picked()
        {
            const int id = combo.getSelectedId();
            if (id >= 1 && id <= factory.size())
            {
                onFactory (id - 1);
                pm.setCurrent (factory[id - 1], false);
            }
            else if (id >= userBase && id - userBase < users.size())
            {
                pm.load (users[id - userBase]);
            }
            refresh();
        }

        void askToSave()
        {
            auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("name", pm.isUser() ? pm.getCurrentName() : juce::String());
            w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            juce::Component::SafePointer<PresetBar> safe (this);
            showInside (w, [safe, w] (int result)
            {
                if (safe == nullptr || result != 1) return;
                const auto name = w->getTextEditorContents ("name").trim();
                if (name.isEmpty()) return;

                if (safe->pm.exists (name))
                    safe->confirm ("Overwrite preset?", "\"" + name + "\" already exists. Replace it with the current settings?",
                                   "Overwrite", [safe, name] { if (safe != nullptr) safe->doSave (name); });
                else
                    safe->doSave (name);
            });
        }

        void doSave (const juce::String& name)
        {
            if (! pm.save (name))
            {
                auto* w = new juce::AlertWindow ("Couldn't save", "Couldn't write to " + pm.getFolder().getFullPathName(),
                                                 juce::MessageBoxIconType::WarningIcon);
                w->addButton ("OK", 0, juce::KeyPress (juce::KeyPress::returnKey));
                showInside (w, [] (int) {});
            }
            refresh();
        }

        void askToDelete()
        {
            if (! pm.isUser()) return;
            const auto name = pm.getCurrentName();
            juce::Component::SafePointer<PresetBar> safe (this);
            confirm ("Delete preset?", "Delete \"" + name + "\"? This can't be undone.", "Delete",
                     [safe, name] { if (safe != nullptr) { safe->pm.remove (name); safe->refresh(); } });
        }

        void confirm (const juce::String& title, const juce::String& message, const juce::String& okText, std::function<void()> onOk)
        {
            auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::QuestionIcon);
            w->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            showInside (w, [onOk] (int result) { if (result == 1) onOk(); });
        }

        // Dialogs are drawn inside the plugin window rather than as separate
        // desktop windows, which some hosts open behind their own window.
        void showInside (juce::AlertWindow* w, std::function<void (int)> callback)
        {
            auto* top = getTopLevelComponent();
            top->addAndMakeVisible (w);
            w->setCentrePosition (top->getLocalBounds().getCentre());
            w->enterModalState (true, juce::ModalCallbackFunction::create (std::move (callback)), true);
            if (auto* editor = w->getTextEditor ("name"))
                editor->grabKeyboardFocus();
        }

        PresetManager& pm;
        juce::StringArray factory, users;
        std::function<void (int)> onFactory;
        RefreshingCombo combo;
        juce::TextButton saveBtn { "Save" }, deleteBtn { "Delete" }, folderBtn { "Folder" };
    };
}
