#pragma once

#include "EncoreProcessorBase.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/EqBand.h"
#include "dsp/Util.h"
#include "ui/PresetManager.h"
#include "ui/FactoryPresets.h"

class EqProcessor : public EncoreProcessorBase
{
public:
    static constexpr int numBands = 8, numSmart = 4;

    EqProcessor();

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void applyPreset (int index);
    juce::StringArray getFactoryPresetNames() const;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Current parameter values as DSP settings (used by audio and the display)
    vs::EqBand::Settings readBand (int i) const;
    vs::DynamicBand::Settings readSmart (int i) const;
    vs::Carve::Settings readCarve() const;
    double getRate() const { return rate.load(); }

    juce::AudioProcessorValueTreeState apvts;
    vs::PresetManager presetManager { apvts, "Equalize", "EQ" };

    // Audio -> UI: spectrum samples (mono) before and after the EQ
    struct Tap
    {
        juce::AbstractFifo fifo { 1 << 15 };
        std::vector<float> buf = std::vector<float> (1 << 15, 0.0f);

        void push (const float* x, int n)
        {
            int s1, b1, s2, b2;
            fifo.prepareToWrite (n, s1, b1, s2, b2);
            if (b1 > 0) std::copy (x, x + b1, buf.begin() + s1);
            if (b2 > 0) std::copy (x + b1, x + b1 + b2, buf.begin() + s2);
            fifo.finishedWrite (b1 + b2);
        }

        int pull (float* dest, int max)
        {
            int s1, b1, s2, b2;
            fifo.prepareToRead (max, s1, b1, s2, b2);
            if (b1 > 0) std::copy (buf.begin() + s1, buf.begin() + s1 + b1, dest);
            if (b2 > 0) std::copy (buf.begin() + s2, buf.begin() + s2 + b2, dest + b1);
            fifo.finishedRead (b1 + b2);
            return b1 + b2;
        }
    };
    Tap preTap, postTap;

    std::atomic<float> smartCutDb[numSmart] {};
    std::atomic<float> carveGainDb[vs::Carve::numBands] {};
    std::atomic<bool> sidechainLive { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct BandParams { std::atomic<float> *on, *type, *freq, *gain, *q, *slope; };
    struct SmartParams { std::atomic<float> *on, *freq, *q, *range, *thresh; };
    BandParams bandP[numBands];
    SmartParams smartP[numSmart];
    std::atomic<float> *carveOn, *carveDepth, *carveLow, *carveHigh, *carveSpeed, *outputP;

    vs::EqBand bands[numBands];
    vs::DynamicBand smart[numSmart];
    vs::Carve carve;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGain;
    std::vector<float> monoScratch;
    std::atomic<double> rate { 48000.0 };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqProcessor)
};
