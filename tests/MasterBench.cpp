// MasterBench: how much CPU ENCORE. Master uses on a fixed test file. Used to
// decide whether link-time optimisation is worth it (see README, "Build speed").
//
//   MasterBench --make <file.wav>     write the fixed test file (60 s, 48 kHz stereo,
//                                     the same bytes every time)
//   MasterBench <file.wav> [runs]     process it with Master's default settings in
//                                     512-sample blocks, print the CPU time of each run
//                                     and the best one
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <ctime>
#include <algorithm>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    // A mix that keeps every stage busy: a chord pad, a bass line, a kick and
    // hats with a little noise, all from a fixed seed
    int makeFile (const juce::File& f)
    {
        const int n = (int) (sampleRate * 60.0);
        juce::AudioBuffer<float> b (2, n);
        juce::Random rnd (20261106);
        const double chord[] = { 220.0, 277.18, 329.63, 440.0 };
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sampleRate;
            const double beat = std::fmod (t, 0.5);
            double pad = 0;
            for (double fr : chord) pad += std::sin (juce::MathConstants<double>::twoPi * fr * t);
            const double bass = std::sin (juce::MathConstants<double>::twoPi * (55.0 * (1 + (int) (t / 2.0) % 2)) * t);
            const double kick = std::exp (-beat * 30.0) * std::sin (juce::MathConstants<double>::twoPi * (50.0 + 80.0 * std::exp (-beat * 40.0)) * beat);
            const double hat = std::fmod (t + 0.25, 0.5) < 0.03 ? (rnd.nextFloat() * 2.0f - 1.0f) * 0.4 : 0.0;
            const double noise = (rnd.nextFloat() * 2.0f - 1.0f) * 0.01;
            const double l = 0.08 * pad + 0.25 * bass + 0.5 * kick + hat + noise;
            const double r = 0.09 * pad + 0.25 * bass + 0.5 * kick + 0.8 * hat - noise;
            b.setSample (0, i, (float) l);
            b.setSample (1, i, (float) r);
        }
        f.deleteFile();
        juce::WavAudioFormat wav;
        auto out = f.createOutputStream();
        if (out == nullptr) { std::printf ("can't write %s\n", f.getFullPathName().toRawUTF8()); return 1; }
        juce::OutputStream* raw = out.release();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (raw, sampleRate, 2, 24, {}, 0));
        if (w == nullptr) { delete raw; return 1; }
        w->writeFromAudioSampleBuffer (b, 0, n);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: %s --make <file.wav> | <file.wav> [runs]\n", argv[0]); return 2; }
    juce::ScopedJuceInitialiser_GUI gui;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    if (juce::String (argv[1]) == "--make" && argc > 2)
        return makeFile (cwd.getChildFile (juce::String (argv[2])));

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (cwd.getChildFile (juce::String (argv[1]))));
    if (reader == nullptr) { std::printf ("can't read %s\n", argv[1]); return 1; }
    const int n = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> input (2, n);
    reader->read (&input, 0, n, 0, true, true);
    const int runs = argc > 2 ? std::max (1, std::atoi (argv[2])) : 5;

    double best = 1e9;
    float check = 0;
    for (int run = 0; run < runs; ++run)
    {
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        p->setPlayConfigDetails (2, 2, sampleRate, blockSize);
        p->prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;
        const std::clock_t start = std::clock();
        for (int pos = 0; pos + blockSize <= n; pos += blockSize)
        {
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, input, ch, pos, blockSize);
            p->processBlock (block, midi);
            check += block.getSample (0, blockSize - 1);
        }
        const double secs = double (std::clock() - start) / CLOCKS_PER_SEC;
        best = std::min (best, secs);
        std::printf ("run %d: %.3f s CPU\n", run + 1, secs);
        p->releaseResources();
    }
    std::printf ("best: %.3f s CPU for %.0f s of audio (%.2f%% of one core), check %.4f\n",
                 best, n / sampleRate, 100.0 * best / (n / sampleRate), (double) check);
    return 0;
}
