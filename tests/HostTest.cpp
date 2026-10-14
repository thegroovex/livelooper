// Host test: loads the compiled livelooper.vst3 through JUCE's VST3 hosting code, the
// same way a DAW does, and checks that the plug-in records and plays a loop, exposes
// all parameters, accepts MIDI and restores its state.
//
// usage: livelooper_hosttest <path to livelooper.vst3>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  [ OK ] " : "  [FAIL] ") << what << std::endl;
    if (! condition)
        ++failures;
}

juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* param : p.getParameters())
        if (param->getName (64) == name)
            return param;
    return nullptr;
}

void processBlocks (juce::AudioPluginInstance& p, int blocks, float input, float& peakOut, juce::MidiBuffer* midi = nullptr)
{
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer empty;
    peakOut = 0.0f;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), input, 512);
        p.processBlock (buffer, midi != nullptr && b == 0 ? *midi : empty);
        peakOut = std::max (peakOut, buffer.getMagnitude (0, 512));
    }
}

void pressParam (juce::AudioPluginInstance& p, juce::AudioProcessorParameter* param)
{
    float peak;
    param->setValueNotifyingHost (0.0f);
    processBlocks (p, 1, 0.0f, peak);
    param->setValueNotifyingHost (1.0f);
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc < 2)
    {
        std::cerr << "usage: livelooper_hosttest <livelooper.vst3>" << std::endl;
        return 2;
    }

    const juce::File pluginFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    std::cout << "Loading " << pluginFile.getFullPathName() << std::endl;

    juce::AudioPluginFormatManager formats;
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* format : formats.getFormats())
        format->findAllTypesForFile (types, pluginFile.getFullPathName());

    check (types.size() == 1, "plug-in found in the bundle");
    if (types.isEmpty())
        return 1;

    const auto& desc = *types[0];
    check (desc.name == "livelooper", "plug-in name is livelooper (" + desc.name + ")");
    check (! desc.isInstrument, "registered as an audio effect");

    juce::String error;
    auto plugin = formats.createPluginInstance (desc, 48000.0, 512, error);
    check (plugin != nullptr, "plug-in instance created " + error);
    if (plugin == nullptr)
        return 1;

    check (plugin->acceptsMidi(), "accepts MIDI input");
    // The VST3 wrapper adds hidden MIDI-CC helper parameters (that is how VST3 delivers CCs) and a bypass.
    int looperParams = 0;
    for (auto* p : plugin->getParameters())
        if (! p->getName (64).startsWith ("MIDI CC") && p->getName (64) != "Bypass")
            ++looperParams;
    check (looperParams == 400, "400 looper parameters exposed (" + juce::String (looperParams) + ")");

    plugin->enableAllBuses();
    plugin->setPlayConfigDetails (2, 2, 48000.0, 512);
    plugin->prepareToPlay (48000.0, 512);

    auto* recPlay = findParam (*plugin, "T1 Rec/Play");
    auto* quantize = findParam (*plugin, "Quantize");
    auto* recAction = findParam (*plugin, "Rec Action");
    auto* monitor = findParam (*plugin, "Input Monitor");
    check (recPlay != nullptr && quantize != nullptr && recAction != nullptr && monitor != nullptr, "parameters found by name");
    if (recPlay == nullptr || quantize == nullptr || recAction == nullptr || monitor == nullptr)
        return 1;

    quantize->setValueNotifyingHost (0.0f);    // Off
    recAction->setValueNotifyingHost (1.0f);   // Rec > Play
    monitor->setValueNotifyingHost (0.0f);     // only loops in the output

    float peak = 0.0f;
    processBlocks (*plugin, 2, 0.0f, peak);
    check (peak == 0.0f, "silent while idle");

    // record 1 second of DC 0.5, then play it back with silent input
    pressParam (*plugin, recPlay);
    processBlocks (*plugin, 94, 0.5f, peak);
    pressParam (*plugin, recPlay);
    processBlocks (*plugin, 40, 0.0f, peak);
    check (peak > 0.45f && peak < 0.55f, "recorded loop plays back (peak " + juce::String (peak, 3) + ")");

    // stop via parameter
    if (auto* stop = findParam (*plugin, "T1 Stop"))
    {
        pressParam (*plugin, stop);
        processBlocks (*plugin, 2, 0.0f, peak);       // 5 ms declick fade
        processBlocks (*plugin, 10, 0.0f, peak);
        check (peak < 1.0e-4f, "stop silences the loop");
    }

    // state round trip
    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    check (state.getSize() > 1000, "state contains the loop audio (" + juce::String ((int) state.getSize()) + " bytes)");

    auto second = formats.createPluginInstance (desc, 48000.0, 512, error);
    check (second != nullptr, "second instance created");
    if (second != nullptr)
    {
        second->enableAllBuses();
        second->setPlayConfigDetails (2, 2, 48000.0, 512);
        second->setStateInformation (state.getData(), (int) state.getSize());
        second->prepareToPlay (48000.0, 512);
        auto* mon2 = findParam (*second, "Input Monitor");
        check (mon2 != nullptr && mon2->getValue() < 0.01f, "parameters restored");
        if (auto* play = findParam (*second, "T1 Rec/Play"))
        {
            pressParam (*second, play);             // stopped -> play
            processBlocks (*second, 20, 0.0f, peak);
            check (peak > 0.45f, "restored loop plays (peak " + juce::String (peak, 3) + ")");
        }
        second->releaseResources();
    }

    // editor
    if (plugin->hasEditor())
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorAndMakeActive());
        check (editor != nullptr && editor->getWidth() > 0, "editor can be created");
    }

    plugin->releaseResources();
    plugin.reset();

    std::cout << (failures == 0 ? "\nHOST TEST PASSED" : "\nHOST TEST FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
