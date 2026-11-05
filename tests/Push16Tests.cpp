// v1.6 Push 1 additions, without hardware: pad pressure chop per track, aftertouch on the XY pad,
// more FX on the XY pad (SHIFT + top buttons), free pads (beat flash, set recording), SHIFT + RECORD,
// PLAY blinking in time in every track mode, encoder double touch pages and track colours.

#include "plugin/PluginProcessor.h"
#include "plugin/push/PushSurface.h"

using namespace livelooper;
using namespace livelooper::push;
using namespace livelooper::params;

namespace
{
constexpr int kBlock16 = 480;

struct PRig
{
    LiveLooperProcessor proc { false, false };
    PushSurface surface { proc };
    juce::AudioBuffer<float> buffer { 2, kBlock16 };
    juce::MidiBuffer midi;
    juce::uint32 ms = 10000;

    PRig()
    {
        proc.prepareToPlay (48000.0, kBlock16);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
        surface.setClock ([this] { return ms; });
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    float get (const juce::String& id)         { return proc.getState().getParameter (id)->getValue(); }
    void run (int blocks, float in = 0.0f)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, kBlock16);
            proc.processBlock (buffer, midi);
            ms += 10;
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                        r->setValueNotifyingHost (0.0f);
        }
    }
    void pad (int row, int col, bool down = true, int vel = 100)
    {
        surface.handleMidi (down ? juce::MidiMessage::noteOn (1, padNote (row, col), (juce::uint8) vel)
                                 : juce::MidiMessage::noteOff (1, padNote (row, col)));
    }
    void polyAT (int row, int col, int value) { surface.handleMidi (juce::MidiMessage::aftertouchChange (1, padNote (row, col), value)); }
    void button (int number, bool down) { surface.handleMidi (juce::MidiMessage::controllerEvent (1, number, down ? 127 : 0)); }
    void press (int number) { button (number, true); button (number, false); }
    void touchEncoder (int i) { surface.handleMidi (juce::MidiMessage::noteOn (1, kEncoderTouchFirst + i, (juce::uint8) 127));
                                surface.handleMidi (juce::MidiMessage::noteOff (1, kEncoderTouchFirst + i)); }
    /** a one-bar loop playing on track t */
    void loop (int t)
    {
        proc.pressTrigger (trackId (t, kRecPlay));
        run (100, 0.4f);
        proc.pressTrigger (trackId (t, kRecPlay));
        run (2, 0.4f);
    }
    int renderedCC (int number)
    {
        std::vector<juce::MidiMessage> out;
        surface.render (out, true);
        return surface.getButtonState (number);
    }
    /** runs until the timeline is in the first or second half of a beat */
    void runToBeatPhase (bool firstHalf)
    {
        for (int i = 0; i < 400; ++i)
        {
            const double ppq = proc.getEngine().getTimelinePpq();
            if ((ppq - std::floor (ppq) < 0.5) == firstHalf && ppq - std::floor (ppq) > 0.02 && ppq - std::floor (ppq) < 0.98)
                return;
            run (1);
        }
    }
};
}

class Push16Tests : public juce::UnitTest
{
public:
    Push16Tests() : juce::UnitTest ("Push 1 v1.6 additions", "livelooper") {}

    void runTest() override
    {
        beginTest ("Poly aftertouch mode sysex (F0 47 7F 15 5C 00 01 01 F7)");
        {
            const auto m = aftertouchModeSysex (true);
            const juce::uint8 expected[] = { 0xf0, 0x47, 0x7f, 0x15, 0x5c, 0x00, 0x01, 0x01, 0xf7 };
            expectEquals (m.getRawDataSize(), (int) sizeof (expected));
            expect (std::memcmp (m.getRawData(), expected, sizeof (expected)) == 0);
            expectEquals ((int) aftertouchModeSysex (false).getRawData()[7], 0);
        }

        beginTest ("LOOP mode: pad pressure chops that track's loop (more pressure = faster), release = normal");
        {
            PRig r;
            r.loop (1);
            r.set (gBfxTarget, 0.0f);
            r.set (gRepeatRate, 0.5f);
            r.pad (4, 1);                    // a position pad of track 2
            r.polyAT (4, 1, 10);
            expectEquals (r.surface.getChopTrack(), -1, "light touch: nothing");
            r.polyAT (4, 1, 30);
            expectEquals (r.surface.getChopTrack(), 1);
            expectEquals (r.get (gRepeatHold), 1.0f);
            expectWithinAbsoluteError (r.get (gBfxTarget), (float) ((int) FxTarget::Track1 + 1) / (float) (kNumFxTargets - 1), 1.0e-4f);
            expectEquals (juce::roundToInt (r.get (gRepeatRate) * 4.0f), 1, "light = 1/4");
            r.polyAT (4, 1, 127);
            expectEquals (juce::roundToInt (r.get (gRepeatRate) * 4.0f), 4, "hard = 1/32");
            r.run (2, 0.4f);
            r.pad (4, 1, false);
            expectEquals (r.surface.getChopTrack(), -1);
            expectEquals (r.get (gRepeatHold), 0.0f);
            expectEquals (r.get (gBfxTarget), 0.0f, "target restored");
            expectWithinAbsoluteError (r.get (gRepeatRate), 0.5f, 1.0e-4f);
            // channel pressure works on the last pad pressed; empty tracks are never chopped
            r.pad (3, 1);
            r.surface.handleMidi (juce::MidiMessage::channelPressureChange (1, 90));
            expectEquals (r.surface.getChopTrack(), 1);
            r.surface.handleMidi (juce::MidiMessage::channelPressureChange (1, 0));
            expectEquals (r.surface.getChopTrack(), -1, "pressure released = normal playback");
            r.pad (3, 1, false);
            r.pad (3, 3);
            r.polyAT (3, 3, 120);
            expectEquals (r.surface.getChopTrack(), -1, "empty track");
            r.pad (3, 3, false);
        }

        beginTest ("FX PAD: poly aftertouch and channel pressure both raise the FX amount");
        {
            PRig r;
            r.press (cc::note);
            r.set (fxId (true, 0, kFxMix), 0.2f);
            r.pad (3, 3);
            r.polyAT (3, 3, 127);
            expectWithinAbsoluteError (r.get (fxId (true, 0, kFxMix)), 1.0f, 1.0e-3f);
            r.surface.handleMidi (juce::MidiMessage::channelPressureChange (1, 0));
            expectWithinAbsoluteError (r.get (fxId (true, 0, kFxMix)), 0.2f, 1.0e-3f);
            r.pad (3, 3, false);
        }

        beginTest ("XY pad: SHIFT + top buttons select beat FX, colour FX, repeat, stutter, punch FX pages (shown on the display)");
        {
            PRig r;
            r.press (cc::note);
            r.button (cc::shift, true);
            r.press (kTopRowFirst + 0);
            r.button (cc::shift, false);
            expect (r.surface.getXyKind() == PushSurface::XyKind::BeatFx);
            r.pad (7, 7);
            expectEquals (r.get (gBfxHold), 1.0f);
            expectWithinAbsoluteError (r.get (gBfxDepth), 1.0f, 1.0e-4f);
            r.pad (7, 7, false);
            expectEquals (r.get (gBfxHold), 0.0f);

            r.button (cc::shift, true);
            r.press (kTopRowFirst + 2);
            r.button (cc::shift, false);
            expect (r.surface.getXyKind() == PushSurface::XyKind::Repeat);
            r.pad (0, 7);
            expectEquals (r.get (gRepeatHold), 1.0f);
            expectEquals (juce::roundToInt (r.get (gRepeatRate) * 4.0f), 4);
            r.pad (0, 7, false);
            expectEquals (r.get (gRepeatHold), 0.0f);

            r.button (cc::shift, true);
            r.press (kTopRowFirst + 5);      // punch page 2: punch 4, 5, 6
            expectEquals (r.surface.getXyPunch(), 3);
            r.press (kTopRowFirst + 5);
            expectEquals (r.surface.getXyPunch(), 4, "press again = next of the page");
            r.button (cc::shift, false);
            expect (r.surface.buildDisplay()[3].contains ("Tape Stop"), r.surface.buildDisplay()[3]);
            r.pad (2, 5);
            expectEquals (r.get (punchHoldId (4)), 1.0f);
            expectWithinAbsoluteError (r.get (gPfxX), 5.0f / 7.0f, 1.0e-4f);
            r.polyAT (2, 5, 64);
            expectWithinAbsoluteError (r.get (gPfxAmount), 64.0f / 127.0f, 1.0e-3f);
            r.pad (2, 5, false);
            expectEquals (r.get (punchHoldId (4)), 0.0f);

            r.press (kTopRowFirst + 1);      // without SHIFT: back to the FX slots
            expect (r.surface.getXyKind() == PushSurface::XyKind::Slot && r.surface.getFxTarget() == 1);
        }

        beginTest ("5 tracks: free top-right pad flashes orange on the beat, the pad below = set recording (red, blinks while recording)");
        {
            PRig r;
            expectEquals (r.proc.getVisibleTracks(), 5);
            r.loop (0);
            r.runToBeatPhase (true);
            std::vector<juce::MidiMessage> out;
            const double ppq = r.proc.getEngine().getTimelinePpq();
            r.surface.render (out, true);
            const bool firstQuarter = ppq - std::floor (ppq) < 0.25;
            expectEquals (r.surface.getPadColour (PushSurface::kBeatPadRow, PushSurface::kFreePadColumn), firstQuarter ? pad::orange : pad::off);
            expectEquals (r.surface.getPadColour (PushSurface::kRecPadRow, PushSurface::kFreePadColumn), pad::redDim);
            r.pad (PushSurface::kRecPadRow, PushSurface::kFreePadColumn);
            r.pad (PushSurface::kRecPadRow, PushSurface::kFreePadColumn, false);
            expectEquals (r.get (gPerfRec), 1.0f);
            r.runToBeatPhase (true);
            r.surface.render (out, true);
            expectEquals (r.surface.getPadColour (PushSurface::kRecPadRow, PushSurface::kFreePadColumn), pad::red);
            r.runToBeatPhase (false);
            r.surface.render (out, true);
            expectEquals (r.surface.getPadColour (PushSurface::kRecPadRow, PushSurface::kFreePadColumn), pad::redDim);
            r.set (gPerfRec, 0.0f);
        }

        beginTest ("8 tracks: SHIFT + RECORD = set recording (RECORD alone = capture), RECORD blinks in time");
        {
            PRig r;
            r.proc.setVisibleTracks (8);
            r.loop (0);
            r.button (cc::shift, true);
            r.press (cc::record);
            r.button (cc::shift, false);
            expectEquals (r.get (gPerfRec), 1.0f);
            r.runToBeatPhase (true);
            expectEquals (r.renderedCC (cc::record), button::on);
            r.runToBeatPhase (false);
            expectEquals (r.renderedCC (cc::record), button::dim);
            r.button (cc::shift, true);
            r.press (cc::record);
            r.button (cc::shift, false);
            expectEquals (r.get (gPerfRec), 0.0f);
            r.press (cc::record);
            expectEquals (r.get (gCapture), 1.0f, "RECORD alone = capture");
        }

        beginTest ("PLAY blinks in time in every track mode");
        {
            for (int tracks : { 5, 8 })
            {
                PRig r;
                r.proc.setVisibleTracks (tracks);
                r.loop (0);
                r.runToBeatPhase (true);
                expectEquals (r.renderedCC (cc::play), button::on, juce::String (tracks) + " tracks, first half");
                r.runToBeatPhase (false);
                expectEquals (r.renderedCC (cc::play), button::dim, juce::String (tracks) + " tracks, second half");
            }
        }

        beginTest ("Encoder double touch: Volume > Pan > Pitch > Feedback > Track colour > Volume; colours per track");
        {
            PRig r;
            expect (r.surface.getEncoderMode() == PushSurface::EncoderMode::Volume);
            r.touchEncoder (2);
            r.ms += 100;
            r.touchEncoder (2);
            expect (r.surface.getEncoderMode() == PushSurface::EncoderMode::Pan);
            r.ms += 1000;
            r.touchEncoder (0);
            r.ms += 1000;
            r.touchEncoder (0);
            expect (r.surface.getEncoderMode() == PushSurface::EncoderMode::Pan, "slow touches do nothing");
            for (int i = 0; i < 3; ++i)
            {
                r.ms += 1000;
                r.touchEncoder (1);
                r.ms += 50;
                r.touchEncoder (1);
            }
            expect (r.surface.getEncoderMode() == PushSurface::EncoderMode::Colour);
            expect (r.surface.buildDisplay()[0].contains ("Col 1"));
            expectEquals (r.proc.getTrackColour (0), -1);
            r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 0, 8));   // 8 ticks = 2 colours
            expectEquals (r.proc.getTrackColour (0), 1);
            expect (r.surface.buildDisplay()[1].contains ("Orange"));
            r.ms += 1000;
            r.touchEncoder (0);
            r.ms += 50;
            r.touchEncoder (0);
            expect (r.surface.getEncoderMode() == PushSurface::EncoderMode::Volume);
            // the colour is saved with the state and shows on the position pads
            juce::MemoryBlock state;
            r.proc.getStateInformation (state);
            LiveLooperProcessor other { false, false };
            other.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (other.getTrackColour (0), 1);
            r.loop (0);
            std::vector<juce::MidiMessage> out;
            r.surface.render (out, true);
            bool orange = false;
            for (int row = 2; row < 8; ++row)
                orange = orange || r.surface.getPadColour (row, 0) == getTrackColourInfo (1).padBright
                                || r.surface.getPadColour (row, 0) == getTrackColourInfo (1).padDim;
            expect (orange, "track colour on the loop position pads");
        }
    }
};

static Push16Tests push16Tests;
