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
            // 5 tracks x 16 + 16 global + 2 x 4 slots x 5
            expectEquals (ids.size(), 5 * 16 + 16 + 40);
            expectEquals (proc.getMidiMap().getNumParams(), ids.size(), "every parameter can be MIDI mapped");
            expect (ids.contains ("t3_recplay") && ids.contains ("allstartstop") && ids.contains ("trD_mix"));
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
