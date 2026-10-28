#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    juce::String hz (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String dB (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    const juce::StringArray typeNames { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
    const juce::StringArray slopeNames { "12 dB/oct", "24 dB/oct", "48 dB/oct" };

    struct BandDefault { bool on; int type; float freq, q; };
    const BandDefault bandDefaults[EqProcessor::numBands] {
        { false, 3, 30.0f, 0.71f }, { true, 1, 100.0f, 0.7f }, { true, 0, 250.0f, 1.0f }, { true, 0, 800.0f, 1.0f },
        { true, 0, 2500.0f, 1.0f }, { true, 0, 6000.0f, 1.0f }, { true, 2, 10000.0f, 0.7f }, { false, 4, 18000.0f, 0.71f },
    };
    const float smartDefaults[EqProcessor::numSmart] { 300.0f, 1200.0f, 3000.0f, 7000.0f };

    // Starting points by source; the gain moves are deliberately modest.
    const std::vector<vs::FactoryPreset> presets
    {
        { "Flat", {} },
        { "Vocal Clean-Up", { { "b1_on", 1 }, { "b1_freq", 90 }, { "b2_gain", -1.5 }, { "b3_gain", -2 }, { "b3_freq", 300 },
                              { "b5_gain", 1.5 }, { "b5_freq", 3200 }, { "b7_gain", 2 }, { "b7_freq", 11000 } } },
        { "Acoustic Guitar", { { "b1_on", 1 }, { "b1_freq", 70 }, { "b2_gain", -2 }, { "b2_freq", 150 }, { "b3_gain", -2.5 }, { "b3_freq", 220 }, { "b3_q", 1.4 },
                               { "b6_gain", 1.5 }, { "b6_freq", 5000 }, { "b7_gain", 2.5 }, { "b7_freq", 12000 } } },
        { "Punk Guitar Wall", { { "b1_on", 1 }, { "b1_freq", 90 }, { "b3_gain", -3 }, { "b3_freq", 400 }, { "b3_q", 1.2 },
                                { "b5_gain", 1.5 }, { "b5_freq", 1800 }, { "b8_on", 1 }, { "b8_freq", 11000 } } },
        { "Kick Punch", { { "b1_on", 1 }, { "b1_freq", 35 }, { "b2_freq", 60 }, { "b2_gain", 2.5 }, { "b3_gain", -4 }, { "b3_freq", 350 }, { "b3_q", 1.5 },
                          { "b5_gain", 3 }, { "b5_freq", 3500 }, { "b5_q", 1.5 } } },
        { "Snare Crack", { { "b1_on", 1 }, { "b1_freq", 80 }, { "b3_freq", 200 }, { "b3_gain", 2 }, { "b4_freq", 900 }, { "b4_gain", -2.5 },
                           { "b6_freq", 5500 }, { "b6_gain", 2.5 } } },
        { "Bass Tight", { { "b1_on", 1 }, { "b1_freq", 35 }, { "b2_freq", 80 }, { "b2_gain", 1.5 }, { "b3_freq", 250 }, { "b3_gain", -2.5 },
                          { "b5_freq", 1500 }, { "b5_gain", 2 }, { "b8_on", 1 }, { "b8_freq", 9000 } } },
        { "Guitars Carved by Vocal", { { "b1_on", 1 }, { "b1_freq", 90 }, { "carve_on", 1 }, { "carve_depth", 5 }, { "carve_low", 250 }, { "carve_high", 6000 } } },
        { "Harsh Scream Tamer", { { "s1_on", 1 }, { "s1_freq", 2800 }, { "s1_range", 6 }, { "s1_thresh", -14 }, { "s2_on", 1 }, { "s2_freq", 4500 },
                                  { "s2_range", 6 }, { "s2_thresh", -16 }, { "b1_on", 1 }, { "b1_freq", 100 } } },
    };
}

EqProcessor::EqProcessor()
    : EncoreProcessorBase (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "EQ", createLayout())
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    for (int i = 0; i < numBands; ++i)
    {
        const auto b = "b" + juce::String (i + 1) + "_";
        bandP[i] = { get (b + "on"), get (b + "type"), get (b + "freq"), get (b + "gain"), get (b + "q"), get (b + "slope") };
    }
    for (int i = 0; i < numSmart; ++i)
    {
        const auto s = "s" + juce::String (i + 1) + "_";
        smartP[i] = { get (s + "on"), get (s + "freq"), get (s + "q"), get (s + "range"), get (s + "thresh") };
    }
    carveOn = get ("carve_on"); carveDepth = get ("carve_depth"); carveLow = get ("carve_low");
    carveHigh = get ("carve_high"); carveSpeed = get ("carve_speed"); outputP = get ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout EqProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto num = [&] (const String& id, const String& name, NormalisableRange<float> range, float def, std::function<String (float)> text)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           Attr().withStringFromValueFunction ([text] (float v, int) { return text (v); })));
    };
    auto logRange = [] (float lo, float hi)
    {
        return NormalisableRange<float> (lo, hi,
            [] (float a, float b, float t) { return a * std::pow (b / a, t); },
            [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
    };

    for (int i = 0; i < numBands; ++i)
    {
        const auto& d = bandDefaults[i];
        const auto b = "b" + String (i + 1) + "_";
        const auto n = "Band " + String (i + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { b + "on", 1 }, n + "On", d.on));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { b + "type", 1 }, n + "Type", typeNames, d.type));
        num (b + "freq", n + "Freq", logRange (20.0f, 20000.0f), d.freq, hz);
        num (b + "gain", n + "Gain", { -18.0f, 18.0f, 0.01f }, 0.0f, dB);
        num (b + "q", n + "Q", logRange (0.1f, 18.0f), d.q, [] (float v) { return String (v, 2); });
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { b + "slope", 1 }, n + "Slope", slopeNames, 1));
    }

    for (int i = 0; i < numSmart; ++i)
    {
        const auto s = "s" + String (i + 1) + "_";
        const auto n = "Smart " + String (i + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { s + "on", 1 }, n + "On", false));
        num (s + "freq", n + "Freq", logRange (40.0f, 18000.0f), smartDefaults[i], hz);
        num (s + "q", n + "Q", logRange (0.7f, 16.0f), 4.0f, [] (float v) { return String (v, 1); });
        num (s + "range", n + "Range", { 0.0f, 18.0f, 0.1f }, 6.0f, [] (float v) { return String (-v, 1) + " dB"; });
        num (s + "thresh", n + "Threshold", { -40.0f, 0.0f, 0.1f }, -18.0f, [] (float v) { return String (v, 1) + " dB"; });
    }

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "carve_on", 1 }, "Carve On", false));
    num ("carve_depth", "Depth", { 0.0f, 15.0f, 0.1f }, 6.0f, [] (float v) { return String (-v, 1) + " dB"; });
    num ("carve_low", "Carve Low", logRange (40.0f, 2000.0f), 120.0f, hz);
    num ("carve_high", "Carve High", logRange (1000.0f, 16000.0f), 8000.0f, hz);
    num ("carve_speed", "Speed", { 0.0f, 1.0f }, 0.5f, pct);

    num ("output", "Output", { -18.0f, 18.0f, 0.1f }, 0.0f, dB);
    return layout;
}

bool EqProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out || (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()))
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

vs::EqBand::Settings EqProcessor::readBand (int i) const
{
    const auto& p = bandP[i];
    vs::EqBand::Settings s;
    s.on = p.on->load() > 0.5f;
    s.type = (int) p.type->load();
    s.freq = p.freq->load();
    s.gainDb = p.gain->load();
    s.q = p.q->load();
    s.slope = (int) p.slope->load();
    return s;
}

vs::DynamicBand::Settings EqProcessor::readSmart (int i) const
{
    const auto& p = smartP[i];
    vs::DynamicBand::Settings s;
    s.on = p.on->load() > 0.5f;
    s.freq = p.freq->load();
    s.q = p.q->load();
    s.rangeDb = p.range->load();
    s.thresholdDb = p.thresh->load();
    return s;
}

vs::Carve::Settings EqProcessor::readCarve() const
{
    vs::Carve::Settings s;
    s.on = carveOn->load() > 0.5f;
    s.depthDb = carveDepth->load();
    s.lowHz = carveLow->load();
    s.highHz = carveHigh->load();
    s.speed = carveSpeed->load();
    return s;
}

void EqProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    rate = sampleRate;
    for (int i = 0; i < numBands; ++i) { bands[i].prepare (sampleRate); bands[i].setTarget (readBand (i)); }
    for (int i = 0; i < numSmart; ++i) { smart[i].prepare (sampleRate); smart[i].setSettings (readSmart (i)); }
    carve.prepare (sampleRate);
    carve.setSettings (readCarve());
    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (vs::dbToGain (outputP->load()));
    monoScratch.assign ((size_t) samplesPerBlock, 0.0f);
}

void EqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples(), nch = main.getNumChannels();
    if (n == 0 || nch == 0) return;
    if ((int) monoScratch.size() < n) monoScratch.resize ((size_t) n);

    // Side-chain channels are read in place from the host buffer (no copy, no allocation)
    const float* scPtrs[2] {};
    int scCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        const int count = std::min (2, getBus (true, 1)->getNumberOfChannels());
        for (int c = 0; c < count; ++c)
        {
            const int idx = getChannelIndexInProcessBlockBuffer (true, 1, c);
            if (idx < buffer.getNumChannels()) scPtrs[scCh++] = buffer.getReadPointer (idx);
        }
    }
    const float* const* sc = scCh > 0 ? scPtrs : nullptr;
    sidechainLive = sc != nullptr;

    auto pushTap = [&] (Tap& tap)
    {
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int c = 0; c < nch; ++c) m += main.getSample (c, i);
            monoScratch[(size_t) i] = m / (float) nch;
        }
        tap.push (monoScratch.data(), n);
    };
    pushTap (preTap);

    for (int i = 0; i < numSmart; ++i) smart[i].setSettings (readSmart (i));
    carve.setSettings (readCarve());

    float* const* d = main.getArrayOfWritePointers();
    for (int start = 0; start < n; start += 32)
    {
        const int len = std::min (32, n - start);
        for (int b = 0; b < numBands; ++b)
        {
            bands[b].setTarget (readBand (b));
            bands[b].process (d, nch, start, len);
        }
        for (int s = 0; s < numSmart; ++s)
            smart[s].process (d, nch, start, len);
        carve.process (d, nch, sc, scCh, start, len);
    }

    outGain.setTargetValue (vs::dbToGain (outputP->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        for (int c = 0; c < nch; ++c) d[c][i] *= g;
    }

    for (int s = 0; s < numSmart; ++s) smartCutDb[s].store (smart[s].currentCutDb());
    for (int k = 0; k < vs::Carve::numBands; ++k) carveGainDb[k].store (carve.bandGainDb (k));
    pushTap (postTap);
}

int EqProcessor::getNumPrograms() { return (int) presets.size(); }
const juce::String EqProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) presets.size()) ? presets[(size_t) i].name : ""; }
juce::StringArray EqProcessor::getFactoryPresetNames() const { return vs::factoryNames (presets); }
void EqProcessor::setCurrentProgram (int index) { if (index != currentProgram) applyPreset (index); }

void EqProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    vs::applyFactoryPreset (*this, presetManager, presets[(size_t) index]);
}

void EqProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void EqProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            vs::PresetManager::restore (apvts, state);
        }
}

juce::AudioProcessorEditor* EqProcessor::createEditor() { return new EqEditor (*this); }
#if ! BASEMENT_BUILTIN // Basement creates the processor itself
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EqProcessor(); }

#endif
