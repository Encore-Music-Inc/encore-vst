#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetManager.h"
#include "dsp/Presets.h"

namespace vs
{
    // Built-in starting points (encore-audio-processing's presets list,
    // dsp/Presets.h). Anything a preset doesn't list falls back to the
    // parameter's default; choice values are list indexes.
    inline void applyFactoryPreset (juce::AudioProcessor& processor, PresetManager& manager, const presets::Preset& preset)
    {
        for (auto* param : processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
            if (ranged == nullptr) continue;

            float value = ranged->convertFrom0to1 (ranged->getDefaultValue());
            for (auto& v : preset.values)
                if (ranged->paramID == v.id)
                    value = (float) v.value;

            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
        }

        // Hosts may call this from the audio thread; the preset name lives in
        // the state tree, which is only safe to touch from the message thread.
        if (juce::MessageManager::existsAndIsCurrentThread())
            manager.setCurrent (preset.name, false);
    }

    inline juce::StringArray factoryNames (const std::vector<presets::Preset>& list)
    {
        juce::StringArray names;
        for (auto& p : list) names.add (p.name);
        return names;
    }
}
