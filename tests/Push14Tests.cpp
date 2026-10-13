// Ableton Push (1) gestures added in 1.4, without hardware: loop cutting on the position pads,
// loop length on the pads, footswitches, touch strip, pad pressure, loop moves, momentary
// mute / solo, level meters on the display.

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

class Push14Tests : public juce::UnitTest
{
public:
    Push14Tests() : juce::UnitTest ("Ableton Push 1.4 gestures", "livelooper") {}

    void runTest() override
    {
        beginTest ("Position pads cut a playing track: tap jumps and stays, hold stutters, release returns");
        {
            Rig r;
            r.recordBar (0);
            expect (r.track (0).getState() == LoopTrack::State::Playing);
            r.pad (3, 0);
            expectEquals (r.get (trackId (0, kSlices[1])), 1.0f, "slice 2 held");
            r.run (3);
            r.pad (3, 0, false);
            expectEquals (r.get (trackId (0, kSlices[1])), 0.0f, "released");
            r.run (2);
            expect (r.track (0).isCut() && ! r.track (0).isStuttering(), "tap: jumped and stays");
            expect (r.columnHas (0, pad::cyan), "the play head turns cyan while cut");

            r.pad (6, 0);
            r.run (30);
            expect (r.track (0).isStuttering(), "hold: stutter");
            r.pad (6, 0, false);
            r.run (2);
            expect (! r.track (0).isCut(), "release: back in sync");
            expect (! r.columnHas (0, pad::cyan));
            expectEquals (r.surface.getSelectedTrack(), 0);
        }

        beginTest ("SHIFT + position pad = back in sync, SELECT + pad only selects, stopped tracks select");
        {
            Rig r;
            r.recordBar (0);
            r.tap (2, 0);
            r.run (2);
            expect (r.track (0).isCut());
            r.button (cc::shift, true);
            r.tap (5, 0);
            r.button (cc::shift, false);
            r.run (2);
            expect (! r.track (0).isCut(), "back in sync");

            r.button (cc::select, true);
            r.tap (0, 2);
            r.button (cc::select, false);
            r.run (2);
            expectEquals (r.surface.getSelectedTrack(), 2);
            expect (r.track (2).isEmpty(), "SELECT + REC pad does not record");

            r.tap (1, 0);
            r.run (2);
            expect (r.track (0).getState() == LoopTrack::State::Stopped);
            r.pad (4, 0);
            expectEquals (r.surface.getSelectedTrack(), 0, "stopped track: selected");
            expectEquals (r.get (trackId (0, kSlices[2])), 0.0f, "no cut on a stopped track");
            r.pad (4, 0, false);
        }

        beginTest ("REC pad held + position pad: length of the recording, progress bar on the column");
        {
            Rig r;
            r.pad (0, 1);
            r.run (1, 0.5f);
            expect (r.track (1).getState() == LoopTrack::State::Recording);
            expect (r.columnHas (1, pad::orangeDim), "the six lengths are offered while REC is held");
            r.pad (2, 1);
            r.run (1, 0.5f);
            r.pad (2, 1, false);
            r.pad (0, 1, false);
            expect (r.proc.getEngine().getTrackStatus (1).recordingSpan.load(), "1 bar");
            expect (r.surface.buildDisplay()[3].contains ("1 bar"), "message on the display");
            r.run (kBarP / kBlockP / 2, 0.5f);
            r.render();
            expectEquals (r.surface.getPadColour (2, 1), (int) pad::red, "progress bar: first half filled");
            expectEquals (r.surface.getPadColour (7, 1), (int) pad::off);
            r.run (kBarP / kBlockP / 2 + 2, 0.5f);
            expect (r.track (1).getState() == LoopTrack::State::Playing);
            expectEquals (r.track (1).getLength(), kBarP);

            // on a playing track the REC pad starts an overdub, position pads keep cutting
            r.pad (0, 1);
            r.run (1);
            r.pad (5, 1);
            expectEquals (r.get (trackId (1, kSlices[3])), 1.0f);
            r.pad (5, 1, false);
            r.pad (0, 1, false);
        }

        beginTest ("Footswitch 2: tap REC / PLAY, double tap STOP, hold CLEAR; footswitch 1: next track");
        {
            Rig r;
            r.press (cc::footswitch2);
            r.run (50, 0.5f);
            expect (r.track (0).getState() == LoopTrack::State::Recording, "tap: record");
            r.ms += 1000;
            r.press (cc::footswitch2);
            r.run (2);
            expect (r.track (0).getState() == LoopTrack::State::Playing, "tap: play");
            r.ms += 1000;
            r.press (cc::footswitch2);
            r.run (5);
            expect (r.track (0).getState() == LoopTrack::State::Overdubbing, "tap: overdub");
            r.ms += 150;
            r.press (cc::footswitch2);
            r.run (2);
            expect (r.track (0).getState() == LoopTrack::State::Stopped, "double tap: stop");

            r.ms += 1000;
            r.button (cc::footswitch2, true);
            r.run (1);
            r.ms += 500;
            r.surface.update();
            r.run (1);
            expect (! r.track (0).isEmpty(), "not yet");
            r.ms += 600;
            r.surface.update();
            r.run (1);
            r.button (cc::footswitch2, false);
            expect (r.track (0).isEmpty(), "held 1 s: clear");

            r.press (cc::footswitch1);
            expectEquals (r.surface.getSelectedTrack(), 1);
            for (int i = 0; i < 4; ++i)
                r.press (cc::footswitch1);
            expectEquals (r.surface.getSelectedTrack(), 0, "wraps around the 5 tracks");
        }

        beginTest ("Touch strip: tape stop springs back on release, SHIFT + strip = filter");
        {
            Rig r;
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, kTouchStripNote, (juce::uint8) 127));
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 0));
            expect (r.surface.isStripTouched() && r.surface.getStripMode() == PushSurface::StripMode::TapeStop);
            expectEquals (r.get (gTapeStop), 1.0f, "bottom: tape stands still");
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 16383));
            expectEquals (r.get (gTapeStop), 0.0f, "top: running");
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 4096));
            expect (r.get (gTapeStop) > 0.7f);
            r.surface.handleMidi (juce::MidiMessage::noteOff (1, kTouchStripNote));
            expectEquals (r.get (gTapeStop), 0.0f, "release: springs back");

            r.button (cc::shift, true);
            r.surface.handleMidi (juce::MidiMessage::noteOn (1, kTouchStripNote, (juce::uint8) 127));
            r.button (cc::shift, false);
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 0));
            expect (r.surface.getStripMode() == PushSurface::StripMode::Filter);
            expectWithinAbsoluteError (r.plain (gPerfFilter), -1.0f, 1.0e-3f, "bottom: low-pass");
            r.surface.handleMidi (juce::MidiMessage::pitchWheel (1, 16383));
            expectWithinAbsoluteError (r.plain (gPerfFilter), 1.0f, 1.0e-3f, "top: high-pass");
            r.surface.handleMidi (juce::MidiMessage::noteOff (1, kTouchStripNote));
            expectWithinAbsoluteError (r.plain (gPerfFilter), 0.0f, 1.0e-3f, "release: off");
        }

        beginTest ("FX pad: pad pressure raises the effect mix, releasing snaps it back");
        {
            Rig r;
            r.press (cc::note);
            const auto mixId = fxId (true, 0, kFxMix);
            const float before = r.get (mixId);
            r.pad (4, 4);
            r.surface.handleMidi (juce::MidiMessage::aftertouchChange (1, padNote (4, 4), 127));
            expectWithinAbsoluteError (r.get (mixId), 1.0f, 1.0e-4f, "full pressure = 100 % wet");
            r.surface.handleMidi (juce::MidiMessage::aftertouchChange (1, padNote (4, 4), 0));
            expectWithinAbsoluteError (r.get (mixId), before, 1.0e-4f, "light touch = its own mix");
            r.surface.handleMidi (juce::MidiMessage::channelPressureChange (1, 64));
            expect (r.get (mixId) > before + 0.1f, "channel pressure works too");
            r.pad (4, 4, false);
            expectWithinAbsoluteError (r.get (mixId), before, 1.0e-4f, "snapped back");
            r.surface.handleMidi (juce::MidiMessage::aftertouchChange (1, padNote (4, 4), 127));
            expectWithinAbsoluteError (r.get (mixId), before, 1.0e-4f, "no pads held: ignored");
        }

        beginTest ("DOUBLE / SHIFT + DOUBLE, OCTAVE up / down, DUPLICATE + pads");
        {
            Rig r;
            r.recordBar (0);
            r.press (cc::doubleBtn);
            r.run (1);
            expectEquals (r.track (0).getLength(), 2 * kBarP, "DOUBLE: loop x 2");
            r.button (cc::shift, true);
            r.press (cc::doubleBtn);
            r.button (cc::shift, false);
            r.run (1);
            expectEquals (r.track (0).getLength(), kBarP, "SHIFT + DOUBLE: half");

            r.press (cc::octaveUp);
            expectWithinAbsoluteError (r.plain (trackId (0, kPitch)), 12.0f, 1.0e-3f);
            r.press (cc::octaveDown);
            r.press (cc::octaveDown);
            expectWithinAbsoluteError (r.plain (trackId (0, kPitch)), -12.0f, 1.0e-3f);

            r.button (cc::duplicate, true);
            r.tap (2, 0);
            r.tap (2, 2);
            r.button (cc::duplicate, false);
            r.run (2);
            expectEquals (r.track (2).getLength(), kBarP, "copied to the pressed track");
            expect (r.track (1).isEmpty());

            r.button (cc::duplicate, true);
            r.tap (2, 0);
            r.button (cc::duplicate, false);
            r.run (2);
            expectEquals (r.track (1).getLength(), kBarP, "only the source: next empty track");

            r.button (cc::duplicate, true);
            r.tap (2, 4);           // empty source: ignored
            r.tap (2, 1);           // becomes the source
            r.tap (2, 0);           // track 1 has a loop: not overwritten
            r.button (cc::duplicate, false);
            r.run (2);
            expect (r.track (4).isEmpty());
            expectEquals (r.track (0).getLength(), kBarP);
        }

        beginTest ("MUTE + pad: tap stops, hold mutes while held; SOLO + pad solos while held");
        {
            Rig r;
            r.recordBar (0);
            r.recordBar (1);
            r.run (2);
            expect (r.peak > 0.9f, "both tracks");

            r.button (cc::mute, true);
            r.pad (2, 0);
            r.run (5);
            r.run (2);
            expectEquals (r.get (trackId (0, kMute)), 1.0f, "muted at once");
            expectWithinAbsoluteError (r.peak, 0.5f, 0.01f);
            expect (r.columnHas (0, pad::redDim));
            r.ms += 400;
            r.pad (2, 0, false);
            r.button (cc::mute, false);
            r.run (3);
            expectEquals (r.get (trackId (0, kMute)), 0.0f, "back on release");
            expect (r.track (0).getState() == LoopTrack::State::Playing, "held: still playing");

            r.button (cc::mute, true);
            r.pad (2, 0);
            r.ms += 100;
            r.pad (2, 0, false);
            r.button (cc::mute, false);
            r.run (3);
            expectEquals (r.get (trackId (0, kMute)), 0.0f);
            expect (r.track (0).getState() == LoopTrack::State::Stopped, "tap: stop as before");

            r.tap (0, 0);           // play again
            r.run (2);
            r.button (cc::solo, true);
            r.pad (7, 1);
            r.run (3);
            r.run (2);
            expectEquals (r.get (trackId (1, kSolo)), 1.0f);
            expectWithinAbsoluteError (r.peak, 0.5f, 0.01f, "only the soloed track");
            r.pad (7, 1, false);
            r.button (cc::solo, false);
            r.run (3);
            expectEquals (r.get (trackId (1, kSolo)), 0.0f);
            expect (r.peak > 0.9f, "both again");
        }

        beginTest ("Level meters: MASTER toggles a meter line with Push display bars, rate-limited");
        {
            Rig r;
            r.recordBar (0);
            r.run (2);
            r.render (true);
            r.press (cc::master);
            expect (r.surface.areMetersShown());
            r.surface.update();
            auto line = r.surface.buildDisplay()[2];
            expectEquals (line.length(), 68);
            expect (line.startsWith (">P"), "selected, playing");
            expect (line.substring (2, 8).containsChar (glyph::block), "a level bar (-6 dB)");
            expect (line.substring (10, 16).containsOnly (juce::String::charToString (glyph::dash)), "empty track: no level");
            expect (line.substring (51, 54).contains ("M"), "master meter in the master section");

            // the meter line goes out at most every 40 ms
            r.render (true);
            const auto t0 = r.ms;
            r.button (cc::select, true);
            r.pad (2, 3);
            r.pad (2, 3, false);
            r.button (cc::select, false);
            r.ms = t0 + 10;
            expectEquals (sysexForLine (r.render(), 2), 0, "not within 40 ms");
            r.ms = t0 + 50;
            expectEquals (sysexForLine (r.render(), 2), 1, "sent after the interval");

            // the sysex carries the display's own bar characters
            const auto m = displayLine (2, levelBar (0.5f, 6) + juce::String::fromUTF8 ("\xc3\xbc"));
            expectEquals ((int) m.getSysExData()[7], (int) glyph::block);
            expectEquals ((int) m.getSysExData()[10], (int) glyph::dash);
            expectEquals ((int) m.getSysExData()[13], 26, "u umlaut in the Push character set");
            expectEquals (levelBar (0.0f, 4), juce::String::repeatedString (juce::String::charToString (glyph::dash), 4));
            expectEquals ((int) levelBar (1.0f / 8.0f, 4)[0], (int) glyph::barLeft, "half steps");

            r.press (cc::master);
            expect (! r.surface.areMetersShown());
            expect (! r.surface.buildDisplay()[2].containsChar (glyph::block), "track states again");
        }

        beginTest ("Messages on display line 4 disappear after a moment; 8 tracks put the master meter there");
        {
            Rig r;
            r.press (cc::footswitch1);
            expect (r.surface.buildDisplay()[3].contains ("T2"));
            r.ms += 2000;
            expect (r.surface.buildDisplay()[3].startsWith ("livelooper"));
            r.proc.setVisibleTracks (8);
            r.press (cc::master);
            r.ms += 2000;
            r.surface.update();
            expect (r.surface.buildDisplay()[3].startsWith ("Out "));
            expectEquals (r.surface.buildDisplay()[2].length(), 68);
        }
    }
};

static Push14Tests push14Tests;
