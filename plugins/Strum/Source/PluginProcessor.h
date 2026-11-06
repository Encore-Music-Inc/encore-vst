#pragma once

#include "EncoreProcessorBase.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/BodyMatch.h"
#include "dsp/GuitarFixes.h"
#include "dsp/Doubler.h"
#include "dsp/Util.h"
#include "ui/PresetManager.h"
#include "ui/FactoryPresets.h"

// Encore: Strum, the acoustic guitar fixer. One section per real problem:
//   Body    make a pickup/DI sound like a mic (Learn from a mic on the side-chain, or a generic curve)
//   Boom    tame a soundhole mic's low-end build-up
//   Squeak  turn down finger squeaks on slides
//   Pick    soften hard pick attacks
//   Double  fake double-track for width (mono-safe)
class StrumProcessor : public EncoreProcessorBase,
                       private juce::Timer,
                       private juce::ValueTree::Listener
{
public:
    StrumProcessor();
    ~StrumProcessor() override;

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
    double getTailLengthSeconds() const override { return 0.1; }

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
    vs::PresetManager presetManager { apvts, "Strum" };

    // UI (message thread)
    void toggleLearn();                 // start learning, or finish early
    void forgetLearned();
    const vs::BodyCurve& getLearned() const { return learned; }
    vs::BodyCurve getActiveCurve() const; // what Body is using right now (learned or generic)
    bool isUsingLearned() const;
    const vs::BodyLearner& getLearner() const { return learner; }

    // Message-thread housekeeping (collect a finished Learn, rebuild the Body
    // filter when its curve or Amount changed). Runs on a timer; public so
    // the offline tests can drive it.
    void service();

    // Audio -> UI
    std::atomic<float> inputDb { -100.0f }, outputDb { -100.0f };
    std::atomic<bool> sidechainLive { false };
    std::atomic<float> sidechainDb { -100.0f };
    float boomCutDb() const   { return boom.meterDb.load(); }
    float boomHz() const      { return boom.foundMeterHz.load(); }
    float squeakCutDb() const { return squeak.meterDb.load(); }
    float pickCutDb() const   { return pick.meterDb.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override { service(); }
    void valueTreeRedirected (juce::ValueTree&) override;
    void redesignBody (bool force);
    void writeLearnedToState();

    struct Params
    {
        std::atomic<float> *bodyOn, *bodyCurve, *bodyAmount, *boomOn, *boom, *squeakOn, *squeak,
                           *pickOn, *pick, *doubleOn, *width, *output, *compare;
    } prm;

    vs::BodyLearner learner;
    vs::BodyFilter body;
    vs::BoomTamer boom;
    vs::SqueakTamer squeak;
    vs::PickTamer pick;
    vs::Doubler doubler;

    // Message thread
    vs::BodyCurve learned;
    struct Designed { vs::BodyCurve curve; float amount = -1.0f; double rate = 0.0; } designed;
    juce::CriticalSection designLock;
    bool restoringProject = false;

    juce::AudioBuffer<float> work, wet, dry;
    std::vector<float> diMono, micMono;
    juce::SmoothedValue<float> bodyMix, compareMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGain, matchGain;
    float wetPow = 0.0f, dryPow = 0.0f, powCoef = 0.0f;
    bool bodyWasIdle = true;
    double fs = 48000.0;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrumProcessor)
};
