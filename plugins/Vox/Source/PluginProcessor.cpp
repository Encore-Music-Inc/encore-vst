#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    Attr withText (std::function<juce::String (float)> fn)
    {
        return Attr().withStringFromValueFunction ([fn] (float v, int) { return fn (v); });
    }

    juce::String dB (float v)      { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String pct (float v)     { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String hz (float v)      { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }

    const juce::StringArray delayTimes { "Slap", "1/16", "1/8", "Dotted 1/8", "1/4", "1/2" };
    const juce::StringArray compStyles { "Smooth", "Punchy", "Slam" };

    // The factory presets: encore-audio-processing's list for this chain (dsp/Presets.h)
    const std::vector<vs::presets::Preset>& presets = vs::presets::forChain ("vox");
}

VocalSuiteProcessor::VocalSuiteProcessor()
    : EncoreProcessorBase (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VocalSuite", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    prm = { get ("ride_on"), get ("ride_target"), get ("ride_range"), get ("ride_speed"),
            get ("clean_on"), get ("hpf"), get ("gate"), get ("deess"), get ("deess_freq"),
            get ("tone_on"), get ("drive"), get ("low"), get ("body"), get ("presence"), get ("air"),
            get ("dyn_on"), get ("comp"), get ("comp_style"),
            get ("space_on"), get ("verb"), get ("verb_size"), get ("delay"), get ("delay_time"), get ("delay_fb"),
            get ("output"),
            get ("tune_on"), get ("key"), get ("scale"), get ("tune_speed"), get ("tune_amount"),
            get ("harm_on"), get ("h1_int"), get ("h1_level"), get ("h1_pan"), get ("h2_int"), get ("h2_level"), get ("h2_pan"),
            get ("formant"), get ("humanize") };
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalSuiteProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto toggle = [&] (const char* id, const char* name, bool def = true)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto choice = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto panText = [] (float v)
    {
        const int p = roundToInt (v * 100.0f);
        return p == 0 ? String ("C") : (p < 0 ? "L" : "R") + String (std::abs (p));
    };
    auto num = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, std::function<String (float)> text)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def, withText (text)));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    toggle ("ride_on", "Ride On");
    num ("ride_target", "Target", { -30.0f, -6.0f, 0.1f }, -18.0f, [] (float v) { return String (v, 1) + " dB"; });
    num ("ride_range", "Range", { 0.0f, 12.0f, 0.1f }, 6.0f, [] (float v) { return String::charToString (0xB1) + String (v, 1) + " dB"; });
    num ("ride_speed", "Speed", { 0.0f, 1.0f }, 0.5f, pct);

    toggle ("clean_on", "Clean On");
    num ("hpf", "Low Cut", skewed (20.0f, 400.0f, 100.0f), 80.0f, [] (float v) { return v < 21.0f ? String ("Off") : hz (v); });
    num ("gate", "Gate", { -90.0f, -30.0f, 0.5f }, -90.0f, [] (float v) { return v < -89.5f ? String ("Off") : String (v, 1) + " dB"; });
    num ("deess", "De-Ess", { 0.0f, 1.0f }, 0.4f, pct);
    num ("deess_freq", "Ess Freq", skewed (3000.0f, 10000.0f, 6000.0f), 6000.0f, hz);

    toggle ("tone_on", "Tone On");
    num ("drive", "Drive", { 0.0f, 1.0f }, 0.15f, pct);
    num ("low", "Low", { -12.0f, 12.0f, 0.1f }, 0.0f, dB);
    num ("body", "Body", { -12.0f, 12.0f, 0.1f }, 0.0f, dB);
    num ("presence", "Presence", { -12.0f, 12.0f, 0.1f }, 0.0f, dB);
    num ("air", "Air", { -12.0f, 12.0f, 0.1f }, 0.0f, dB);

    toggle ("dyn_on", "Dynamics On");
    num ("comp", "Comp", { 0.0f, 1.0f }, 0.35f, pct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "comp_style", 1 }, "Style", compStyles, 0));

    toggle ("space_on", "Space On");
    num ("verb", "Reverb", { 0.0f, 1.0f }, 0.15f, pct);
    num ("verb_size", "Size", { 0.0f, 1.0f }, 0.45f, pct);
    num ("delay", "Delay", { 0.0f, 1.0f }, 0.0f, pct);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "delay_time", 1 }, "Time", delayTimes, 2));
    num ("delay_fb", "Repeats", { 0.0f, 0.85f }, 0.25f, pct);

    num ("output", "Output", { -18.0f, 18.0f, 0.1f }, 0.0f, dB);

    toggle ("tune_on", "Tune On", false);
    choice ("key", "Key", vs::scales::keyNames, 0);
    choice ("scale", "Scale", vs::scales::scaleNames, 0);
    num ("tune_speed", "Retune", skewed (0.0f, 200.0f, 40.0f), 40.0f, [] (float v) { return v < 0.5f ? String ("Hard") : String (roundToInt (v)) + " ms"; });
    num ("tune_amount", "Amount", { 0.0f, 1.0f }, 1.0f, pct);

    toggle ("harm_on", "Harmony On", false);
    choice ("h1_int", "Voice 1", vs::scales::intervalNames, 7);
    num ("h1_level", "V1 Level", { -24.0f, 6.0f, 0.1f }, -6.0f, dB);
    num ("h1_pan", "V1 Pan", { -1.0f, 1.0f, 0.01f }, -0.4f, panText);
    choice ("h2_int", "Voice 2", vs::scales::intervalNames, 0);
    num ("h2_level", "V2 Level", { -24.0f, 6.0f, 0.1f }, -6.0f, dB);
    num ("h2_pan", "V2 Pan", { -1.0f, 1.0f, 0.01f }, 0.4f, panText);
    num ("formant", "Formant", { -3.0f, 3.0f, 0.1f }, 0.0f, [] (float v) { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; });
    num ("humanize", "Humanize", { 0.0f, 1.0f }, 0.5f, pct);

    return layout;
}

bool VocalSuiteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto monoSet = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    return (in == monoSet && out == monoSet) || (in == stereo && out == stereo) || (in == monoSet && out == stereo);
}

void VocalSuiteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    work.setSize (2, samplesPerBlock);

    rider.prepare (sampleRate);
    gate.prepare (sampleRate);
    saturator.prepare (sampleRate, samplesPerBlock);
    deEsser.prepare (spec);
    comp.prepare (sampleRate);
    space.prepare (spec);
    pitch.prepare (sampleRate);
    harm.setSize (2, samplesPerBlock);
    mono.assign ((size_t) samplesPerBlock, 0.0f);

    updateFilters();
    eq.prepare (sampleRate);
    eq.reset();

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTarget (vs::dbToGain (prm.output->load()));

    pitchActive = prm.tuneOn->load() > 0.5f || prm.harmOn->load() > 0.5f;
    setLatencySamples (saturator.getLatencySamples() + (pitchActive ? pitch.getLatency() : 0));
}

void VocalSuiteProcessor::updateFilters()
{
    vs::VocalEq::Settings s;
    s.lowCutHz = prm.hpf->load();
    s.lowDb = prm.low->load();
    s.bodyDb = prm.body->load();
    s.presenceDb = prm.presence->load();
    s.airDb = prm.air->load();
    eq.setSettings (s);
}

void VocalSuiteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = getTotalNumOutputChannels();
    if (n == 0 || inCh == 0) return;

    if (work.getNumSamples() < n)
    {
        work.setSize (2, n, false, false, true);
        harm.setSize (2, n, false, false, true);
        mono.resize ((size_t) n);
    }

    // Everything runs in stereo internally; a mono input is duplicated.
    work.copyFrom (0, 0, buffer, 0, 0, n);
    work.copyFrom (1, 0, buffer, inCh > 1 ? 1 : 0, 0, n);
    inputDb.store (vs::ampToDb (vs::channels::peakOf (work.getArrayOfReadPointers(), 2, n)));

    float* const* d = work.getArrayOfWritePointers();
    const bool rideOn = prm.rideOn->load() > 0.5f;
    const bool cleanOn = prm.cleanOn->load() > 0.5f;
    const bool toneOn = prm.toneOn->load() > 0.5f;
    const bool dynOn = prm.dynOn->load() > 0.5f;
    const bool spaceOn = prm.spaceOn->load() > 0.5f;

    updateFilters();

    // 1. Clean: low cut + gate
    if (cleanOn)
    {
        eq.processLowCut (d, 2, n);

        gate.setThreshold (prm.gate->load());
        gate.process (d, 2, n);
    }

    // 2. Pitch: tune the lead and generate harmonies. The engine needs ~35 ms
    // of look-ahead, so it only runs (and only adds latency) while Tune or
    // Harmony is switched on. The lead becomes mono while it's active.
    const bool tuneOn = prm.tuneOn->load() > 0.5f;
    const bool harmOn = prm.harmOn->load() > 0.5f;
    const bool active = tuneOn || harmOn;
    if (active != pitchActive.load())
    {
        pitchActive = active;
        pitch.reset();
        setLatencySamples (saturator.getLatencySamples() + (active ? pitch.getLatency() : 0));
    }

    float* const h[2] { harm.getWritePointer (0), harm.getWritePointer (1) };
    if (active)
    {
        vs::PitchEngine::Settings s;
        s.tuneOn = tuneOn;
        s.key = (int) prm.key->load();
        s.scale = (int) prm.scale->load();
        s.speedMs = prm.tuneSpeed->load();
        s.amount = prm.tuneAmount->load();
        s.harmonyOn = harmOn;
        s.voice[0] = { (int) prm.h1Int->load(), prm.h1Level->load(), prm.h1Pan->load() };
        s.voice[1] = { (int) prm.h2Int->load(), prm.h2Level->load(), prm.h2Pan->load() };
        s.formantSemis = prm.formant->load();
        s.humanize = prm.humanize->load();
        pitch.setSettings (s);

        float* monoOut[1] { mono.data() };
        vs::channels::map (vs::channels::Map::mono, d, 2, n, monoOut);

        pitch.process (mono.data(), d[0], h[0], h[1], n);
        work.copyFrom (1, 0, work, 0, 0, n);
    }
    else
    {
        pitch.detectedMidi.store (-1.0f);
    }

    // 3. Ride: even out the level before anything level-dependent. Harmony
    // voices follow the lead's gain moves.
    if (rideOn)
    {
        rider.setParams (prm.rideTarget->load(), prm.rideRange->load(), prm.rideSpeed->load());
        rider.process (d, 2, n, harmOn ? h : nullptr, harmOn ? 2 : 0);
    }
    else
    {
        rider.meterDb.store (0.0f);
    }

    if (active && harmOn)
    {
        work.addFrom (0, 0, harm, 0, 0, n);
        work.addFrom (1, 0, harm, 1, 0, n);
    }

    // 4. Tone: drive (always runs so latency stays constant) + EQ
    saturator.setDrive (toneOn ? prm.drive->load() : 0.0f);
    {
        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
        saturator.process (block);
    }

    if (toneOn)
        eq.processTone (d, 2, n);

    // 5. De-ess after EQ, since presence/air boosts make esses worse
    if (cleanOn && prm.deess->load() > 0.001f)
    {
        deEsser.setParams (prm.deess->load(), prm.deessFreq->load());
        deEsser.process (d, 2, n);
    }
    else
    {
        deEsser.meterDb.store (0.0f);
    }

    // 6. Dynamics
    if (dynOn)
    {
        comp.setParams (prm.comp->load(), (int) prm.compStyle->load());
        comp.process (d, 2, n);
    }
    else
    {
        comp.meterDb.store (0.0f);
    }

    // 7. Space (keeps running when off so echoes and reverb ring out naturally)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        const float seconds = vs::Space::delaySeconds ((int) prm.delayTime->load(), bpm);

        space.setParams (prm.verb->load(), prm.verbSize->load(), prm.delay->load(), seconds, prm.delayFb->load());
        space.process (d, n, spaceOn);
    }

    // 8. Output
    outGain.setTarget (vs::dbToGain (prm.output->load()));
    vs::mix::applyGain (outGain, d, 2, n);

    // A mono output gets the average of the two sides
    vs::channels::map (outCh == 1 ? vs::channels::Map::mono : vs::channels::Map::keep,
                       work.getArrayOfReadPointers(), 2, n, buffer.getArrayOfWritePointers());
    for (int c = juce::jmax (outCh, 2); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    outputDb.store (vs::ampToDb (vs::channels::peakOf (buffer.getArrayOfReadPointers(), juce::jmin (outCh, buffer.getNumChannels()), n)));
}

int VocalSuiteProcessor::getNumPrograms() { return (int) presets.size(); }

const juce::String VocalSuiteProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void VocalSuiteProcessor::setCurrentProgram (int index)
{
    // Hosts often re-send the current program while loading a project; ignoring
    // that keeps them from wiping the saved settings.
    if (index != currentProgram)
        applyPreset (index);
}

void VocalSuiteProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    vs::applyFactoryPreset (*this, presetManager, presets[(size_t) index]);
}

juce::StringArray VocalSuiteProcessor::getFactoryPresetNames()
{
    juce::StringArray names;
    for (auto& p : presets) names.add (p.name);
    return names;
}

void VocalSuiteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocalSuiteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            vs::PresetManager::restore (apvts, state);
        }
    }
}

juce::AudioProcessorEditor* VocalSuiteProcessor::createEditor()
{
    return new VocalSuiteEditor (*this);
}

#if ! BASEMENT_BUILTIN // Basement creates the processor itself
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocalSuiteProcessor();
}
#endif

