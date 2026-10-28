#pragma once

// Every Encore plugin's processor derives from EncoreProcessorBase.
//
// Built as a VST3 (this repo) it's just juce::AudioProcessor. Built into
// Encore: Basement (BASEMENT_BUILTIN=1) it's a juce::AudioPluginInstance, so
// Basement can host the processor directly, with its parameters, automation
// and saved state, without loading a VST3 at all.

#include <juce_audio_processors/juce_audio_processors.h>

#if BASEMENT_BUILTIN
struct EncoreProcessorBase : public juce::AudioPluginInstance
{
    using juce::AudioPluginInstance::AudioPluginInstance;

    void fillInPluginDescription (juce::PluginDescription& d) const override
    {
        d.name = d.descriptiveName = getName();
        d.pluginFormatName = "Encore";
        d.category = "Effect";
        d.manufacturerName = "Encore";
        d.version = "1.0";
        d.fileOrIdentifier = "encore:" + getName();
        d.uniqueId = d.deprecatedUid = getName().hashCode();
        d.isInstrument = false;
        d.numInputChannels = d.numOutputChannels = 2;
    }
};
#else
using EncoreProcessorBase = juce::AudioProcessor;
#endif
