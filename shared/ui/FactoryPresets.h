#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetManager.h"

namespace vs
{
    // Built-in starting points. Anything a preset doesn't list falls back to
    // the parameter's default; choice values are list indexes.
    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<const char*, double>> values; // double so integer literals convert cleanly
    };

    inline void applyFactoryPreset (juce::AudioProcessor& processor, PresetManager& manager, const FactoryPreset& preset)
    {
        for (auto* param : processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
            if (ranged == nullptr) continue;

            float value = ranged->convertFrom0to1 (ranged->getDefaultValue());
            for (auto& [id, v] : preset.values)
                if (ranged->paramID == id)
                    value = (float) v;

            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
        }

        // Hosts may call this from the audio thread; the preset name lives in
        // the state tree, which is only safe to touch from the message thread.
        if (juce::MessageManager::existsAndIsCurrentThread())
            manager.setCurrent (preset.name, false);
    }

    inline juce::StringArray factoryNames (const std::vector<FactoryPreset>& presets)
    {
        juce::StringArray names;
        for (auto& p : presets) names.add (p.name);
        return names;
    }
}
