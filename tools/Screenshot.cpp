// Renders the livelooper editor into PNG files for the documentation.
// usage: livelooper_screenshot <output directory>

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

using namespace livelooper;

namespace
{
void runBlocks (LiveLooperProcessor& proc, int blocks, float input)
{
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, input * (float) std::sin ((b * 512 + i) * 0.03));
        proc.processBlock (buffer, midi);
    }
}

void press (LiveLooperProcessor& proc, const juce::String& id)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // let earlier triggers reset
    runBlocks (proc, 1, 0.0f);
    proc.pressTrigger (id);
}

void save (juce::Component& c, const juce::File& file, float scale = 1.0f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, out);
    std::cout << "wrote " << file.getFullPathName() << std::endl;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                      : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    LiveLooperProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    auto set = [&proc] (const juce::String& id, float plain) {
        auto* p = proc.getState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };
    set ("quantize", 0.0f);
    set ("recaction", 1.0f);

    // Track 1: playing loop, track 2: overdubbing, track 3: stopped, track 4: recording
    press (proc, "t1_recplay"); runBlocks (proc, 300, 0.4f);
    press (proc, "t1_recplay"); runBlocks (proc, 60, 0.0f);
    press (proc, "t2_recplay"); runBlocks (proc, 200, 0.3f);
    press (proc, "t2_recplay"); runBlocks (proc, 20, 0.0f);
    press (proc, "t2_recplay"); runBlocks (proc, 70, 0.2f);
    press (proc, "t3_recplay"); runBlocks (proc, 150, 0.3f);
    press (proc, "t3_recplay"); runBlocks (proc, 2, 0.0f);
    press (proc, "t3_stop");    runBlocks (proc, 2, 0.0f);
    press (proc, "t4_recplay"); runBlocks (proc, 90, 0.5f);

    set ("t1_fx", 1.0f);
    set ("t2_reverse", 1.0f);
    set ("inA_on", 1.0f);
    set ("trA_on", 1.0f);
    set ("rhythm_on", 1.0f);
    set ("t3_level", 0.8f);
    set ("t2_pan", -0.3f);
    proc.loadDefaultMidiMap();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    runBlocks (proc, 1, 0.0f);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (LiveLooperEditor::kBaseWidth, LiveLooperEditor::kBaseHeight);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    save (*editor, outDir.getChildFile ("livelooper.png"));

    auto* ed = dynamic_cast<LiveLooperEditor*> (editor.get());
    ed->setLearnMode (true);
    proc.startMidiLearn ("t5_recplay");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    save (*editor, outDir.getChildFile ("livelooper-midi-learn.png"));
    proc.cancelMidiLearn();
    ed->setLearnMode (false);

    // track setup panel on its own
    TrackSettingsPanel panel (proc, *ed, 0);
    panel.setLookAndFeel (&editor->getLookAndFeel());
    save (panel, outDir.getChildFile ("livelooper-track-setup.png"), 2.0f);
    panel.setLookAndFeel (nullptr);
    return 0;
}
