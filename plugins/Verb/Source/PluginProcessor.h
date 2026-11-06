#pragma once

#include "EncoreProcessorBase.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ReverbEngine.h"
#include "ui/PresetManager.h"
#include "ui/FactoryPresets.h"

class VerbProcessor : public EncoreProcessorBase
{
public:
    VerbProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 25.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void applyPreset (int index);
    juce::StringArray getFactoryPresetNames() const;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    vs::PresetManager presetManager { apvts, "Verb" };

    std::atomic<float> wetDb { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Params
    {
        std::atomic<float> *mode, *mix, *predelay, *decay, *size, *tone, *lowCut, *width, *mod, *duck, *shimmer;
    } prm;

    vs::ReverbEngine engine;
    juce::AudioBuffer<float> wet;
    juce::SmoothedValue<float> mixSmooth;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerbProcessor)
};
