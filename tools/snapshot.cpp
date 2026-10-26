#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    Dr16AudioProcessor p;
    p.prepareToPlay (48000, 512);
    if (argc > 2) p.loadProgramDefaults (juce::String (argv[2]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File f { juce::String (argv[1]) };
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}
