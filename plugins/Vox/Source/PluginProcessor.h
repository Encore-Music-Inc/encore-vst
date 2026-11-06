#pragma once

#include "EncoreProcessorBase.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/Rider.h"
#include "dsp/Gate.h"
#include "dsp/Saturator.h"
#include "dsp/DeEsser.h"
#include "dsp/Compressor.h"
#include "dsp/Space.h"
#include "dsp/PitchEngine.h"
#include "ui/PresetManager.h"

class VocalSuiteProcessor : public EncoreProcessorBase
{
public:
    VocalSuiteProcessor();

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    // Always applies, even if it's already the current preset (used by the UI
    // so re-picking a preset resets any tweaks).
    void applyPreset (int index);

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    vs::PresetManager presetManager { apvts, "Vox" };
    juce::StringArray getFactoryPresetNames();

    // Meter readouts for the UI (written on the audio thread)
    std::atomic<float> inputDb { -100.0f }, outputDb { -100.0f };
    const std::atomic<float>& riderDb() const   { return rider.meterDb; }
    const std::atomic<float>& deEssDb() const   { return deEsser.meterDb; }
    const std::atomic<float>& compDb() const    { return comp.meterDb; }
    const std::atomic<float>& detectedMidi() const    { return pitch.detectedMidi; }
    const std::atomic<float>& correctionCents() const { return pitch.correctionCents; }
    bool isPitchActive() const { return pitchActive; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters();

    // Cached raw parameter pointers so the audio thread never looks up by name
    struct Params
    {
        std::atomic<float> *rideOn, *rideTarget, *rideRange, *rideSpeed,
                           *cleanOn, *hpf, *gate, *deess, *deessFreq,
                           *toneOn, *drive, *low, *body, *presence, *air,
                           *dynOn, *comp, *compStyle,
                           *spaceOn, *verb, *verbSize, *delay, *delayTime, *delayFb,
                           *output,
                           *tuneOn, *key, *scale, *tuneSpeed, *tuneAmount,
                           *harmOn, *h1Int, *h1Level, *h1Pan, *h2Int, *h2Level, *h2Pan, *formant, *humanize;
    } prm;

    vs::PitchEngine pitch;
    juce::AudioBuffer<float> harm;
    std::vector<float> mono;
    std::atomic<bool> pitchActive { false };

    vs::Rider rider;
    vs::Gate gate;
    vs::Saturator saturator;
    vs::DeEsser deEsser;
    vs::Compressor comp;
    vs::Space space;

    using Filter = juce::dsp::IIR::Filter<float>;
    Filter hpf1[2], hpf2[2], lowEq[2], bodyEq[2], presEq[2], airEq[2];
    float lastHpf = -1, lastLow = -100, lastBody = -100, lastPres = -100, lastAir = -100;

    juce::AudioBuffer<float> work;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGain;
    double fs = 44100.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalSuiteProcessor)
};
