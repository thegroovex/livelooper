// Tests of the plug-in processor: parameters, trigger parameters, MIDI mapping through
// processBlock, saving / restoring a project (including the loop audio) and the editor.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

using namespace livelooper;

namespace
{
struct ProcHarness
{
    LiveLooperProcessor proc;
    juce::AudioBuffer<float> buffer { 2, 512 };
    juce::MidiBuffer midi;

    ProcHarness() { proc.prepareToPlay (48000.0, 512); }

    void setParam (const juce::String& id, float plainValue)
    {
        auto* p = proc.getState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    }

    void run (int blocks, float inputValue = 0.0f)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), inputValue, 512);
            proc.processBlock (buffer, midi);
            midi.clear();
        }
    }

    /** Lets the processor's timer reset the momentary parameters (normally the message thread does this). */
    void pumpMessages()
    {
        // CI machines can be slow: keep the message loop running until every momentary
        // parameter has been reset by the processor's timer (at most 3 s)
        const auto start = juce::Time::getMillisecondCounter();
        do
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        while (anyTriggerOn() && juce::Time::getMillisecondCounter() - start < 3000);
    }

    bool anyTriggerOn()
    {
        for (auto* p : proc.getParameters())
            if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                if (params::isTrigger (wp->paramID) && wp->getValue() >= 0.5f)
                    return true;
        return false;
    }
};
} // namespace

class ProcessorTests : public juce::UnitTest
{
public:
    ProcessorTests() : juce::UnitTest ("Processor", "plugin") {}

    void runTest() override
    {
        beginTest ("Every function is a host parameter with a unique id");
        {
            LiveLooperProcessor proc;
            juce::StringArray ids;
            for (auto* p : proc.getParameters())
                ids.add (dynamic_cast<juce::RangedAudioParameter*> (p)->getParameterID());
            juce::StringArray unique (ids);
            unique.removeDuplicates (false);
            expectEquals (unique.size(), ids.size());
            // 5 tracks x 16 + 17 global + 2 x 4 slots x 5
            expectEquals (ids.size(), 8 * (22 + 15) + (17 + 6 + 3 + params::kNumScenes + 2) + 40
                                      + 7 + kNumTracks + 6 + 3 + 2 + 2
                                      + kNumTracks * 3 + 2 /* v1.6 features: loop tricks, recorder, clip export */
                                      + 5 + 4 * params::kNumBeatRows
                                      + 2 /* v1.6 beat library: next / previous */
                                      + 23 /* v1.6 punch FX (4 send + 12 holds + 5), controller audio mode, resample */);   // v1.6 rhythm: variation, fill, switch, drum FX routing x 2, BEATS row mute / solo / level / pitch   // v1.5: beat FX + colour type, colour per track, repeat / stutter, metronome, Link   // 8 tracks x (22 + 15 since 1.4), global (+3 rhythm feel since 1.3, +2 tape stop / filter since 1.4), FX
            expectEquals (proc.getMidiMap().getNumParams(), ids.size(), "every parameter can be MIDI mapped");
            expect (ids.contains ("t3_recplay") && ids.contains ("allstartstop") && ids.contains ("trD_mix"));
        }

        beginTest ("v1.6 parameters are appended in a fixed order: 400 up to 1.5, features (26), rhythm (37), beat library (2)");
        {
            LiveLooperProcessor proc;
            juce::StringArray ids;
            for (auto* p : proc.getParameters())
                ids.add (dynamic_cast<juce::RangedAudioParameter*> (p)->getParameterID());
            expectEquals (ids.indexOf (params::trackId (0, params::kHalfSpeed)), 400, "features16 start at index 400");
            expectEquals (ids.indexOf (params::gClipExport), 425, "features16 end at index 425");
            expectEquals (ids.indexOf (params::gRhythmVariation), 426, "rhythm starts at index 426");
            expectEquals (ids.indexOf (params::beatRowId (params::kBeatPitch, params::kNumBeatRows - 1)), 462, "rhythm ends at index 462");
            expectEquals (ids.indexOf (params::gBeatLibNext), 463);
            expectEquals (ids.indexOf (params::gBeatLibPrev), 464);
            expectEquals (ids.indexOf (params::gSfxType), 465, "v1.6 punch FX block starts at index 465 (parameter 466)");
            expectEquals (ids.indexOf (params::punchHoldId (0)), 469);
            expectEquals (ids.indexOf (params::punchHoldId (11)), 480);
            expectEquals (ids.indexOf (params::gPfxLatch), 485);
            expectEquals (ids.indexOf (params::gCtlAudioMode), 486);
            expectEquals (ids.indexOf (params::gKo2Resample), 487);
            expectEquals (ids.size(), params::kNumParameters);
            expectEquals (ids.size(), 488);
        }

        beginTest ("Trigger parameters drive the looper and reset themselves");
        {
            ProcHarness h;
            h.setParam ("quantize", 0.0f);
            h.setParam ("recaction", 1.0f);
            h.setParam ("monitor", 0.0f);
            h.proc.pressTrigger ("t2_recplay");
            h.run (1, 0.5f);
            expect (h.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Recording);
            h.run (5, 0.5f);
            h.pumpMessages();
            expectEquals (h.proc.getState().getParameter ("t2_recplay")->getValue(), 0.0f, "momentary parameter is off again");
            h.run (1, 0.5f);
            h.proc.pressTrigger ("t2_recplay");
            h.run (1, 0.0f);
            expect (h.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Playing);
            expectEquals (h.proc.getEngine().getTrack (1).getLength(), 7 * 512);
        }

        beginTest ("MIDI notes and CCs from the host control the looper (default map)");
        {
            ProcHarness h;
            h.proc.loadDefaultMidiMap();
            h.setParam ("quantize", 0.0f);
            h.midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);   // C1 = track 1 rec
            h.run (1);
            expect (h.proc.getEngine().getTrack (0).getState() == LoopTrack::State::Recording);

            h.midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 0), 0);         // CC20 = track 1 level
            h.run (1);
            expectWithinAbsoluteError (h.proc.getState().getRawParameterValue ("t1_level")->load(), 0.0f, 1.0e-4f);
        }

        beginTest ("MIDI learn through processBlock");
        {
            ProcHarness h;
            h.proc.startMidiLearn ("t4_reverse");
            h.midi.addEvent (juce::MidiMessage::controllerEvent (5, 80, 127), 0);
            h.run (1);
            expectEquals (h.proc.describeMidiMapping ("t4_reverse"), juce::String ("CC 80 / Ch 5"));
            h.midi.addEvent (juce::MidiMessage::controllerEvent (5, 80, 0), 0);
            h.midi.addEvent (juce::MidiMessage::controllerEvent (5, 80, 127), 1);
            h.run (1);
            expect (h.proc.getState().getRawParameterValue ("t4_reverse")->load() > 0.5f, "toggle switched on");
        }

        beginTest ("Project state restores parameters, MIDI map and loop audio");
        {
            juce::MemoryBlock state;
            std::vector<float> original;
            {
                ProcHarness h;
                h.setParam ("quantize", 0.0f);
                h.setParam ("recaction", 1.0f);
                h.setParam ("t5_level", 1.5f);
                h.proc.loadDefaultMidiMap();
                h.proc.pressTrigger ("t5_recplay");
                for (int b = 0; b < 20; ++b)
                {
                    for (int i = 0; i < 512; ++i)
                        h.buffer.setSample (0, i, 0.4f * (float) std::sin ((b * 512 + i) * 0.02)),
                        h.buffer.setSample (1, i, 0.4f * (float) std::sin ((b * 512 + i) * 0.02));
                    h.proc.processBlock (h.buffer, h.midi);
                }
                h.pumpMessages();
                h.run (1);
                h.proc.pressTrigger ("t5_recplay");
                h.run (1);
                auto& tr = h.proc.getEngine().getTrack (4);
                original.resize ((size_t) tr.getLength());
                std::vector<float> r ((size_t) tr.getLength());
                tr.readLoop (original.data(), r.data());
                h.proc.getStateInformation (state);
            }

            LiveLooperProcessor restored;
            restored.setStateInformation (state.getData(), (int) state.getSize());
            restored.prepareToPlay (48000.0, 512);
            auto& tr = restored.getEngine().getTrack (4);
            expectEquals (tr.getLength(), (int) original.size());
            expect (tr.getState() == LoopTrack::State::Stopped);
            std::vector<float> l ((size_t) tr.getLength()), r ((size_t) tr.getLength());
            tr.readLoop (l.data(), r.data());
            float maxErr = 0.0f;
            for (size_t i = 0; i < l.size(); ++i)
                maxErr = std::max (maxErr, std::abs (l[i] - original[i]));
            expectLessThan (maxErr, 1.0e-5f, "24-bit FLAC round trip");
            expectWithinAbsoluteError (restored.getState().getRawParameterValue ("t5_level")->load(), 1.5f, 1.0e-4f);
            expect (restored.describeMidiMapping ("t1_recplay").isNotEmpty());
        }

        beginTest ("Loops survive a sample-rate change (resampled)");
        {
            ProcHarness h;
            h.setParam ("quantize", 0.0f);
            h.setParam ("recaction", 1.0f);
            h.proc.pressTrigger ("t1_recplay");
            h.run (1, 0.3f);
            h.pumpMessages();
            h.run (93, 0.3f);
            h.proc.pressTrigger ("t1_recplay");
            h.run (1);
            const int len48 = h.proc.getEngine().getTrack (0).getLength();
            h.proc.prepareToPlay (96000.0, 512);
            expectWithinAbsoluteError (h.proc.getEngine().getTrack (0).getLength(), len48 * 2, 2);
        }

        beginTest ("WAV export and import");
        {
            ProcHarness h;
            h.setParam ("quantize", 0.0f);
            h.setParam ("recaction", 1.0f);
            h.proc.pressTrigger ("t1_recplay");
            h.run (1, 0.25f);
            h.pumpMessages();
            h.run (40, 0.25f);
            h.proc.pressTrigger ("t1_recplay");
            h.run (1);
            auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("livelooper_test.wav");
            expect (h.proc.exportTrack (0, file));
            juce::String error;
            expect (h.proc.importTrack (3, file, error), error);
            expectEquals (h.proc.getEngine().getTrack (3).getLength(), h.proc.getEngine().getTrack (0).getLength());
            file.deleteFile();
        }

        beginTest ("Editor opens, scales and shows MIDI learn");
        {
            LiveLooperProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            expect (editor != nullptr);
            expectEquals (editor->getWidth(), LiveLooperEditor::kBaseWidth);
            editor->setSize (LiveLooperEditor::kBaseWidth / 2 * 3 / 2, LiveLooperEditor::kBaseHeight / 2 * 3 / 2);
            auto* ed = dynamic_cast<LiveLooperEditor*> (editor.get());
            ed->setLearnMode (true);
            expect (ed->isLearnMode());
            auto img = editor->createComponentSnapshot (editor->getLocalBounds());
            expect (img.isValid());
            ed->setLearnMode (false);
        }
    }
};

static ProcessorTests processorTests;
