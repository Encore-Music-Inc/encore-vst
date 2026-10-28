#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;
    using MC = vs::MasterChain;

    juce::String dB (float v)  { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    const juce::StringArray targetNames { "Streaming -14", "Balanced -11", "Loud -9" };
    const float targetLufs[] { -14.0f, -11.0f, -9.0f };

    // Only the styles fitted to measured BandLab masters are offered for now
    constexpr int offeredStyles = 4;

    juce::StringArray styleNames()
    {
        juce::StringArray s;
        for (int i = 0; i < offeredStyles; ++i) s.add (MC::style (i).name);
        return s;
    }

    const std::vector<vs::FactoryPreset> presets
    {
        { "Default (Universal, Streaming)", {} },
        { "Folk / Acoustic",       { { "style", MC::universal }, { "intensity", 0.35 }, { "target", 0 } } },
        { "Punk / Loud",           { { "style", MC::fire },      { "intensity", 0.6 }, { "target", 2 } } },
        { "Emo / Big Chorus",      { { "style", MC::fire },      { "intensity", 0.45 }, { "target", 1 } } },
        { "Lo-fi Demo",            { { "style", MC::tape },      { "intensity", 0.6 }, { "target", 0 }, { "tone_air", -1.5 } } },
        { "Clear Vocal Mix",       { { "style", MC::clarity },   { "intensity", 0.5 }, { "target", 0 } } },
    };
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
    rawInMeter.prepare (sampleRate);
    tpMeterOs.initProcessing ((size_t) samplesPerBlock);
    tpScratch.setSize (2, samplesPerBlock);
    original.setSize (2, samplesPerBlock);

    origDelayLen = std::max (1, chain.getLatency());
    for (auto& d : origDelay) d.assign ((size_t) origDelayLen, 0.0f);
    origIdx = 0;
    matchGain.reset (sampleRate, 0.5);
    mix.reset (sampleRate, 0.03);
    mix.setCurrentAndTargetValue (prm.compare->load() > 0.5f ? 0.0f : 1.0f);
    setLatencySamples (chain.getLatency());
}

void MasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;
    if (original.getNumSamples() < n) { original.setSize (2, n, false, false, true); tpScratch.setSize (2, n, false, false, true); }

    if (learnRequested.exchange (false)) chain.startLearning();

    vs::MasterChain::Settings s;
    s.style = (int) prm.style->load();
    s.intensity = prm.intensity->load();
    s.inputGainDb = prm.input->load();
    s.targetLufs = targetLufs[juce::jlimit (0, 2, (int) prm.target->load())];
    s.ceilingDb = prm.ceiling->load();
    s.glue = prm.glue->load();
    s.widthTrim = prm.width->load();
    s.satTrim = prm.sat->load();
    for (int b = 0; b < MC::numTone; ++b) s.toneTrim[b] = prm.tone[b]->load();
    chain.setSettings (s);

    // Keep a latency-aligned copy of the untouched input for the A/B
    rawInMeter.process (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
    for (int c = 0; c < 2; ++c)
    {
        const float* in = buffer.getReadPointer (c);
        float* o = original.getWritePointer (c);
        int idx = origIdx;
        for (int i = 0; i < n; ++i) { o[i] = origDelay[c][(size_t) idx]; origDelay[c][(size_t) idx] = in[i]; idx = (idx + 1) % origDelayLen; }
    }
    origIdx = (origIdx + n) % origDelayLen;

    chain.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);

    // "Hear Original" plays the unprocessed mix at the *same loudness*, so
    // the comparison is about tone and punch, not just volume.
    const float outST = chain.outMeter.shortTerm.load(), inST = rawInMeter.shortTerm.load();
    if (outST > -60.0f && inST > -60.0f) matchGain.setTargetValue (vs::dbToGain (juce::jlimit (-24.0f, 24.0f, outST - inST)));
    mix.setTargetValue (prm.compare->load() > 0.5f ? 0.0f : 1.0f);
    if (mix.isSmoothing() || mix.getTargetValue() < 0.5f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = mix.getNextValue(), g = matchGain.getNextValue();
            for (int c = 0; c < 2; ++c)
            {
                float* p = buffer.getWritePointer (c);
                p[i] = p[i] * m + original.getSample (c, i) * g * (1.0f - m);
            }
        }
    }
    else
    {
        matchGain.skip (n);
    }

    // Meters
    outShortTerm = outST;
    outMomentary = chain.outMeter.momentary.load();
    inShortTerm = inST;
    limiterGrDb = chain.limiter.grDb.load();
    learnProgress = chain.learnProgress();
    learning = chain.isLearning();
    settling = chain.isSettling();
    learnedValid = chain.getLearned().valid;
    for (int b = 0; b < MC::numTone; ++b) toneNow[b] = chain.toneDbNow (b);

    for (int c = 0; c < 2; ++c) tpScratch.copyFrom (c, 0, buffer, c, 0, n);
    juce::dsp::AudioBlock<float> tb (tpScratch.getArrayOfWritePointers(), 2, (size_t) n);
    auto up = tpMeterOs.processSamplesUp (tb);
    float peak = 0.0f;
    for (size_t c = 0; c < 2; ++c)
        for (size_t i = 0; i < up.getNumSamples(); ++i) peak = std::max (peak, std::abs (up.getSample ((int) c, (int) i)));
    if (peakHoldReset.exchange (false)) truePeakHoldDb = -100.0f;
    truePeakHoldDb = std::max (truePeakHoldDb.load(), vs::ampToDb (peak));

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
