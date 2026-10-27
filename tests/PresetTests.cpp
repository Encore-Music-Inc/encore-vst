// Round-trip check for the shared user-preset system.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include "ui/PresetManager.h"

namespace
{
    struct Dummy : juce::AudioProcessor
    {
        Dummy() : apvts (*this, nullptr, "Dummy", layout()) {}
        static juce::AudioProcessorValueTreeState::ParameterLayout layout()
        {
            juce::AudioProcessorValueTreeState::ParameterLayout l;
            l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gain", 1 }, "Gain", -12.0f, 12.0f, 0.0f));
            l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "on", 1 }, "On", true));
            l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", juce::StringArray { "A", "B", "C" }, 0));
            return l;
        }
        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
        juce::AudioProcessorValueTreeState apvts;
    };

    int failures = 0;
    void expect (const char* name, bool ok) { std::printf ("%s %s\n", ok ? "PASS" : "FAIL", name); if (! ok) ++failures; }
    void set (Dummy& d, const char* id, float v) { auto* p = d.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    float get (Dummy& d, const char* id) { return d.apvts.getRawParameterValue (id)->load(); }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    Dummy d;
    vs::PresetManager pm (d.apvts, "_selftest");

    set (d, "gain", 4.5f); set (d, "on", 0.0f); set (d, "mode", 2.0f);
    expect ("save writes a file", pm.save ("My Lead / Take 2"));   // slash must be made filename-safe
    expect ("saved preset is listed", pm.getUserPresets().contains (juce::File::createLegalFileName ("My Lead / Take 2")));

    set (d, "gain", -7.0f); set (d, "on", 1.0f); set (d, "mode", 0.0f);
    expect ("load succeeds", pm.load (juce::File::createLegalFileName ("My Lead / Take 2")));
    expect ("float restored", std::abs (get (d, "gain") - 4.5f) < 0.01f);
    expect ("switch restored", get (d, "on") < 0.5f);
    expect ("choice restored", (int) get (d, "mode") == 2);
    expect ("current name is the user preset", pm.isUser());

    expect ("loading a missing preset fails safely", ! pm.load ("does not exist"));
    expect ("delete removes it", pm.remove (juce::File::createLegalFileName ("My Lead / Take 2")) && pm.getUserPresets().isEmpty());

    pm.getFolder().getParentDirectory().deleteRecursively(); // clean up Documents\Encore\_selftest
    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
