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

    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, double>> values; // double so integer literals convert cleanly
    };

    // Starting points, not finished mixes. Anything not listed falls back to
    // the parameter's default. Choice values are list indexes.
    const std::vector<Preset> presets
    {
        { "Clean Start", {} },
        { "Folk Intimate", {
            { "ride_target", -20 }, { "ride_range", 5 }, { "ride_speed", 0.35f },
            { "hpf", 70 }, { "deess", 0.35f }, { "deess_freq", 6500 },
            { "drive", 0.1f }, { "low", 1 }, { "body", -1 }, { "presence", 1.5f }, { "air", 2.5f },
            { "comp", 0.3f }, { "comp_style", 0 },
            { "verb", 0.22f }, { "verb_size", 0.55f }, { "delay", 0 } } },
        { "Punk Shout", {
            { "ride_target", -16 }, { "ride_range", 8 }, { "ride_speed", 0.7f },
            { "hpf", 110 }, { "gate", -55 }, { "deess", 0.45f }, { "deess_freq", 5500 },
            { "drive", 0.45f }, { "low", -1 }, { "body", -2 }, { "presence", 3 }, { "air", 1 },
            { "comp", 0.6f }, { "comp_style", 1 },
            { "verb", 0.08f }, { "verb_size", 0.3f }, { "delay", 0.15f }, { "delay_time", 0 }, { "delay_fb", 0.1f } } },
        { "Emo Lead", {
            { "ride_target", -17 }, { "ride_range", 7 }, { "ride_speed", 0.55f },
            { "hpf", 90 }, { "gate", -65 }, { "deess", 0.45f }, { "deess_freq", 6500 },
            { "drive", 0.25f }, { "body", -2 }, { "presence", 2 }, { "air", 3 },
            { "comp", 0.5f }, { "comp_style", 1 },
            { "verb", 0.22f }, { "verb_size", 0.6f }, { "delay", 0.18f }, { "delay_time", 3 }, { "delay_fb", 0.3f } } },
        { "Scream Control", {
            { "ride_target", -16 }, { "ride_range", 10 }, { "ride_speed", 0.85f },
            { "hpf", 120 }, { "gate", -50 }, { "deess", 0.3f }, { "deess_freq", 5000 },
            { "drive", 0.55f }, { "low", -2 }, { "body", -3 }, { "presence", 1.5f },
            { "comp", 0.75f }, { "comp_style", 2 },
            { "verb", 0.1f }, { "verb_size", 0.35f }, { "delay", 0.1f }, { "delay_time", 0 }, { "delay_fb", 0.05f } } },
        { "Backing Vocals", {
            { "ride_target", -22 }, { "ride_range", 6 }, { "ride_speed", 0.4f },
            { "hpf", 150 }, { "gate", -60 }, { "deess", 0.6f }, { "deess_freq", 7000 },
            { "drive", 0.1f }, { "low", -3 }, { "body", -2 }, { "presence", -1 }, { "air", 2 },
            { "comp", 0.5f }, { "comp_style", 0 },
            { "verb", 0.3f }, { "verb_size", 0.6f }, { "delay", 0 } } },
        // Pitch presets: set Key and Scale to the song for Tune/Harmony to land right
        { "Folk Harmony", {
            { "ride_target", -20 }, { "ride_range", 5 }, { "ride_speed", 0.35f },
            { "hpf", 70 }, { "deess", 0.35f }, { "deess_freq", 6500 },
            { "drive", 0.1f }, { "low", 1 }, { "body", -1 }, { "presence", 1.5f }, { "air", 2.5f },
            { "comp", 0.3f }, { "comp_style", 0 },
            { "verb", 0.25f }, { "verb_size", 0.55f }, { "delay", 0 },
            { "scale", 1 }, { "tune_on", 1 }, { "tune_speed", 120 }, { "tune_amount", 0.5f },
            { "harm_on", 1 }, { "h1_int", 7 }, { "h1_level", -9 }, { "h1_pan", -0.35f }, { "h2_int", 0 }, { "humanize", 0.6f } } },
        { "Punk Gang Double", {
            { "ride_target", -16 }, { "ride_range", 8 }, { "ride_speed", 0.7f },
            { "hpf", 110 }, { "gate", -55 }, { "deess", 0.45f }, { "deess_freq", 5500 },
            { "drive", 0.45f }, { "low", -1 }, { "body", -2 }, { "presence", 3 }, { "air", 1 },
            { "comp", 0.6f }, { "comp_style", 1 },
            { "verb", 0.1f }, { "verb_size", 0.35f }, { "delay", 0 },
            { "harm_on", 1 }, { "h1_int", 1 }, { "h1_level", -5 }, { "h1_pan", -0.8f },
            { "h2_int", 1 }, { "h2_level", -5 }, { "h2_pan", 0.8f }, { "humanize", 0.8f } } },
        { "Hard Tune", {
            { "tune_on", 1 }, { "tune_speed", 0 }, { "tune_amount", 1 },
            { "drive", 0.2f }, { "presence", 2 }, { "air", 2 },
            { "comp", 0.5f }, { "comp_style", 1 },
            { "verb", 0.15f }, { "delay", 0.15f }, { "delay_time", 2 }, { "delay_fb", 0.25f } } },
    };
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

    lastHpf = -1; lastLow = lastBody = lastPres = lastAir = -100;
    for (int c = 0; c < 2; ++c)
    {
        for (auto* f : { &hpf1[c], &hpf2[c], &lowEq[c], &bodyEq[c], &presEq[c], &airEq[c] })
        {
            f->coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 1000.0, 1.0, 1.0f);
            f->reset();
        }
    }
    updateFilters();

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (vs::dbToGain (prm.output->load()));

    pitchActive = prm.tuneOn->load() > 0.5f || prm.harmOn->load() > 0.5f;
    setLatencySamples (saturator.getLatencySamples() + (pitchActive ? pitch.getLatency() : 0));
}

void VocalSuiteProcessor::updateFilters()
{
    using AC = juce::dsp::IIR::ArrayCoefficients<float>;

    const float hpf = prm.hpf->load();
    if (hpf != lastHpf)
    {
        lastHpf = hpf;
        // 4th-order Butterworth = two biquads with these Qs
        const auto a = AC::makeHighPass (fs, hpf, 0.5412f);
        const auto b = AC::makeHighPass (fs, hpf, 1.3066f);
        for (int c = 0; c < 2; ++c) { *hpf1[c].coefficients = a; *hpf2[c].coefficients = b; }
    }

    auto band = [this] (float value, float& last, Filter* filters, auto make)
    {
        if (value == last) return;
        last = value;
        const auto coefs = make (vs::dbToGain (value));
        for (int c = 0; c < 2; ++c) *filters[c].coefficients = coefs;
    };

    band (prm.low->load(), lastLow, lowEq,        [this] (float g) { return AC::makeLowShelf (fs, 110.0f, 0.7f, g); });
    band (prm.body->load(), lastBody, bodyEq,     [this] (float g) { return AC::makePeakFilter (fs, 350.0f, 0.9f, g); });
    band (prm.presence->load(), lastPres, presEq, [this] (float g) { return AC::makePeakFilter (fs, 3500.0f, 0.8f, g); });
    band (prm.air->load(), lastAir, airEq,        [this] (float g) { return AC::makeHighShelf (fs, juce::jmin (11000.0f, (float) fs * 0.42f), 0.7f, g); });
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
    inputDb.store (vs::ampToDb (buffer.getMagnitude (0, n)));

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
        if (lastHpf > 21.0f)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    d[c][i] = hpf2[c].processSample (hpf1[c].processSample (d[c][i]));

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

        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = 0.5f * (d[0][i] + d[1][i]);

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
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                d[c][i] = airEq[c].processSample (presEq[c].processSample (bodyEq[c].processSample (lowEq[c].processSample (d[c][i]))));

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
                    bpm = juce::jlimit (30.0, 300.0, *b);

        static constexpr float beats[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f };
        const int t = juce::jlimit (0, 5, (int) prm.delayTime->load());
        const float seconds = t == 0 ? 0.095f : (float) (60.0 / bpm) * beats[t];

        space.setParams (prm.verb->load(), prm.verbSize->load(), prm.delay->load(), seconds, prm.delayFb->load());
        space.process (d, n, spaceOn);
    }

    // 8. Output
    outGain.setTargetValue (vs::dbToGain (prm.output->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        d[0][i] *= g;
        d[1][i] *= g;
    }

    if (outCh == 1)
    {
        buffer.copyFrom (0, 0, work.getReadPointer (0), n, 0.5f);
        buffer.addFrom (0, 0, work.getReadPointer (1), n, 0.5f);
    }
    else
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.copyFrom (1, 0, work, 1, 0, n);
    }
    for (int c = juce::jmax (outCh, 2); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    outputDb.store (vs::ampToDb (buffer.getMagnitude (0, n)));
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

    for (auto* param : getParameters())
    {
        auto* withId = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (withId == nullptr) continue;

        float value = withId->convertFrom0to1 (withId->getDefaultValue());
        for (auto& [id, v] : presets[(size_t) index].values)
            if (withId->paramID == id)
                value = (float) v;

        withId->setValueNotifyingHost (withId->convertTo0to1 (value));
    }

    // Hosts may call this from the audio thread; the name lives in the state
    // tree, which is only safe to touch from the message thread.
    if (juce::MessageManager::existsAndIsCurrentThread())
        presetManager.setCurrent (presets[(size_t) index].name, false);
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

