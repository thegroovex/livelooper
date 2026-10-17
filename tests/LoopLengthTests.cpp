// v1.7 loop length in the GUI (branch v1.7-wip): the per-track loop length pill on the track strips and
// the Live view cards (bound to the existing "Loop Length" parameter t*_measure), the one-shot length
// ("Loop Length Once", t*_reclen) shown in the pill, and the Push 1 FIXED LENGTH hold gesture
// (pads / encoders choose the length, LED, display), with the old FIXED LENGTH tap kept.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Parameters.h"
#include "plugin/LiveView.h"
#include "plugin/push/PushSurface.h"

using namespace livelooper;
using namespace livelooper::params;

namespace
{
struct LenRig
{
    LiveLooperProcessor proc { false, false };
    juce::AudioBuffer<float> buffer { 2, 480 };
    juce::MidiBuffer midi;

    LenRig()
    {
        proc.prepareToPlay (48000.0, 480);
        set ("quantize", 0.0f);
        set ("recaction", 1.0f);
        set ("sync", 0.0f);
        set ("monitor", 0.0f);
    }
    void set (const juce::String& id, float v) { proc.getState().getParameter (id)->setValueNotifyingHost (v); }
    float get (const juce::String& id)         { return proc.getState().getParameter (id)->getValue(); }
    int choice (int t)
    {
        auto* p = proc.getState().getParameter (trackId (t, kMeasure));
        return juce::roundToInt (p->convertFrom0to1 (p->getValue()));
    }
    void setChoice (int t, int c)
    {
        auto* p = proc.getState().getParameter (trackId (t, kMeasure));
        p->setValueNotifyingHost (p->convertTo0to1 ((float) c));
    }
    void run (int blocks, float in)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), in, 480);
            proc.processBlock (buffer, midi);
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    if (isTrigger (r->getParameterID()) && r->getValue() >= 0.5f)
                        r->setValueNotifyingHost (0.0f);
        }
    }
    LoopTrack::State state (int t) { return (LoopTrack::State) proc.getEngine().getTrackStatus (t).state.load(); }
};

struct PushLenRig : LenRig
{
    push::PushSurface surface { proc };
    void button (int number, bool down) { surface.handleMidi (juce::MidiMessage::controllerEvent (1, number, down ? 127 : 0)); }
    void pad (int row, int col, bool down)
    {
        surface.handleMidi (down ? juce::MidiMessage::noteOn (1, push::padNote (row, col), (juce::uint8) 100)
                                 : juce::MidiMessage::noteOff (1, push::padNote (row, col)));
    }
    void tap (int row, int col) { pad (row, col, true); run (1, 0.0f); pad (row, col, false); }
    void turn (int encoder, int value) { surface.handleMidi (juce::MidiMessage::controllerEvent (1, encoder, value)); }
    int captureBars() { return kCaptureBars[juce::roundToInt (get (gCaptureBars) * (float) (kNumCaptureChoices - 1))]; }
    std::vector<juce::MidiMessage> render() { std::vector<juce::MidiMessage> out; surface.render (out, false); return out; }
};

template <typename T>
void findAll (juce::Component& root, std::vector<T*>& out)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c))
            out.push_back (t);
        findAll (*c, out);
    }
}
} // namespace

class LoopLengthTests : public juce::UnitTest
{
public:
    LoopLengthTests() : juce::UnitTest ("Loop length in the GUI + Push (1.7)", "livelooper") {}

    void runTest() override
    {
        beginTest ("Parameters: same ids and count, one concept in the names (Loop Length / Loop Length Once)");
        {
            LenRig r;
            expectEquals (r.proc.getParameters().size(), 501, "no parameter added");
            expect (r.proc.getState().getParameter ("t1_measure") != nullptr);
            expect (r.proc.getState().getParameter ("t1_reclen") != nullptr);
            expectEquals (englishName (trackId (0, kMeasure)), juce::String ("T1 Loop Length"));
            expectEquals (englishName (trackId (2, kRecLen)), juce::String ("T3 Loop Length Once"));
            expectEquals (i18n::germanFor ("Loop Length"), juce::String::fromUTF8 ("Loop-L\xc3\xa4nge"));
            expectEquals (i18n::germanFor ("Loop Length Once"), juce::String::fromUTF8 ("Loop-L\xc3\xa4nge einmalig"));
            for (auto* s : { "Loop length", "AUTO", "T%1 loop length: %2", "T%1 Bars", "Loop length - track %1",
                             "Auto (ends with the next press)", "Applies to the next new recording (after CLEAR)",
                             "LOOP LENGTH  pads: AUTO/1/2/4/8/16  encoder: step  alone: capture",
                             "Next recording only: %1 (set on the controller); the setting stays %2",
                             "The track holds a loop: the length applies to the next new recording (after CLEAR)" })
                expect (i18n::germanFor (s).isNotEmpty(), juce::String ("German text for ") + s);
            for (int t = 1; t <= 8; ++t)
                expect (i18n::germanFor ("T%1 Bars").replace ("%1", juce::String (t)).length() <= 7, "Push column (7 characters)");
            expect (i18n::germanFor ("LOOP LENGTH  pads: AUTO/1/2/4/8/16  encoder: step  alone: capture").length() <= push::kDisplayChars,
                    "Push status line fits 68 characters");
        }

        beginTest ("Pill: bound to t*_measure (MIDI learn id), text, set / step, inactive while a loop exists");
        {
            LenRig r;
            LoopLengthPill pill (r.proc, 1);
            expectEquals (getParamId (pill), trackId (1, kMeasure), "right-click MIDI learn targets the Loop Length parameter");
            expectEquals (pill.getChoice(), 0);
            expectEquals (pill.getText(), juce::String ("AUTO"));
            pill.setChoice (3);
            expectEquals (r.choice (1), 3, "the pill writes the host parameter");
            expectEquals (pill.getText(), juce::String ("4 BARS"));
            r.setChoice (1, 1);
            expectEquals (pill.getText(), juce::String ("1 BAR"), "follows the parameter (automation / MIDI)");
            pill.setChoice (99);
            expectEquals (r.choice (1), (int) kMeasureChoices.size() - 1, "clamped to 16 bars");
            pill.setChoice (-3);
            expectEquals (r.choice (1), 0, "clamped to AUTO");
            expect (! pill.isInactive(), "empty track: active");
            expectEquals (LoopLengthPill::choiceText (5), juce::String ("16 BARS"));
            {
                // German
                i18n::setSetting (i18n::Language::German, false);
                expectEquals (LoopLengthPill::choiceText (2), juce::String ("2 TAKTE"));
                expectEquals (LoopLengthPill::choiceText (1), juce::String ("1 TAKT"));
                i18n::setSetting (i18n::Language::English, false);
            }
            expect (pill.getPreferredWidth (20) >= 40 && pill.getPreferredWidth (20) <= 90, "compact: " + juce::String (pill.getPreferredWidth (20)));

            // fixed length recording: shows the length, becomes inactive once the loop exists
            r.setChoice (1, 1);                                   // 1 bar = 2 s at 120 BPM
            r.proc.pressTrigger (trackId (1, kRecPlay));
            r.run (20, 0.3f);
            expect (r.state (1) == LoopTrack::State::Recording);
            expectEquals (r.proc.getEngine().getTrackStatus (1).recBars.load(), 1, "recording span in bars");
            expect (! pill.isInactive());
            r.run (260, 0.3f);
            expect (r.state (1) == LoopTrack::State::Playing, "1 bar later the recording ended by itself");
            expectEquals (r.proc.getEngine().getTrackStatus (1).recBars.load(), 0);
            expect (pill.isInactive(), "loop exists: the length waits for the next new recording");
            expectEquals (pill.getText(), juce::String ("1 BAR"));
            expect (pill.getTooltip().contains ("next new recording"), pill.getTooltip());
        }

        beginTest ("Pill: the one-shot length (Loop Length Once / Push REC pad + pad) is shown, the setting stays");
        {
            LenRig r;
            LoopLengthPill pill (r.proc, 0);
            r.set (trackId (0, kRecLen), 4.0f / 6.0f);           // 8 bars, fires once
            r.run (2, 0.0f);
            expectEquals (r.proc.getEngine().getTrackStatus (0).recBars.load(), 8);
            expectEquals (pill.getText(), juce::String ("8 BARS"));
            expectEquals (pill.getChoice(), 0, "the Loop Length setting stays AUTO");
            expect (pill.getTooltip().contains ("Next recording only"), pill.getTooltip());
        }

        beginTest ("Editor: one pill per track strip in the title row (5 and 8 tracks), not over name or LED");
        {
            for (int tracks : { 5, 8 })
            {
                LenRig r;
                r.proc.setVisibleTracks (tracks);
                std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc.createEditor());
                std::vector<TrackStrip*> strips;
                findAll (*ed, strips);
                int shown = 0;
                for (auto* s : strips)
                {
                    if (! s->isVisible())
                        continue;
                    ++shown;
                    auto& pill = s->getLengthPill();
                    expect (pill.isVisible());
                    expectEquals (getParamId (pill), trackId (s->getTrackIndex(), kMeasure));
                    const auto b = pill.getBounds();
                    expect (b.getY() >= 8 && b.getBottom() <= 8 + 26, "in the title row");
                    expect (b.getRight() <= s->getWidth() - 10 - 16, "left of the LED");
                    expect (b.getX() >= 10 + 70, "room for the track name: x = " + juce::String (b.getX()));
                    expect (b.getWidth() >= 50, "wide enough: " + juce::String (b.getWidth()));
                }
                expectEquals (shown, tracks);
            }
        }

        beginTest ("Live view: the pill on the cards while the track is empty / recording, hidden once a loop exists");
        {
            LenRig r;
            std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc.createEditor());
            auto* e = dynamic_cast<LiveLooperEditor*> (ed.get());
            r.set (gLiveView, 1.0f);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            auto* lv = e->getLiveView();
            expect (lv != nullptr);
            if (lv == nullptr)
                return;
            lv->setBounds (0, 0, 1180, 856);
            r.setChoice (0, 1);
            r.proc.pressTrigger (trackId (0, kRecPlay));
            r.run (300, 0.3f);
            expect (r.state (0) == LoopTrack::State::Playing);
            lv->refresh (false);
            expect (lv->getLengthPill (0) != nullptr && ! lv->getLengthPill (0)->isVisible(), "track 1 holds a loop: no pill");
            expect (lv->getLengthPill (1)->isVisible(), "track 2 empty: pill shown");
            const auto card = lv->getCardArea (1);
            expect (card.contains (lv->getLengthPill (1)->getBounds()), "inside its card");
            expectEquals (getParamId (*lv->getLengthPill (1)), trackId (1, kMeasure));
            r.set (gLiveView, 0.0f);
        }

        beginTest ("Push 1: hold FIXED LENGTH + pad = loop length of that track (rows 3 - 8: AUTO .. 16), tap alone = capture");
        {
            PushLenRig r;
            const int capture = r.captureBars();
            r.button (push::cc::fixedLength, true);
            expect (r.surface.isLengthHeld());
            r.tap (2 + 3, 2);                                     // row 6 from the bottom -> 4 bars on track 3
            expectEquals (r.choice (2), 3);
            expectEquals (r.surface.getSelectedTrack(), 2, "the track gets selected");
            expect (r.surface.getNotice().contains ("4 BARS"), r.surface.getNotice());
            r.tap (2, 0);                                         // row 3 = AUTO
            expectEquals (r.choice (0), 0);
            r.tap (7, 0);                                         // top row = 16 bars
            expectEquals (r.choice (0), 5);
            r.tap (0, 4);                                         // REC row: only selects, no recording
            expect (r.state (4) == LoopTrack::State::Empty, "no recording started");
            expectEquals (r.surface.getSelectedTrack(), 4);
            r.button (push::cc::fixedLength, false);
            expect (! r.surface.isLengthHeld());
            expectEquals (r.captureBars(), capture, "used for a length: the capture length stays");

            // pad releases after the button: swallowed (no slice / stop)
            r.button (push::cc::fixedLength, true);
            r.pad (2 + 2, 1, true);
            r.button (push::cc::fixedLength, false);
            r.pad (2 + 2, 1, false);
            expectEquals (r.choice (1), 2);

            // tap alone: the capture length as before
            r.button (push::cc::fixedLength, true);
            r.button (push::cc::fixedLength, false);
            expect (r.captureBars() != capture, "FIXED LENGTH alone still steps the capture length");
        }

        beginTest ("Push 1: hold FIXED LENGTH + encoder steps the length; LED, pads and display show it; works in every mode");
        {
            PushLenRig r;
            r.button (push::cc::fixedLength, true);
            for (int i = 0; i < 3; ++i)
                r.turn (push::kEncoderFirst + 1, 1);              // three ticks = one step
            expectEquals (r.choice (1), 1, "T2: 1 bar");
            for (int i = 0; i < 6; ++i)
                r.turn (push::kEncoderFirst + 1, 1);
            expectEquals (r.choice (1), 3, "T2: 4 bars");
            const auto lines = r.surface.buildDisplay();
            expect (lines[0].contains ("T2 Bars"), lines[0]);
            expect (lines[1].contains (">4"), "selected track marked: " + lines[1]);
            expect (lines[1].contains ("AUTO"), lines[1]);
            expect (lines[3].contains ("LOOP LENGTH") || lines[3].contains ("T2 loop length"), lines[3]);
            r.render();
            expectEquals (r.surface.getPadColour (2 + 3, 1), push::pad::orange, "chosen length lit");
            expectEquals (r.surface.getPadColour (2 + 1, 1), push::pad::orangeDim, "other lengths dim");
            expectEquals (r.surface.getPadColour (2, 0), push::pad::white, "track 1: AUTO lit");
            expectEquals (r.surface.getButtonState (push::cc::fixedLength), push::button::on);
            r.button (push::cc::fixedLength, false);
            r.render();
            expectEquals (r.surface.getButtonState (push::cc::fixedLength), push::button::on, "selected T2 has 4 bars: LED on");
            r.tap (1, 0);                                         // select track 1 (STOP pad; empty track)
            r.render();
            expectEquals (r.surface.getSelectedTrack(), 0);
            expectEquals (r.surface.getButtonState (push::cc::fixedLength), push::button::dim, "T1 AUTO: LED dim");
            expect (! r.surface.buildDisplay()[0].contains ("Bars"), "normal display after release");

            // other modes: FX PAD (NOTE) and STEP (SCALES) - the pads choose lengths while the button is held
            for (int modeButton : { push::cc::note, push::cc::scales })
            {
                r.button (modeButton, true);
                r.button (modeButton, false);
                r.button (push::cc::fixedLength, true);
                r.tap (2 + 2, 3);
                r.button (push::cc::fixedLength, false);
                expectEquals (r.choice (3), 2, "mode " + juce::String (modeButton));
                r.setChoice (3, 0);
            }
            expect (r.surface.getMode() == push::PushSurface::Mode::Step);
            r.button (push::cc::session, true);
            r.button (push::cc::session, false);
            expect (r.surface.getMode() == push::PushSurface::Mode::Loop);
        }

        beginTest ("Push 1: REC pad + position pad (one-shot length) unchanged");
        {
            PushLenRig r;
            r.pad (0, 0, true);                                   // REC pad held (recording starts)
            r.run (1, 0.2f);
            r.pad (2 + 3, 0, true);                               // 8 bars for this recording
            r.run (1, 0.2f);
            r.pad (2 + 3, 0, false);
            r.pad (0, 0, false);
            r.run (2, 0.2f);
            expectEquals (r.proc.getEngine().getTrackStatus (0).recBars.load(), 8, "this recording: 8 bars");
            expectEquals (r.choice (0), 0, "Loop Length setting untouched");
        }
    }
};

static LoopLengthTests loopLengthTests;
