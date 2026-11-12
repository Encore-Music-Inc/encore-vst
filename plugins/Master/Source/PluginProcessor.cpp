#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;
    using MC = vs::MasterChain;

    juce::String dB (float v)  { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    // The loudness targets' names, in the order of encore-audio-processing's list (MasterChain::targets)
    const juce::StringArray targetNames { "Streaming -14", "Balanced -11", "Loud -9" };

    // Only the styles fitted to measured reference masters are offered for now
    constexpr int offeredStyles = 4;

    juce::StringArray styleNames()
    {
        juce::StringArray s;
        for (int i = 0; i < offeredStyles; ++i) s.add (MC::style (i).name);
        return s;
    }

    // The factory presets: encore-audio-processing's list for this chain (dsp/Presets.h)
    const std::vector<vs::presets::Preset>& presets = vs::presets::forChain ("master");
}

MasterProcessor::MasterProcessor()
    : EncoreProcessorBase (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Master", createLayout())
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    prm.style = get ("style"); prm.intensity = get ("intensity"); prm.input = get ("input"); prm.target = get ("target");
    prm.ceiling = get ("ceiling"); prm.glue = get ("glue"); prm.width = get ("width"); prm.sat = get ("sat"); prm.compare = get ("compare");
    static const char* toneIds[] { "tone_low", "tone_lowmid", "tone_mid", "tone_pres", "tone_air" };
    for (int b = 0; b < MC::numTone; ++b) prm.tone[b] = get (toneIds[b]);
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto num = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, std::function<String (float)> text)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           Attr().withStringFromValueFunction ([text] (float v, int) { return text (v); })));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", styleNames(), MC::universal));
    num ("intensity", "Intensity", { 0.0f, 1.0f }, 0.5f, [] (float v) { return v > 0.495f && v < 0.505f ? juce::String ("Normal") : pct (v); });
    num ("input", "Input", { -12.0f, 12.0f, 0.1f }, 0.0f, dB);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Loudness", targetNames, 0));
    num ("ceiling", "Ceiling", { -3.0f, -0.1f, 0.1f }, -1.0f, [] (float v) { return String (v, 1) + " dBTP"; });

    num ("tone_low", "Low", { -6.0f, 6.0f, 0.1f }, 0.0f, dB);
    num ("tone_lowmid", "Low Mid", { -6.0f, 6.0f, 0.1f }, 0.0f, dB);
    num ("tone_mid", "Mid", { -6.0f, 6.0f, 0.1f }, 0.0f, dB);
    num ("tone_pres", "Presence", { -6.0f, 6.0f, 0.1f }, 0.0f, dB);
    num ("tone_air", "Air", { -6.0f, 6.0f, 0.1f }, 0.0f, dB);
    num ("glue", "Glue", { 0.0f, 1.0f }, 0.5f, [] (float v) { return v > 0.49f && v < 0.51f ? String ("Style") : pct (v * 2.0f); });
    num ("width", "Width", { -0.5f, 0.5f, 0.01f }, 0.0f, [] (float v) { return (v > 0.004f ? "+" : "") + String (roundToInt (v * 100.0f)) + "%"; });
    num ("sat", "Saturation", { -1.0f, 1.0f, 0.01f }, 0.0f, [] (float v) { return (v > 0.004f ? "+" : "") + String (roundToInt (v * 100.0f)) + "%"; });

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "compare", 1 }, "Hear Original", false));
    return layout;
}

bool MasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, samplesPerBlock);
    compare.setHearOriginal (prm.compare->load() > 0.5f);
    compare.prepare (sampleRate, samplesPerBlock, chain.getLatency());
    tpMeter.prepare (2, samplesPerBlock);
    setLatencySamples (chain.getLatency());
}

void MasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    if (learnRequested.exchange (false)) chain.startLearning();

    vs::MasterChain::Settings s;
    s.style = (int) prm.style->load();
    s.intensity = prm.intensity->load();
    s.inputGainDb = prm.input->load();
    s.targetLufs = MC::targetLufs ((int) prm.target->load());
    s.ceilingDb = prm.ceiling->load();
    s.glue = prm.glue->load();
    s.widthTrim = prm.width->load();
    s.satTrim = prm.sat->load();
    for (int b = 0; b < MC::numTone; ++b) s.toneTrim[b] = prm.tone[b]->load();
    chain.setSettings (s);

    // "Hear Original" plays the unprocessed mix, latency-aligned and at the
    // same loudness, so the comparison is about tone and punch, not volume.
    compare.takeOriginal (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
    chain.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    const float outST = chain.outMeter.shortTerm.load();
    compare.setHearOriginal (prm.compare->load() > 0.5f);
    compare.mix (buffer.getWritePointer (0), buffer.getWritePointer (1), n, outST);

    // Meters
    outShortTerm = outST;
    outMomentary = chain.outMeter.momentary.load();
    inShortTerm = compare.originalShortTerm();
    limiterGrDb = chain.limiter.grDb.load();
    learnProgress = chain.learnProgress();
    learning = chain.isLearning();
    settling = chain.isSettling();
    learnedValid = chain.getLearned().valid;
    for (int b = 0; b < MC::numTone; ++b) toneNow[b] = chain.toneDbNow (b);

    if (peakHoldReset.exchange (false)) tpMeter.reset();
    tpMeter.process (buffer.getArrayOfReadPointers(), n);
    truePeakHoldDb = std::max (-100.0f, tpMeter.truePeakDb());

    // When learning finishes, flag it so the UI thread copies the result
    // into the plugin state (it's also copied on every project save)
    if (lastLearning && ! chain.isLearning()) learnedChanged = true;
    lastLearning = chain.isLearning();
}

void MasterProcessor::syncLearnedToState()
{
    const auto& l = chain.getLearned();
    auto& st = apvts.state;
    st.setProperty ("learnValid", l.valid, nullptr);
    st.setProperty ("learnPre", l.preGainDb, nullptr);
    st.setProperty ("learnDrive", l.driveDb, nullptr);
    for (int b = 0; b < MC::numTone; ++b) st.setProperty ("learnTone" + juce::String (b), l.toneMatch[b], nullptr);
    for (int b = 0; b < 3; ++b) st.setProperty ("learnBand" + juce::String (b), l.bandLevelDb[b], nullptr);
}

int MasterProcessor::getNumPrograms() { return (int) presets.size(); }
const juce::String MasterProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) presets.size()) ? presets[(size_t) i].name : ""; }
juce::StringArray MasterProcessor::getFactoryPresetNames() const { return vs::factoryNames (presets); }
void MasterProcessor::setCurrentProgram (int index) { if (index != currentProgram) applyPreset (index); }

void MasterProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    vs::applyFactoryPreset (*this, presetManager, presets[(size_t) index]);
}

void MasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    syncLearnedToState();
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = state.getProperty ("program", 0);
    vs::PresetManager::restore (apvts, state);

    // What the plugin learned about this song comes back with the project
    vs::MasterChain::Learned l;
    l.valid = state.getProperty ("learnValid", false);
    if (l.valid)
    {
        l.preGainDb = state.getProperty ("learnPre", 0.0f);
        l.driveDb = state.getProperty ("learnDrive", 6.0f);
        for (int b = 0; b < MC::numTone; ++b) l.toneMatch[b] = state.getProperty ("learnTone" + juce::String (b), 0.0f);
        for (int b = 0; b < 3; ++b) l.bandLevelDb[b] = state.getProperty ("learnBand" + juce::String (b), -24.0f);
        suspendProcessing (true);
        chain.setLearned (l);
        suspendProcessing (false);
    }
}

juce::AudioProcessorEditor* MasterProcessor::createEditor() { return new MasterEditor (*this); }
#if ! BASEMENT_BUILTIN // Basement creates the processor itself
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterProcessor(); }

#endif
