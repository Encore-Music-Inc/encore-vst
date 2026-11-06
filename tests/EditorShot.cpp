// EditorShot <file.png>: draws one plugin's editor offscreen to a PNG, to check the
// look without a host or an audio device. Built once per plugin (Shot<Name>).
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: %s <file.png>\n", argv[0]); return 2; }
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    p->setPlayConfigDetails (2, 2, 48000.0, 512);
    p->prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());   // createEditor() is private from JUCE 9
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    const juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1])));
    f.deleteFile();
    juce::FileOutputStream out (f);
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::printf ("wrote %s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    p->releaseResources();
    return 0;
}
