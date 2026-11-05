// Ableton Push (1) mapping of the 1.5 live performance features, without hardware: beat FX,
// beat repeat / stutter (buttons and touch strip), colour FX, capture length, Link, metronome.

#include "plugin/PluginProcessor.h"
#include "plugin/push/PushSurface.h"

using namespace livelooper;
using namespace livelooper::push;
using namespace livelooper::params;

namespace
{
constexpr int kBlockP = 480;
constexpr int kBarP = 96000;

struct Rig
{
    LiveLooperProcessor proc { false, false };
    PushSurface surface { proc };
    juce::AudioBuffer<float> buffer { 2, kBlockP };
    juce::MidiBuffer midi;
    juce::uint32 ms = 10000;
    float peak = 0.0f;

    Rig()
    {
        proc.prepareToPlay (48000.0, kBlockP);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
        surface.setClock ([this] { return ms; });
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    float get (const juce::String& id)         { return proc.getState().getParameter (id)->getValue(); }
    float plain (const juce::String& id)
    {
        auto* p = proc.getState().getParameter (id);
        return p->convertFrom0to1 (p->getValue());
    }
    void run (int blocks, float in = 0.0f)
    {
        peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, kBlockP);
            proc.processBlock (buffer, midi);
            peak = std::max (peak, buffer.getMagnitude (0, 0, kBlockP));
            ms += 10;
            // what the message-thread timer does in the app
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                        r->setValueNotifyingHost (0.0f);
        }
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
    void button (int number, bool down) { surface.handleMidi (juce::MidiMessage::controllerEvent (1, number, down ? 127 : 0)); }
    void press (int number)
    {
        button (number, true);
        run (1);
        button (number, false);
    }
    /** records one bar (fixed length) on track t with the REC pad */
    void recordBar (int t)
    {
        set (trackId (t, kMeasure), 0.2f);
        tap (0, t, 0.5f);
        run (kBarP / kBlockP, 0.5f);
        set (trackId (t, kMeasure), 0.0f);
    }
    LoopTrack& track (int t) { return proc.getEngine().getTrack (t); }
    std::vector<juce::MidiMessage> render (bool full = false)
    {
        std::vector<juce::MidiMessage> out;
        surface.render (out, full);
        return out;
    }
    bool columnHas (int col, int colour)
    {
        render();
        for (int row = 2; row < 8; ++row)
            if (surface.getPadColour (row, col) == colour)
                return true;
        return false;
    }
};

int sysexForLine (const std::vector<juce::MidiMessage>& msgs, int line)
{
    int n = 0;
    for (auto& m : msgs)
        if (m.isSysEx() && m.getSysExDataSize() > 3 && m.getSysExData()[3] == 0x18 + line)
            ++n;
    return n;
}
} // namespace


class Push15Tests : public juce::UnitTest
{
public:
    Push15Tests() : juce::UnitTest ("Ableton Push 1.5 performance FX", "livelooper") {}

    void runTest() override
    {
        beginTest ("ADD EFFECT = beat FX on / off, ADD TRACK = beat FX while held, LEFT / RIGHT = beat");
        {
            Rig r;
            expectEquals (r.get (gBfxOn), 0.0f);
            r.press (cc::addEffect);
            expectEquals (r.get (gBfxOn), 1.0f, "on");
            expect (r.surface.getNotice().isNotEmpty(), "notice on the display");
            r.press (cc::addEffect);
            expectEquals (r.get (gBfxOn), 0.0f, "off");

            r.button (cc::addTrack, true);
            expectEquals (r.get (gBfxHold), 1.0f, "hold: on while pressed");
            r.render();
            r.button (cc::addTrack, false);
            expectEquals (r.get (gBfxHold), 0.0f, "released: off");

            auto* beat = r.proc.getState().getParameter (gBfxBeat);
            const auto before = beat->getCurrentValueAsText();
            r.press (cc::right);
            expect (beat->getCurrentValueAsText() != before, "RIGHT: longer beat");
            r.press (cc::left);
            expectEquals (beat->getCurrentValueAsText(), before, "LEFT: back");
            for (int i = 0; i < 40; ++i)
                r.press (cc::left);
            expectEquals (beat->getValue(), 0.0f, "stops at the shortest beat");
        }

        beginTest ("BROWSE: encoders for beat FX type / beat / level / channel, colour FX, repeat and stutter rate, click level");
        {
            Rig r;
            r.press (cc::browse);
            expect (r.surface.getEncoderMode() == PushSurface::EncoderMode::BeatFx);
            auto lines = r.surface.buildDisplay();
            expect (lines[0].contains ("BeatFX") && lines[0].contains ("Stutter"), "names on line 1: " + lines[0]);
            const auto type = r.get (gBfxType);
            for (int i = 0; i < 8; ++i)
                r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 0, 1));
            expect (r.get (gBfxType) > type, "encoder 1: next beat FX type");
            const auto depth = r.get (gBfxDepth);
            r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 2, 127));   // -1
            expect (r.get (gBfxDepth) < depth, "encoder 3: level down");
            const auto colour = r.get (gColorType);
            for (int i = 0; i < 4; ++i)
                r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 4, 1));
            expect (r.get (gColorType) > colour, "encoder 5: colour FX");
            const auto rate = r.get (gRepeatRate);
            for (int i = 0; i < 4; ++i)
                r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 5, 1));
            expect (r.get (gRepeatRate) > rate, "encoder 6: repeat rate");
            const auto metro = r.get (gMetroLevel);
            r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 7, 127));
            expect (r.get (gMetroLevel) < metro, "encoder 8: metronome level");
            const auto master = r.get (gMaster);
            r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 6, 1));
            expectEquals (r.get (gMaster), master, "no master column in this mode");
            r.press (cc::addEffect);
            lines = r.surface.buildDisplay();
            expect (lines[2].contains ("ON"), "state line shows beat FX on: " + lines[2]);
            r.press (cc::volume);
            expect (r.surface.getEncoderMode() == PushSurface::EncoderMode::Volume);
        }

        beginTest ("REPEAT / ACCENT: beat repeat / stutter while held, SHIFT = latch");
        {
            Rig r;
            r.button (cc::repeat, true);
            expectEquals (r.get (gRepeatHold), 1.0f, "repeat while held");
            r.run (2);
            r.button (cc::repeat, false);
            expectEquals (r.get (gRepeatHold), 0.0f, "released");

            r.button (cc::accent, true);
            expectEquals (r.get (gStutterHold), 1.0f, "stutter while held");
            r.button (cc::accent, false);
            expectEquals (r.get (gStutterHold), 0.0f);

            r.button (cc::shift, true);
            r.press (cc::repeat);
            r.press (cc::accent);
            r.button (cc::shift, false);
            expectEquals (r.get (gRepeatLatch), 1.0f, "SHIFT + REPEAT: latched");
            expectEquals (r.get (gStutterLatch), 1.0f, "SHIFT + ACCENT: latched");
            expectEquals (r.get (gRepeatHold), 0.0f, "latching does not hold");
            std::vector<juce::MidiMessage> out;
            r.surface.render (out, true);
            bool repeatLed = false;
            for (auto& m : out)
                if (m.isController() && m.getControllerNumber() == cc::repeat && m.getControllerValue() == button::on)
                    repeatLed = true;
            expect (repeatLed, "REPEAT lights while latched");
            r.button (cc::shift, true);
            r.press (cc::repeat);
            r.button (cc::shift, false);
            expectEquals (r.get (gRepeatLatch), 0.0f, "unlatched");
        }

        beginTest ("REPEAT / ACCENT held + touch strip: rate 1/2 (bottom) .. 1/32 (top), released = off");
        {
            Rig r;
            auto rateText = [&r] (const juce::String& id) { return r.proc.getState().getParameter (id)->getCurrentValueAsText(); };
            r.button (cc::repeat, true);
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, kTouchStripNote, (juce::uint8) 127));
            expect (r.surface.getStripMode() == PushSurface::StripMode::RepeatRate);
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 0));
            expectEquals (rateText (gRepeatRate), juce::String ("1/2"), "bottom: 1/2");
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 16383));
            expectEquals (rateText (gRepeatRate), juce::String ("1/32"), "top: 1/32");
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 8192));
            expectEquals (rateText (gRepeatRate), juce::String ("1/8"), "middle: 1/8");
            r.button (cc::repeat, false);
            expectEquals (r.get (gRepeatHold), 1.0f, "still repeating while the finger is on the strip");
            r.surface.handleMidi (juce::MidiMessage::noteOff (1, kTouchStripNote));
            expectEquals (r.get (gRepeatHold), 0.0f, "finger off: repeat off");
            expectEquals (r.get (gTapeStop), 0.0f, "tape stop untouched");

            r.button (cc::accent, true);
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, kTouchStripNote, (juce::uint8) 127));
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 13000));
            expect (r.surface.getStripMode() == PushSurface::StripMode::StutterRate);
            expectEquals (rateText (gStutterRate), juce::String ("1/16"), "stutter rate from the strip");
            r.surface.handleMidi (juce::MidiMessage::noteOff (1, kTouchStripNote));
            expectEquals (r.get (gStutterHold), 1.0f, "ACCENT still held: stutter goes on");
            r.button (cc::accent, false);
            expectEquals (r.get (gStutterHold), 0.0f);

            // without REPEAT / ACCENT the strip is the tape stop as before
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, kTouchStripNote, (juce::uint8) 127));
            expect (r.surface.getStripMode() == PushSurface::StripMode::TapeStop);
            r.surface.handleMidi (juce::MidiMessage::noteOff (1, kTouchStripNote));
        }

        beginTest ("FIXED LENGTH = capture length 1 .. 32 bars, SHIFT + METRONOME = Link, SHIFT + TAP = click");
        {
            Rig r;
            auto bars = [&r] { return kCaptureBars[juce::roundToInt (r.get (gCaptureBars) * (float) (kNumCaptureChoices - 1))]; };
            r.set (gCaptureBars, 0.0f);
            expectEquals (bars(), 1);
            juce::Array<int> seen;
            for (int i = 0; i < kNumCaptureChoices; ++i)
            {
                r.press (cc::fixedLength);
                seen.add (bars());
            }
            expect (seen == juce::Array<int> { 2, 4, 8, 16, 32, 1 }, "1 .. 32 bars and around");
            r.button (cc::shift, true);
            r.press (cc::fixedLength);
            r.button (cc::shift, false);
            expectEquals (bars(), 32, "SHIFT: shorter (wraps to 32)");

            const auto rhythm = r.get (gRhythmOn);
            r.button (cc::shift, true);
            r.press (cc::metronome);
            r.button (cc::shift, false);
            expectEquals (r.get (gLinkOn), 1.0f, "SHIFT + METRONOME: Link on");
            expectEquals (r.get (gRhythmOn), rhythm, "rhythm unchanged");
            r.ms += PushSurface::kNoticeMs + 100;      // the notice has gone
            expect (r.surface.buildDisplay()[3].contains ("Link"), "Link on the status line: " + r.surface.buildDisplay()[3]);
            r.button (cc::shift, true);
            r.press (cc::metronome);
            r.press (cc::tapTempo);
            r.button (cc::shift, false);
            expectEquals (r.get (gLinkOn), 0.0f, "Link off");
            expectEquals (r.get (gMetroOn), 1.0f, "SHIFT + TAP TEMPO: metronome click");
            r.press (cc::metronome);
            expect (r.get (gRhythmOn) != rhythm, "METRONOME alone: rhythm as before");
        }

        // ---------------------------------------------------------------- v1.6 rhythm (branch v1.6-rhythm)
        beginTest ("v1.6 SCALES = STEP mode: pads edit the drum pattern, a built-in groove is copied first");
        {
            Rig r;
            auto* pat = r.proc.getState().getParameter (gRhythmPattern);
            pat->setValueNotifyingHost (pat->convertTo0to1 (1.0f));        // Rock
            r.press (cc::scales);
            expect (r.surface.getMode() == PushSurface::Mode::Step, "STEP mode");
            r.run (2);
            r.render();
            expectEquals (r.surface.getPadColour (0, 0), (int) pad::red, "Rock kick on step 1 (accent)");
            expectEquals (r.surface.getPadColour (1, 2), (int) pad::grey, "no snare on step 3 (beat 1 = lit background)");

            // a pad press on a preset: copy into user slot 1, which then plays
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, padNote (1, 2), (juce::uint8) 70));
            r.surface.handleMidi (juce::MidiMessage::noteOff (1, padNote (1, 2)));
            const int user = RhythmGenerator::getNumPatterns();
            expectEquals (juce::roundToInt (r.plain (gRhythmPattern)), user, "the copy plays");
            auto& bank = r.proc.getUserPatterns();
            expectEquals (bank.getCell (0, RhythmGenerator::Snare, 2), 70, "pad velocity = step velocity");
            expectEquals (bank.getCell (0, RhythmGenerator::Kick, 0), 127, "the rest of Rock is there");
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, padNote (1, 2), (juce::uint8) 90));
            expectEquals (bank.getCell (0, RhythmGenerator::Snare, 2), 0, "second press: off");
            r.button (cc::shift, true);
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, padNote (3, 5), (juce::uint8) 20));
            r.button (cc::shift, false);
            expectEquals (bank.getCell (0, RhythmGenerator::HatClosed, 5), 127, "SHIFT + pad = accent");

            // pages of 8 steps
            r.press (cc::right);
            expectEquals (r.surface.getStepPage(), 8);
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, padNote (7, 0), (juce::uint8) 100));
            expectEquals (bank.getCell (0, RhythmGenerator::Tom, 8), 100, "page 2: step 9");
            r.press (cc::right);
            expectEquals (r.surface.getStepPage(), 8, "16 steps: no third page");
            r.press (cc::left);
            expectEquals (r.surface.getStepPage(), 0);

            // DELETE + pad clears the row
            r.button (cc::deleteBtn, true);
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, padNote (0, 3), (juce::uint8) 100));
            r.button (cc::deleteBtn, false);
            for (int s = 0; s < 16; ++s)
                expectEquals (bank.getCell (0, RhythmGenerator::Kick, s), 0);

            // rows: mute / solo buttons, encoders = level / pitch
            r.press (kTopRowFirst + 1);
            expectEquals (r.get (beatRowId (kBeatMute, 1)), 1.0f, "top button 2: snare row mute");
            r.press (kBottomRowFirst + 2);
            expectEquals (r.get (beatRowId (kBeatSolo, 2)), 1.0f, "lower button 3: clap row solo");
            expectEquals (r.get (trackId (1, kTrackFx)), 0.0f, "track FX untouched in STEP mode");
            const float vol = r.plain (beatRowId (kBeatVol, 0));
            r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst, 127));   // one step left
            expectLessThan (r.plain (beatRowId (kBeatVol, 0)), vol, "encoder 1: kick level");
            r.button (cc::shift, true);
            r.surface.handleMidi (juce::MidiMessage::controllerEvent (1, kEncoderFirst + 7, 4));   // 4 ticks = 1 semitone
            r.button (cc::shift, false);
            expectEquals (r.plain (beatRowId (kBeatPitch, 7)), 1.0f, "SHIFT + encoder 8: tom pitch +1");
            const auto lines = r.surface.buildDisplay();
            expect (lines[2].startsWith ("STEP"), lines[2]);
            expect (lines[0].contains ("Kick"), lines[0]);

            // UP / DOWN = pattern, SESSION back
            r.press (cc::up);
            expectEquals (juce::roundToInt (r.plain (gRhythmPattern)), user + 1);
            r.press (cc::session);
            expect (r.surface.getMode() == PushSurface::Mode::Loop);
            r.press (kTopRowFirst + 1);
            expectEquals (r.get (trackId (1, kTrackFx)), 1.0f, "LOOP mode: top buttons = track FX again");
        }

        beginTest ("v1.6 IN = FILL while held (every mode), OUT = drums through beat FX, SHIFT + OUT = through repeat");
        {
            Rig r;
            r.button (cc::inBtn, true);
            expectEquals (r.get (gRhythmFill), 1.0f, "FILL held");
            r.run (1);
            r.button (cc::inBtn, false);
            expectEquals (r.get (gRhythmFill), 0.0f, "released");
            r.press (cc::outBtn);
            expectEquals (r.get (gDrumsBfx), 1.0f);
            r.button (cc::shift, true);
            r.press (cc::outBtn);
            r.button (cc::shift, false);
            expectEquals (r.get (gDrumsRepeat), 1.0f);
            expectEquals (r.get (gDrumsBfx), 1.0f, "SHIFT + OUT leaves beat FX routing alone");
            r.press (cc::scales);
            r.button (cc::inBtn, true);
            expectEquals (r.get (gRhythmFill), 1.0f, "FILL in STEP mode too");
            r.button (cc::inBtn, false);
        }

        beginTest ("v1.6 merge: SHIFT + SCALES is STEP mode too (reverse loop moved to SHIFT + bottom-row button)");
        {
            Rig r;
            r.button (cc::shift, true);
            r.press (cc::scales);
            r.button (cc::shift, false);
            expect (r.surface.getMode() == PushSurface::Mode::Step, "SHIFT + SCALES = STEP mode");
            expect (! r.proc.getEngine().getTrack (0).canUndo(), "SHIFT + SCALES does not reverse a loop");
            r.press (cc::scales);
            expect (r.surface.getMode() == PushSurface::Mode::Loop);
        }

    }
};

static Push15Tests push15Tests;
