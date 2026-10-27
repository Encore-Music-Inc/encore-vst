#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace vs
{
    // User presets for any Encore plugin. Each preset is a small XML file
    //   Documents\Encore\<Plugin>\Presets\<Name>.encorepreset
    // so people can back them up, email them, or drop in ones others made.
    // Presets saved before the plugins were renamed (Documents\Sick & Sad\...,
    // .snspreset) are copied over the first time the folder is opened.
    class PresetManager
    {
    public:
        static constexpr const char* extension = ".encorepreset";
        static constexpr const char* legacyExtension = ".snspreset";

        PresetManager (juce::AudioProcessorValueTreeState& state, const juce::String& pluginName, const juce::String& legacyName = {})
            : apvts (state),
              folder (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("Encore").getChildFile (pluginName).getChildFile ("Presets")),
              legacyFolder (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("Sick & Sad").getChildFile (legacyName.isEmpty() ? pluginName : legacyName).getChildFile ("Presets"))
        {
        }

        juce::File getFolder() const
        {
            if (! folder.isDirectory())
            {
                folder.createDirectory();
                // bring presets over from before the rename (the old files stay where they were)
                for (auto& f : legacyFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + legacyExtension))
                    f.copyFileTo (folder.getChildFile (f.getFileNameWithoutExtension() + extension));
            }
            return folder;
        }

        juce::StringArray getUserPresets() const
        {
            juce::StringArray names;
            for (auto& f : getFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
                names.add (f.getFileNameWithoutExtension());
            names.sortNatural();
            return names;
        }

        bool exists (const juce::String& name) const { return fileFor (name).existsAsFile(); }

        bool save (const juce::String& name)
        {
            setCurrent (name, true);
            auto xml = apvts.copyState().createXml();
            return xml != nullptr && xml->writeTo (fileFor (name));
        }

        bool load (const juce::String& name)
        {
            auto xml = juce::XmlDocument::parse (fileFor (name));
            if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
                return false;

            restore (apvts, juce::ValueTree::fromXml (*xml));
            setCurrent (name, true);
            return true;
        }

        bool remove (const juce::String& name)
        {
            const bool ok = fileFor (name).deleteFile();
            if (ok && isUser() && getCurrentName() == name)
                setCurrent ({}, false);
            return ok;
        }

        // Which preset is loaded travels with the plugin's saved state, so a
        // reopened Cubase project still shows the right name.
        juce::String getCurrentName() const { return apvts.state.getProperty ("presetName").toString(); }
        bool isUser() const                 { return (bool) apvts.state.getProperty ("presetIsUser", false); }

        void setCurrent (const juce::String& name, bool user)
        {
            apvts.state.setProperty ("presetName", name, nullptr);
            apvts.state.setProperty ("presetIsUser", user, nullptr);
        }

        // replaceState() skips parameters whose stored value looks unchanged,
        // which can leave an on/off switch half-way. This snaps them.
        static void restore (juce::AudioProcessorValueTreeState& state, const juce::ValueTree& tree)
        {
            state.replaceState (tree);

            for (auto* param : state.processor.getParameters())
                if (auto* b = dynamic_cast<juce::AudioParameterBool*> (param))
                    b->setValueNotifyingHost (tree.getChildWithProperty ("id", b->paramID)
                                                  .getProperty ("value", 1.0f).operator float() > 0.5f ? 1.0f : 0.0f);
        }

    private:
        juce::File fileFor (const juce::String& name) const
        {
            return getFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + extension);
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::File folder, legacyFolder;
    };
}
