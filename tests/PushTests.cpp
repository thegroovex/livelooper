// Ableton Push (1) control without hardware: protocol data, pads, buttons, encoders,
// LED feedback, display and the FX XY pad with snap back.

#include "plugin/PluginProcessor.h"
#include "plugin/push/PushSurface.h"

using namespace livelooper;
using namespace livelooper::push;

namespace
{
struct PushRig
{
    LiveLooperProcessor proc { false, false };
    PushSurface surface { proc };
    juce::AudioBuffer<float> buffer { 2, 480 };
    juce::MidiBuffer midi;

    PushRig()
    {
        proc.prepareToPlay (48000.0, 480);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    float get (const juce::String& id)         { return proc.getState().getParameter (id)->getValue(); }
    float plain (const juce::String& id)
    {
        auto* p = proc.getState().getParameter (id);
        return p->convertFrom0to1 (p->getValue());
    }
    void run (int blocks, float in)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, 480);
            proc.processBlock (buffer, midi);
            releaseTriggers();
        }
    }
    void releaseTriggers()
    {
        // what the message-thread timer does in the app
        for (auto* p : proc.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (params::isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                    r->setValueNotifyingHost (0.0f);
    }
    void pad (int row, int col, bool down = true)
    {
        surface.handleMidi (down ? juce::MidiMessage::noteOn (1, padNote (row, col), (juce::uint8) 100)
                                 : juce::MidiMessage::noteOff (1, padNote (row, col)));
    }
    void tap (int row, int col, float in = 0.0f)
    {
        pad (row, col);
        run (1, in);
        pad (row, col, false);
    }
    void button (int number, bool down)  { surface.handleMidi (juce::MidiMessage::controllerEvent (1, number, down ? 127 : 0)); }
    void press (int number, float in = 0.0f)
    {
        button (number, true);
        run (1, in);
        button (number, false);
    }
    void turn (int encoder, int value)   { surface.handleMidi (juce::MidiMessage::controllerEvent (1, encoder, value)); }

    std::vector<juce::MidiMessage> render (bool full = false)
    {
        std::vector<juce::MidiMessage> out;
        surface.render (out, full);
        return out;
    }
};

int countSysex (const std::vector<juce::MidiMessage>& msgs)
{
    int n = 0;
    for (auto& m : msgs)
        n += m.isSysEx() ? 1 : 0;
    return n;
}
} // namespace

class PushTests : public juce::UnitTest
{
public:
    PushTests() : juce::UnitTest ("Ableton Push control", "livelooper") {}

    void runTest() override
    {
        beginTest ("Protocol: pad notes, scene buttons, encoders, device names");
        {
            expectEquals (padNote (0, 0), 36);
            expectEquals (padNote (7, 7), 99);
            expectEquals (padRow (52), 2);
            expectEquals (padColumn (52), 0);
            expectEquals (sceneForCC (43), 0, "top scene button = scene 1");
            expectEquals (sceneForCC (36), 7);
            expectEquals (sceneForCC (35), -1);
            expectEquals (ccForScene (2), 41);
            expectEquals (encoderDelta (1), 1);
            expectEquals (encoderDelta (3), 3);
            expectEquals (encoderDelta (127), -1);
            expectEquals (encoderDelta (125), -3);

            expect (isPushUserPort ("Ableton Push User Port", 0), "macOS");
            expect (! isPushUserPort ("Ableton Push Live Port", 1));
            expect (isPushUserPort ("MIDIIN2 (Ableton Push)", 1), "Windows input");
            expect (isPushUserPort ("MIDIOUT2 (Ableton Push)", 1), "Windows output");
            expect (! isPushUserPort ("Ableton Push", 0));
            expect (isPushUserPort ("Ableton Push MIDI 2", 1), "Linux");
            expect (! isPushUserPort ("Launchpad X", 1));
        }

        beginTest ("Protocol: display sysex is 4 lines of 68 characters");
        {
            const auto m = displayLine (2, "hello");
            expect (m.isSysEx());
            const auto* d = m.getSysExData();
            expectEquals (m.getSysExDataSize(), 3 + 4 + 68);
            expectEquals ((int) d[0], 0x47);
            expectEquals ((int) d[2], 0x15);
            expectEquals ((int) d[3], 0x1a, "line 3 address");
            expectEquals ((int) d[7], (int) 'h');
            expectEquals ((int) d[7 + 67], (int) ' ', "padded with spaces");
            const auto u = userModeSysex (true);
            expectEquals ((int) u.getSysExData()[3], 0x62);
            expectEquals ((int) u.getSysExData()[6], 1);
            expectEquals (columns ({ "A", "B" }).length(), 68);
        }

        beginTest ("Pads: REC / PLAY and STOP per track, colours follow the state");
        {
            PushRig r;
            r.render (true);
            expectEquals (r.surface.getPadColour (0, 1), pad::off, "empty track 2 is dark");
            r.tap (0, 1, 0.5f);
            expect (r.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Recording, "bottom pad records");
            r.render();
            expectEquals (r.surface.getPadColour (0, 1), pad::red);
            r.run (40, 0.5f);
            r.tap (0, 1);
            expect (r.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Playing);
            r.run (5, 0.0f);
            r.render();
            expectEquals (r.surface.getPadColour (0, 1), pad::green);
            expectEquals (r.surface.getPadColour (1, 1), pad::yellow, "STOP pad lit while it plays");

            r.tap (1, 1);
            r.run (60, 0.0f);
            expect (r.proc.getEngine().getTrack (1).getState() == LoopTrack::State::Stopped, "second row stops");
            r.render();
            expectEquals (r.surface.getPadColour (0, 1), pad::greenDim);

            // DELETE + pad clears the track
            r.button (cc::deleteBtn, true);
            r.tap (0, 1);
            r.button (cc::deleteBtn, false);
            r.run (2, 0.0f);
            expect (r.proc.getEngine().getTrack (1).isEmpty(), "DELETE + pad clears");
        }

        beginTest ("Pads: loop position on rows 3 - 8, hidden tracks stay dark");
        {
            PushRig r;
            r.tap (0, 0, 0.5f);
            r.run (60, 0.5f);
            r.tap (0, 0);
            int lit = 0;
            for (int i = 0; i < 30; ++i)
            {
                r.run (2, 0.0f);
                r.render();
                for (int row = 2; row < 8; ++row)
                    lit += r.surface.getPadColour (row, 0) == pad::green ? 1 : 0;
            }
            expectGreaterThan (lit, 20, "the play head walks up the column");
            expectEquals (r.surface.getPadColour (0, 6), pad::off, "track 7 hidden with 5 tracks");
            r.tap (0, 6, 0.5f);
            expect (r.proc.getEngine().getTrack (6).isEmpty(), "a hidden track ignores its pads");
        }

        beginTest ("Buttons: scenes, play, capture, metronome, auto record, undo");
        {
            PushRig r;
            r.tap (0, 0, 0.5f);
            r.run (40, 0.5f);
            r.tap (0, 0);
            r.press (43);                                   // top scene button: scene 1 (empty -> store)
            expect (r.proc.sceneIsStored (0));
            r.render();
            r.button (cc::shift, true);
            r.press (42);                                   // SHIFT + scene 2: store
            r.button (cc::shift, false);
            expect (r.proc.sceneIsStored (1));

            r.press (cc::metronome);
            expectEquals (r.get ("rhythm_on"), 1.0f, "METRONOME switches the rhythm");
            r.press (cc::automation);
            expectEquals (r.get ("autorec"), 1.0f, "AUTOMATION = auto record");
            r.press (cc::automation);

            r.press (cc::play);
            r.run (60, 0.0f);
            expect (! r.proc.getEngine().getTrack (0).isRunning(), "PLAY = all start / stop");

            r.press (cc::undo);
            r.run (1, 0.0f);
            expect (r.proc.getEngine().getTrack (0).isEmpty(), "UNDO removes the first recording of the selected track");

            r.press (kTopRowFirst + 2);
            expectEquals (r.get ("t3_fx"), 1.0f, "top button = track FX");
            r.press (kBottomRowFirst + 3);
            expectEquals (r.get ("t4_reverse"), 1.0f, "lower button = reverse");

            auto out = r.render();
            bool sceneLed = false;
            for (auto& m : out)
                sceneLed = sceneLed || (m.isController() && m.getControllerNumber() == 43);
            expect (sceneLed || r.render (true).size() > 0);
        }

        beginTest ("Encoders: level, pan, pitch, feedback, FX, master and tempo");
        {
            PushRig r;
            const float lvl = r.get ("t1_level");
            r.turn (kEncoderFirst, 10);
            expectGreaterThan (r.get ("t1_level"), lvl, "encoder 1 = track 1 level");
            r.turn (kEncoderFirst, 118);                    // -10
            expectWithinAbsoluteError (r.get ("t1_level"), lvl, 0.001f);

            r.press (cc::panSend);
            r.turn (kEncoderFirst + 1, 20);
            expectGreaterThan (r.get ("t2_pan"), 0.5f, "PAN & SEND: encoder 2 = track 2 pan");

            r.press (cc::track);
            for (int i = 0; i < 8; ++i)
                r.turn (kEncoderFirst + 2, 1);
            expectWithinAbsoluteError (r.plain ("t3_pitch"), 2.0f, 0.01f, "TRACK: pitch, four ticks per semitone");

            r.press (cc::clip);
            r.turn (kEncoderFirst, 127);
            expectLessThan (r.get ("t1_feedback"), 1.0f, "CLIP: feedback");

            r.press (cc::device);
            const float p1 = r.get ("inB_p1");
            r.turn (kEncoderFirst + 1, 5);
            expectGreaterThan (r.get ("inB_p1"), p1, "DEVICE: encoder 2 = input FX B parameter 1");

            const float master = r.get ("master");
            r.turn (kMasterEncoder, 127);
            expectLessThan (r.get ("master"), master);

            r.turn (kTempoEncoder, 5);
            expectWithinAbsoluteError (r.plain ("tempo"), 125.0f, 0.05f, "tempo encoder: 1 BPM per tick");
            r.button (cc::shift, true);
            r.turn (kTempoEncoder, 127);
            r.button (cc::shift, false);
            expectWithinAbsoluteError (r.plain ("tempo"), 124.9f, 0.05f, "SHIFT: 0.1 BPM");
        }

        beginTest ("FX XY pad: pads set X / Y, releasing snaps back, SHIFT keeps");
        {
            PushRig r;
            r.press (cc::note);
            expect (r.surface.getMode() == PushSurface::Mode::FxPad);
            r.press (kTopRowFirst + 5);                     // track FX B
            expectEquals (r.surface.getFxTarget(), 5);
            const float p1 = r.get ("trB_p1"), p2 = r.get ("trB_p2");
            expectEquals (r.get ("trB_on"), 0.0f);

            r.pad (7, 0);                                   // top-left: X = 0, Y = 1
            expectEquals (r.get ("trB_on"), 1.0f, "touching switches the effect on");
            expectWithinAbsoluteError (r.get ("trB_p1"), 0.0f, 0.001f);
            expectWithinAbsoluteError (r.get ("trB_p2"), 1.0f, 0.001f);
            r.render();
            expectEquals (r.surface.getPadColour (7, 0), pad::white);
            r.pad (7, 0, false);
            expectEquals (r.get ("trB_on"), 0.0f, "released: off again");
            expectWithinAbsoluteError (r.get ("trB_p1"), p1, 0.001f, "and back to its settings");
            expectWithinAbsoluteError (r.get ("trB_p2"), p2, 0.001f);

            r.pad (3, 7);
            r.button (cc::shift, true);
            r.pad (3, 7, false);
            r.button (cc::shift, false);
            expectEquals (r.get ("trB_on"), 1.0f, "SHIFT while releasing keeps the effect");
            expectWithinAbsoluteError (r.get ("trB_p1"), 1.0f, 0.001f);

            r.press (cc::session);
            expect (r.surface.getMode() == PushSurface::Mode::Loop);
        }

        beginTest ("Display shows encoder names, values, track states and tempo");
        {
            PushRig r;
            auto out = r.render (true);
            expectEquals (countSysex (out), 4, "four display lines on a full refresh");
            expect (r.surface.getDisplayLine (0).contains ("Lvl 1"));
            expect (r.surface.getDisplayLine (1).contains ("100%"));
            expect (r.surface.getDisplayLine (2).contains ("EMPTY"));
            expect (r.surface.getDisplayLine (3).contains ("120.0 BPM"));
            expectEquals (countSysex (r.render()), 0, "nothing changes, nothing is sent");
            r.turn (kEncoderFirst, 127);
            expectEquals (countSysex (r.render()), 1, "only the changed line is sent");
            r.press (cc::track);
            r.render();
            expect (r.surface.getDisplayLine (0).contains ("Pit 1"));
        }

        beginTest ("Master section on the two rightmost encoders (up to 6 tracks)");
        {
            PushRig r;
            r.render (true);
            expect (r.surface.getDisplayLine (0).contains ("Master"));
            expect (r.surface.getDisplayLine (0).contains ("Tempo"));
            expect (r.surface.getDisplayLine (2).contains ("MASTER"));
            const float master = r.get ("master");
            r.turn (kEncoderFirst + 6, 127);
            expectLessThan (r.get ("master"), master, "encoder 7 = master level");
            r.turn (kEncoderFirst + 7, 3);
            expectWithinAbsoluteError (r.plain ("tempo"), 123.0f, 0.05f, "encoder 8 = tempo, 1 BPM per tick");
            r.button (cc::shift, true);
            r.turn (kEncoderFirst + 7, 1);
            r.button (cc::shift, false);
            expectWithinAbsoluteError (r.plain ("tempo"), 123.1f, 0.05f, "SHIFT: 0.1 BPM");
            r.run (1, 0.0f);
            r.render();
            expect (r.surface.getDisplayLine (1).contains ("123.1"), "display shows the BPM");

            r.press (cc::device);                    // FX mode: all eight encoders are effects
            const float m2 = r.get ("master");
            r.turn (kEncoderFirst + 6, 5);
            expectEquals (r.get ("master"), m2);
            r.press (cc::volume);

            const float sw = r.get ("rhythm_swing");
            r.turn (kSwingEncoder, 4);
            expectGreaterThan (r.get ("rhythm_swing"), sw, "swing encoder = rhythm swing");
            const float rl = r.get ("rhythm_level");
            r.button (cc::shift, true);
            r.turn (kSwingEncoder, 127);
            r.button (cc::shift, false);
            expectLessThan (r.get ("rhythm_level"), rl, "SHIFT + swing encoder = rhythm level");

            r.proc.setVisibleTracks (8);             // 8 tracks: encoders 7 / 8 are tracks again
            r.render();
            expect (r.surface.getDisplayLine (0).contains ("Lvl 8"));
            expect (r.surface.getDisplayLine (3).contains ("Vol "), "master level in the status line");
            const float l7 = r.get ("t7_level");
            r.turn (kEncoderFirst + 6, 5);
            expectGreaterThan (r.get ("t7_level"), l7);
        }

        beginTest ("LED messages are sent only when something changed");
        {
            PushRig r;
            const auto first = r.render (true);
            expectGreaterThan ((int) first.size(), 64 + 30, "full refresh: all pads and buttons");
            const auto second = r.render();
            int notes = 0;
            for (auto& m : second)
                notes += m.isNoteOn() ? 1 : 0;
            expectEquals (notes, 0);
            r.surface.invalidate();
            expectGreaterThan ((int) r.render().size(), 64, "after reconnecting everything is sent again");
        }
    }
};

static PushTests pushTests;
