#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String hz (float v)  { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }

    const juce::StringArray modeNames { "Room", "Chamber", "Plate", "Hall", "Huge", "Spring" };

    const std::vector<vs::FactoryPreset> presets
    {
        { "Clean Start", {} },
        { "Vocal Plate",       { { "mode", 2 }, { "mix", 0.25f }, { "predelay", 30 }, { "decay", 1.8f }, { "size", 0.5f }, { "tone", 9000 }, { "lowcut", 250 }, { "mod", 0.3f }, { "duck", 0.3f } } },
        { "Snare Room",        { { "mode", 0 }, { "mix", 0.2f }, { "predelay", 0 }, { "decay", 0.8f }, { "size", 0.35f }, { "tone", 7000 }, { "lowcut", 200 }, { "width", 1.1f }, { "mod", 0.15f } } },
        { "Ballad Chamber",    { { "mode", 1 }, { "mix", 0.25f }, { "predelay", 15 }, { "decay", 1.6f }, { "size", 0.6f }, { "tone", 7500 }, { "lowcut", 200 } } },
        { "Big Hall",          { { "mode", 3 }, { "mix", 0.3f }, { "predelay", 25 }, { "decay", 3.2f }, { "size", 0.75f }, { "tone", 6500 }, { "lowcut", 180 }, { "width", 1.2f }, { "mod", 0.35f } } },
        { "Ducked Vocal Hall", { { "mode", 3 }, { "mix", 0.3f }, { "predelay", 60 }, { "decay", 2.8f }, { "size", 0.65f }, { "tone", 7000 }, { "lowcut", 250 }, { "duck", 0.7f } } },
        { "Emo Ambient",       { { "mode", 4 }, { "mix", 0.35f }, { "predelay", 40 }, { "decay", 8.0f }, { "size", 0.9f }, { "tone", 6000 }, { "lowcut", 300 }, { "width", 1.3f }, { "mod", 0.6f }, { "duck", 0.4f }, { "shimmer", 0.35f } } },
        { "Surf Spring",       { { "mode", 5 }, { "mix", 0.35f }, { "predelay", 0 }, { "decay", 2.5f }, { "size", 0.5f }, { "tone", 4500 }, { "lowcut", 200 } } },
        { "Lo-fi Spring Vox",  { { "mode", 5 }, { "mix", 0.2f }, { "predelay", 10 }, { "decay", 1.2f }, { "size", 0.3f }, { "tone", 3500 }, { "lowcut", 400 } } },
    };
}

VerbProcessor::VerbProcessor()
    : EncoreProcessorBase (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Verb", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    prm = { get ("mode"), get ("mix"), get ("predelay"), get ("decay"), get ("size"), get ("tone"),
            get ("lowcut"), get ("width"), get ("mod"), get ("duck"), get ("shimmer") };
}

juce::AudioProcessorValueTreeState::ParameterLayout VerbProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto num = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, std::function<String (float)> text)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           Attr().withStringFromValueFunction ([text] (float v, int) { return text (v); })));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames, 2));
    num ("mix", "Mix", { 0.0f, 1.0f }, 0.3f, pct);
    num ("predelay", "Pre-Delay", skewed (0.0f, 250.0f, 40.0f), 20.0f, [] (float v) { return String (roundToInt (v)) + " ms"; });
    num ("decay", "Decay", skewed (0.2f, 20.0f, 2.5f), 2.0f, [] (float v) { return String (v, v < 10.0f ? 2 : 1) + " s"; });
    num ("size", "Size", { 0.0f, 1.0f }, 0.5f, pct);
    num ("tone", "Tone", skewed (1000.0f, 20000.0f, 6000.0f), 8000.0f, hz);
    num ("lowcut", "Low Cut", skewed (20.0f, 1000.0f, 200.0f), 150.0f, hz);
    num ("width", "Width", { 0.0f, 1.5f }, 1.0f, pct);
    num ("mod", "Movement", { 0.0f, 1.0f }, 0.3f, pct);
    num ("duck", "Ducking", { 0.0f, 1.0f }, 0.0f, pct);
    num ("shimmer", "Shimmer", { 0.0f, 1.0f }, 0.0f, pct);

    return layout;
}

bool VerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    return (in == mono && out == mono) || (in == stereo && out == stereo) || (in == mono && out == stereo);
}

void VerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    wet.setSize (2, samplesPerBlock);
    mixSmooth.reset (sampleRate, 0.05);
    mixSmooth.setCurrentAndTargetValue (prm.mix->load());
}

void VerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (n == 0 || inCh == 0) return;
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    vs::ReverbEngine::Settings s;
    s.mode = (int) prm.mode->load();
    s.predelayMs = prm.predelay->load();
    s.decaySec = prm.decay->load();
    s.size = prm.size->load();
    s.toneHz = prm.tone->load();
    s.lowCutHz = prm.lowCut->load();
    s.width = prm.width->load();
    s.mod = prm.mod->load();
    s.duck = prm.duck->load();
    s.shimmer = prm.shimmer->load();
    engine.setSettings (s);

    // mono in -> stereo out: feed the one channel to both sides
    if (outCh > inCh)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (buffer.getNumChannels() > 1 && outCh > 1 ? 1 : 0);
    engine.process (inL, inR, wet.getWritePointer (0), wet.getWritePointer (1), n);

    // Dry stays full up to 50% mix, wet reaches full at 50%: a send-style
    // curve where the middle of the knob is "both at full".
    mixSmooth.setTargetValue (prm.mix->load());
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSmooth.getNextValue();
        const float dry = std::min (1.0f, 2.0f * (1.0f - m)), wg = std::min (1.0f, 2.0f * m);
        const float wl = wet.getSample (0, i) * wg, wr = wet.getSample (1, i) * wg;
        peak = std::max ({ peak, std::abs (wl), std::abs (wr) });

        if (outCh == 1)
            buffer.setSample (0, i, buffer.getSample (0, i) * dry + 0.5f * (wl + wr));
        else
        {
            buffer.setSample (0, i, buffer.getSample (0, i) * dry + wl);
            buffer.setSample (1, i, buffer.getSample (1, i) * dry + wr);
        }
    }
    wetDb.store (vs::ampToDb (peak));
}

int VerbProcessor::getNumPrograms() { return (int) presets.size(); }
const juce::String VerbProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) presets.size()) ? presets[(size_t) i].name : ""; }
juce::StringArray VerbProcessor::getFactoryPresetNames() const { return vs::factoryNames (presets); }

void VerbProcessor::setCurrentProgram (int index)
{
    if (index != currentProgram) applyPreset (index); // hosts re-send the current one on load
}

void VerbProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    vs::applyFactoryPreset (*this, presetManager, presets[(size_t) index]);
}

void VerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void VerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            vs::PresetManager::restore (apvts, state);
        }
}

juce::AudioProcessorEditor* VerbProcessor::createEditor() { return new VerbEditor (*this); }
#if ! BASEMENT_BUILTIN // Basement creates the processor itself
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VerbProcessor(); }

#endif
