#!/usr/bin/env bash
# Fails when the plugins or shared/ carry audio processing of their own.
# All of ENCORE.'s audio processing lives in encore-audio-processing
# (external/encore-audio-processing/dsp): filters, smoothing, oversampling,
# FFTs, channel maps, levels and meters are called from there. JUCE's own
# processing classes count too, since using them would be a second version
# of something that repository has. Drawing, parameter ranges and host
# plumbing are not processing and are not matched.
set -euo pipefail
cd "$(dirname "$0")/.."

pattern='juce::dsp::(IIR|FIR|FFT|Oversampling|Convolution|StateVariable[A-Za-z]*|LinkwitzRiley[A-Za-z]*|Compressor|Limiter|Reverb|Gain|Panner|WindowingFunction|DelayLine|Chorus|Phaser|LadderFilter|ProcessorDuplicator|BallisticsFilter|NoiseGate|WaveShaper|Bias|DryWetMixer)\b'
pattern+='|juce::(SmoothedValue|LinearSmoothedValue|Reverb|IIRFilter|IIRCoefficients|LagrangeInterpolator|CatmullRomInterpolator|WindowedSincInterpolator|Decibels)\b'
pattern+='|\.(getMagnitude|getRMSLevel|applyGainRamp)\s*\('

if hits=$(grep -rnE "$pattern" plugins shared --include='*.cpp' --include='*.h'); then
    echo "$hits"
    echo "::error::Audio processing in the plugins. Call encore-audio-processing's headers instead (and when it lacks something, add it there first)."
    exit 1
fi
echo "No audio processing outside encore-audio-processing."
