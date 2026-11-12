#!/usr/bin/env bash
# Fails when the plugins or shared/ carry audio processing of their own.
# All of ENCORE.'s audio processing lives in encore-audio-processing
# (external/encore-audio-processing/dsp): filters, smoothing, oversampling,
# FFTs, channel maps, mixing, levels, meters, the axes an editor draws on and
# the log ranges of frequency knobs are called from there. JUCE's own
# processing classes and buffer mixing count too, since using them would be a
# second version of something that repository has. Copying audio unchanged
# and host plumbing are not processing and are not matched.
set -euo pipefail
cd "$(dirname "$0")/.."

pattern='juce::dsp::(IIR|FIR|FFT|Oversampling|Convolution|StateVariable[A-Za-z]*|LinkwitzRiley[A-Za-z]*|Compressor|Limiter|Reverb|Gain|Panner|WindowingFunction|DelayLine|Chorus|Phaser|LadderFilter|ProcessorDuplicator|BallisticsFilter|NoiseGate|WaveShaper|Bias|DryWetMixer)\b'
pattern+='|juce::(SmoothedValue|LinearSmoothedValue|Reverb|IIRFilter|IIRCoefficients|LagrangeInterpolator|CatmullRomInterpolator|WindowedSincInterpolator|Decibels)\b'
pattern+='|\.(getMagnitude|getRMSLevel|applyGainRamp)\s*\('
# hand-written gain and mix loops (a channel's samples scaled or blended one by one)
pattern+='|\]\[i\]\s*\*=|\]\[i\]\s*\+=\s*[a-z]+\s*\*\s*\('
# buffer mixing, a channel's samples summed by hand, channel duplication and a resampler
pattern+='|\.(addFrom|addFromWithRamp|copyFromWithRamp)\s*\(|\+=\s*[a-z]+\.getSample\s*\(|copyFrom\s*\(\s*1\s*,\s*0\s*,\s*[a-z]+\s*,\s*0|ResamplingAudioSource|AudioThumbnail'
# a log frequency axis or knob range written out (20 Hz times a power, or a log ratio)
pattern+='|std::log\s*\(\s*[a-z]+\s*/\s*(20|[a-z]+)\.?0?f?\s*\)\s*/\s*std::log|\*\s*std::pow\s*\(\s*(1000|[a-z]+\s*/\s*[a-z]+)'

if hits=$(grep -rnE "$pattern" plugins shared --include='*.cpp' --include='*.h'); then
    echo "$hits"
    echo "::error::Audio processing in the plugins. Call encore-audio-processing's headers instead (and when it lacks something, add it there first)."
    exit 1
fi
echo "No audio processing outside encore-audio-processing."
