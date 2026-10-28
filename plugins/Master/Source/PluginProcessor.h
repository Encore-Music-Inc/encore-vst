#pragma once

#include "EncoreProcessorBase.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/MasterChain.h"
#include "ui/PresetManager.h"
#include "ui/FactoryPresets.h"

class MasterProcessor : public EncoreProcessorBase
{
public:
    MasterProcessor();

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
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void applyPreset (int index);
    juce::StringArray getFactoryPresetNames() const;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // UI -> audio: (re)learn this song
    void requestLearn() { learnRequested = true; }
    void resetPeakHold() { peakHoldReset = true; }

    juce::AudioProcessorValueTreeState apvts;
    vs::PresetManager presetManager { apvts, "Master" };

    // Audio -> UI
    std::atomic<float> outShortTerm { -100.0f }, outMomentary { -100.0f }, inShortTerm { -100.0f };
    std::atomic<float> truePeakHoldDb { -100.0f }, limiterGrDb { 0.0f }, learnProgress { 0.0f };
    std::atomic<float> toneNow[vs::MasterChain::numTone] {};
    std::atomic<bool> learning { false }, learnedValid { false }, settling { false };
    float outIntegrated() const { return chain.outMeter.integrated(); }

    // Message thread only
    void syncLearnedToState();
    std::atomic<bool> learnedChanged { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Params
    {
        std::atomic<float> *style, *intensity, *input, *target, *ceiling, *glue, *width, *sat, *compare;
        std::atomic<float>* tone[vs::MasterChain::numTone];
    } prm;

    vs::MasterChain chain;
    vs::LoudnessMeter rawInMeter;
    juce::dsp::Oversampling<float> tpMeterOs { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, false };
    juce::AudioBuffer<float> original, tpScratch;
    std::vector<float> origDelay[2];
    int origDelayLen = 1, origIdx = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> matchGain;
    juce::SmoothedValue<float> mix;
    std::atomic<bool> learnRequested { false }, peakHoldReset { false };
    bool lastLearning = false;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessor)
};
