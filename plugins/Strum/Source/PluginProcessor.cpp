#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    juce::String dB (float v)  { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

    const juce::StringArray curveNames { "Generic", "Learned" };

    // The factory presets: encore-audio-processing's list for this chain (dsp/Presets.h)
    const std::vector<vs::presets::Preset>& presets = vs::presets::forChain ("strum");

    const char* learnedKey = "learnedCurve";
}

StrumProcessor::StrumProcessor()
    : EncoreProcessorBase (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "Strum", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    prm = { get ("body_on"), get ("body_curve"), get ("body_amount"), get ("boom_on"), get ("boom"),
            get ("squeak_on"), get ("squeak"), get ("pick_on"), get ("pick"), get ("double_on"), get ("width"),
            get ("output"), get ("compare") };

    apvts.state.addListener (this);
    startTimerHz (20);
}

StrumProcessor::~StrumProcessor()
{
    stopTimer();
    apvts.state.removeListener (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout StrumProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto toggle = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto num = [&] (const char* id, const char* name, NormalisableRange<float> range, float def, std::function<String (float)> text)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           Attr().withStringFromValueFunction ([text] (float v, int) { return text (v); })));
    };

    toggle ("body_on", "Body On", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "body_curve", 1 }, "Body Curve", curveNames, 0));
    num ("body_amount", "Body Amount", { 0.0f, 1.0f }, 1.0f, pct);

    toggle ("boom_on", "Boom On", true);
    num ("boom", "Boom", { 0.0f, 1.0f }, 0.4f, pct);
    toggle ("squeak_on", "Squeak On", true);
    num ("squeak", "Squeak", { 0.0f, 1.0f }, 0.5f, pct);
    toggle ("pick_on", "Pick On", true);
    num ("pick", "Pick", { 0.0f, 1.0f }, 0.3f, pct);
    toggle ("double_on", "Double On", false);
    num ("width", "Width", { 0.0f, 1.0f }, 0.6f, pct);

    num ("output", "Output", { -18.0f, 18.0f, 0.1f }, 0.0f, dB);
    toggle ("compare", "Hear Original", false);
    return layout;
}

bool StrumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const auto monoSet = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    if (! ((in == monoSet && out == monoSet) || (in == stereo && out == stereo) || (in == monoSet && out == stereo)))
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != monoSet && sc != stereo)
            return false;
    }
    return true;
}

void StrumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    work.setSize (2, samplesPerBlock);
    wet.setSize (2, samplesPerBlock);
    diMono.assign ((size_t) samplesPerBlock, 0.0f);
    micMono.assign ((size_t) samplesPerBlock, 0.0f);

    learner.prepare (sampleRate);
    boom.prepare (sampleRate);
    squeak.prepare (sampleRate);
    pick.prepare (sampleRate);
    doubler.prepare (sampleRate);

    {
        const juce::ScopedLock sl (designLock);
        redesignBody (true);
    }
    body.prepare (spec); // picks up the filter just designed
    bodyWasIdle = true;

    bodyMix.reset (sampleRate, 0.03);
    bodyMix.setCurrentAndTarget (prm.bodyOn->load() > 0.5f ? 1.0f : 0.0f);
    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTarget (vs::dbToGain (prm.output->load()));
    compare.setHearOriginal (prm.compare->load() > 0.5f);
    compare.prepare (sampleRate, samplesPerBlock, 0);

    // Nothing looks ahead and the Body filter is minimum phase, so there's no latency to report
    setLatencySamples (0);
}

void StrumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int inCh = getMainBusNumInputChannels();
    const int outCh = getMainBusNumOutputChannels();
    if (n == 0 || inCh == 0) return;

    if (work.getNumSamples() < n) // hosts shouldn't exceed the prepared size, but be safe
    {
        for (auto* b : { &work, &wet }) b->setSize (2, n, false, false, true);
        diMono.resize ((size_t) n);
        micMono.resize ((size_t) n);
    }

    // Side-chain (the mic, for Learn), read straight from the host buffer
    const float* sc[2] {};
    int scCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        const int count = std::min (2, getBus (true, 1)->getNumberOfChannels());
        for (int c = 0; c < count; ++c)
        {
            const int idx = getChannelIndexInProcessBlockBuffer (true, 1, c);
            if (idx < buffer.getNumChannels()) sc[scCh++] = buffer.getReadPointer (idx);
        }
    }
    sidechainLive = scCh > 0;

    // Everything runs in stereo internally; a mono input is duplicated
    work.copyFrom (0, 0, buffer, 0, 0, n);
    work.copyFrom (1, 0, buffer, inCh > 1 ? 1 : 0, 0, n);
    inputDb.store (vs::ampToDb (vs::channels::peakOf (work.getArrayOfReadPointers(), 2, n)));

    float* const* d = work.getArrayOfWritePointers();
    compare.takeOriginal (d[0], d[1], n);

    // Learn listens to the untouched DI and the mic
    float* diOut[1] { diMono.data() };
    vs::channels::map (vs::channels::Map::mono, d, 2, n, diOut);
    float scPeak = 0.0f;
    if (scCh > 0)
    {
        float* micOut[1] { micMono.data() };
        vs::channels::map (vs::channels::Map::mono, sc, scCh, n, micOut);
        scPeak = vs::channels::peakOf (micOut, 1, n);
    }
    sidechainDb.store (scCh > 0 ? vs::ampToDb (scPeak) : -100.0f);
    learner.process (diMono.data(), scCh > 0 ? micMono.data() : nullptr, n);

    // 1. Body (crossfades in and out; the filter only runs while it's heard)
    bodyMix.setTarget (prm.bodyOn->load() > 0.5f ? 1.0f : 0.0f);
    if (bodyMix.isRamping() || bodyMix.getTarget() > 0.5f)
    {
        if (bodyWasIdle) { body.reset(); bodyWasIdle = false; }
        wet.copyFrom (0, 0, work, 0, 0, n);
        wet.copyFrom (1, 0, work, 1, 0, n);
        body.process (wet.getArrayOfWritePointers(), n);

        if (! bodyMix.isRamping())
        {
            work.copyFrom (0, 0, wet, 0, 0, n);
            work.copyFrom (1, 0, wet, 1, 0, n);
        }
        else
        {
            vs::mix::crossfadeTo (bodyMix, d, wet.getArrayOfReadPointers(), 2, n);
        }
    }
    else
    {
        bodyWasIdle = true;
    }

    // 2-4. Dynamic fixes: lows first, then squeaks, then attacks
    boom.process (d, 2, n, prm.boom->load(), prm.boomOn->load() > 0.5f);
    squeak.process (d, 2, n, prm.squeak->load(), prm.squeakOn->load() > 0.5f);
    pick.process (d, 2, n, prm.pick->load(), prm.pickOn->load() > 0.5f);

    // 5. Double (last, so the copies are already cleaned up)
    doubler.process (d[0], d[1], n, prm.doubleOn->load() > 0.5f ? prm.width->load() : 0.0f);

    // 6. Output
    outGain.setTarget (vs::dbToGain (prm.output->load()));
    vs::mix::applyGain (outGain, d, 2, n);

    // "Hear Original" plays the untouched input at the processed sound's
    // loudness, so the A/B is about tone, not volume.
    compare.setHearOriginal (prm.compare->load() > 0.5f);
    compare.mix (d[0], d[1], n);

    // A mono output gets the average of the two sides
    vs::channels::map (outCh == 1 ? vs::channels::Map::mono : vs::channels::Map::keep,
                       work.getArrayOfReadPointers(), 2, n, buffer.getArrayOfWritePointers());
    for (int c = outCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    outputDb.store (vs::ampToDb (vs::channels::peakOf (d, 2, n)));
}

//==============================================================================
bool StrumProcessor::isUsingLearned() const
{
    return (int) prm.bodyCurve->load() == 1 && learned.valid;
}

vs::BodyCurve StrumProcessor::getActiveCurve() const
{
    static const vs::BodyCurve generic = vs::BodyCurve::generic();
    return isUsingLearned() ? learned : generic;
}

void StrumProcessor::redesignBody (bool force)
{
    const auto curve = getActiveCurve();
    const float amount = prm.bodyAmount->load();
    if (! force && curve == designed.curve && std::abs (amount - designed.amount) < 0.004f && designed.rate == fs)
        return;
    body.setCurve (curve, amount, fs);
    designed = { curve, amount, fs };
}

void StrumProcessor::service()
{
    const juce::ScopedLock sl (designLock);

    vs::BodyCurve fresh;
    if (learner.takeResult (fresh))
    {
        learned = fresh;
        writeLearnedToState();
        // a fresh Learn is what you want to hear, so switch Body to it
        if (auto* p = apvts.getParameter ("body_curve"))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (1.0f));
            p->endChangeGesture();
        }
    }
    redesignBody (false);
}

void StrumProcessor::toggleLearn()
{
    if (learner.isLearning()) learner.requestStop();
    else                      learner.requestStart();
}

void StrumProcessor::forgetLearned()
{
    {
        const juce::ScopedLock sl (designLock);
        learned = {};
        writeLearnedToState();
    }
    if (auto* p = apvts.getParameter ("body_curve"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (0.0f);
        p->endChangeGesture();
    }
    service();
}

// The learned curve lives in the state tree, so it's saved with the project
// and with any user preset ("My guitar, mic") saved while it's there.
void StrumProcessor::writeLearnedToState()
{
    if (learned.valid) apvts.state.setProperty (learnedKey, juce::String (learned.toString()), nullptr);
    else               apvts.state.removeProperty (learnedKey, nullptr);
}

// A user preset was loaded (it replaces the whole state tree). If it carries
// a learned curve, that becomes the curve; if not, the curve already learned
// in this session is kept, so trying presets never throws away a Learn.
void StrumProcessor::valueTreeRedirected (juce::ValueTree&)
{
    if (restoringProject) return;
    const juce::ScopedLock sl (designLock);
    const auto text = apvts.state.getProperty (learnedKey).toString();
    if (text.isNotEmpty())
    {
        const auto c = vs::BodyCurve::fromString (text.toStdString());
        if (c.valid) learned = c;
    }
    else
    {
        writeLearnedToState();
    }
    redesignBody (false);
}

//==============================================================================
int StrumProcessor::getNumPrograms() { return (int) presets.size(); }
const juce::String StrumProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) presets.size()) ? presets[(size_t) i].name : ""; }
juce::StringArray StrumProcessor::getFactoryPresetNames() const { return vs::factoryNames (presets); }
void StrumProcessor::setCurrentProgram (int index) { if (index != currentProgram) applyPreset (index); }

void StrumProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    vs::applyFactoryPreset (*this, presetManager, presets[(size_t) index]);
}

void StrumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void StrumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = state.getProperty ("program", 0);
    restoringProject = true;
    vs::PresetManager::restore (apvts, state);
    restoringProject = false;

    // The project's learned curve comes back with it (or none, if it had none)
    const juce::ScopedLock sl (designLock);
    learned = vs::BodyCurve::fromString (apvts.state.getProperty (learnedKey).toString().toStdString());
    redesignBody (true);
}

juce::AudioProcessorEditor* StrumProcessor::createEditor() { return new StrumEditor (*this); }
#if ! BASEMENT_BUILTIN // Basement creates the processor itself
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new StrumProcessor(); }
#endif
