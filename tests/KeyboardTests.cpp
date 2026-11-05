// Tests of the computer-keyboard control (standalone app) and the typed-in tempo.

#include "plugin/KeyMap.h"
#include "plugin/KeyMapEditor.h"
#include "plugin/Localisation.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

using namespace livelooper;

namespace
{
juce::KeyPress key (int code, bool shift = false)
{
    return juce::KeyPress (code, shift ? juce::ModifierKeys::shiftModifier : 0, 0);
}

float plain (LiveLooperProcessor& p, const juce::String& id)
{
    auto* param = p.getState().getParameter (id);
    return param->convertFrom0to1 (param->getValue());
}
}

class KeyboardTests : public juce::UnitTest
{
public:
    KeyboardTests() : juce::UnitTest ("Computer keyboard", "livelooper") {}

    void runTest() override
    {
        beginTest ("Default mapping: logical rows per track");
        {
            KeyMap km;
            expectEquals (km.getActionFor (key ('1')), juce::String ("t1_recplay"));
            expectEquals (km.getActionFor (key ('5')), juce::String ("t5_recplay"));
            expectEquals (km.getActionFor (key ('Q')), juce::String ("t1_stop"));
            expectEquals (km.getActionFor (key ('q')), juce::String ("t1_stop"));
            expectEquals (km.getActionFor (key ('T', true)), juce::String ("t5_clear"));
            expectEquals (km.getActionFor (key ('A')), juce::String ("t1_undo"));
            expectEquals (km.getActionFor (key ('G', true)), juce::String ("t5_redo"));
            expectEquals (km.getActionFor (key (juce::KeyPress::spaceKey)), juce::String ("allstartstop"));
            expectEquals (km.getActionFor (key (juce::KeyPress::returnKey)), juce::String ("tap"));
            expectEquals (km.getActionFor (key ('B')), KeyMap::actEnterTempo);
            expectEquals (km.getActionFor (key (juce::KeyPress::upKey, true)), KeyMap::actTempoUpFine);
            expect (km.getActionFor (key ('Z')).isEmpty());

            // every default action is offered in the mapping window
            const auto available = KeyMap::getAvailableActions();
            for (const auto& a : available)
                expect (KeyMap::describeAction (a).isNotEmpty());
            for (const auto& a : { juce::String ("t1_recplay"), juce::String ("tap"), KeyMap::actEnterTempo })
                expect (available.contains (a));
        }

        beginTest ("A key belongs to one function, a function has one key");
        {
            KeyMap km;
            km.assign (key ('1'), "t2_reverse");
            expectEquals (km.getActionFor (key ('1')), juce::String ("t2_reverse"));
            expect (! km.getKeyFor ("t1_recplay").isValid());
            km.assign (key ('9'), "t2_reverse");
            expect (km.getActionFor (key ('1')).isEmpty());
            km.removeAction ("t2_reverse");
            expect (km.getActionFor (key ('9')).isEmpty());
        }

        beginTest ("Key names in English and German");
        {
            i18n::setSetting (i18n::Language::English, false);
            expectEquals (KeyMap::describeKey (key ('Q', true)), juce::String ("Shift + Q"));
            expectEquals (KeyMap::describeKey (key (juce::KeyPress::spaceKey)), juce::String ("Space"));
            i18n::setSetting (i18n::Language::German, false);
            expectEquals (KeyMap::describeKey (key (juce::KeyPress::spaceKey)), juce::String ("Leertaste"));
            expectEquals (KeyMap::describeAction (KeyMap::actEnterTempo), juce::String ("Tempo eintippen (BPM)"));
            for (const auto& a : KeyMap::getAvailableActions())
                if (KeyMap::isSpecialAction (a))
                    expect (KeyMap::describeAction (a) != a);
            i18n::setSetting (i18n::Language::English, false);
        }

        beginTest ("Custom mapping is saved with the state");
        {
            LiveLooperProcessor a;
            a.getKeyMap().clear();
            a.getKeyMap().assign (key ('X', true), "t3_reverse");
            a.getKeyMap().assign (key (juce::KeyPress::leftKey), KeyMap::actTempoDown);
            a.getKeyMap().setEnabled (false);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            LiveLooperProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.getKeyMap().getNumAssignments(), 2);
            expectEquals (b.getKeyMap().getActionFor (key ('X', true)), juce::String ("t3_reverse"));
            expectEquals (b.getKeyMap().getActionFor (key (juce::KeyPress::leftKey)), KeyMap::actTempoDown);
            expect (! b.getKeyMap().isEnabled());
            expect (b.getKeyMap().getActionFor (key ('1')).isEmpty());
        }

        beginTest ("Keyboard control is only active where it is available");
        {
            LiveLooperProcessor p;
            expect (! p.isKeyboardControlAvailable());   // not the standalone wrapper
            expect (! p.isKeyboardControlActive());
            p.setKeyboardControlAvailable (true);
            expect (p.isKeyboardControlActive());
            p.getKeyMap().setEnabled (false);
            expect (! p.isKeyboardControlActive());
        }

        beginTest ("Keys drive the looper through the editor");
        {
            LiveLooperProcessor p;
            p.setKeyboardControlAvailable (true);
            p.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* editor = dynamic_cast<LiveLooperEditor*> (ed.get());
            expect (editor != nullptr);

            expect (editor->keyPressed (key ('M')));                   // toggle rhythm
            expectEquals (plain (p, "rhythm_on"), 1.0f);
            const int patternBefore = juce::roundToInt (plain (p, "rhythm_pattern"));
            editor->performKeyAction ("rhythm_pattern");
            expectEquals (juce::roundToInt (plain (p, "rhythm_pattern")), patternBefore + 1);

            const float before = plain (p, "tempo");
            editor->performKeyAction (KeyMap::actTempoUp);
            expectWithinAbsoluteError (plain (p, "tempo"), before + 1.0f, 0.051f);
            editor->performKeyAction (KeyMap::actTempoDownFine);
            expectWithinAbsoluteError (plain (p, "tempo"), before + 0.9f, 0.051f);

            editor->performKeyAction ("t1_recplay");
            expect (p.getState().getParameter ("t1_recplay")->getValue() >= 0.5f);

            expect (! editor->keyPressed (key ('Z')));                 // unassigned key passes on
            p.getKeyMap().setEnabled (false);
            expect (! editor->keyPressed (key ('N')));
        }

        beginTest ("A track clear key must be held like the STOP button");
        {
            LiveLooperProcessor p;
            p.setKeyboardControlAvailable (true);
            p.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* editor = dynamic_cast<LiveLooperEditor*> (ed.get());
            auto cleared = [&p] (int t) { return p.getState().getParameter (juce::String ("t") + juce::String (t) + "_clear")->getValue() >= 0.5f; };

            expect (editor->keyPressed (key ('W', true)));             // Shift+W: clear track 2
            expectEquals (editor->getPendingKeyClearTrack(), 1);
            expect (! cleared (2));                                    // nothing happens on the press itself
            const auto now = juce::Time::getMillisecondCounter();
            editor->updateKeyClearHold (now + 400, false);             // released too early
            expect (! cleared (2));
            expectEquals (editor->getPendingKeyClearTrack(), -1);

            editor->keyStateChanged (false);
            expect (editor->keyPressed (key ('E', true)));             // Shift+E: clear track 3, held
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + 700, true);
            expect (! cleared (3));
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + LiveLooperEditor::kClearHoldMs + 10, true);
            expect (cleared (3));
            expectEquals (editor->getPendingKeyClearTrack(), -1);
        }

        beginTest ("Keyboard clear (v1.7.1): auto-repeat keeps the hold, release cancels, progress on every STOP button");
        {
            LiveLooperProcessor p;
            p.setKeyboardControlAvailable (true);
            p.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* editor = dynamic_cast<LiveLooperEditor*> (ed.get());
            auto cleared = [&p] (int t) { return p.getState().getParameter (juce::String ("t") + juce::String (t) + "_clear")->getValue() >= 0.5f; };
            const auto hold = LiveLooperEditor::kClearHoldMs;

            // short press: nothing
            expect (editor->keyPressed (key ('Q', true)));
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + hold / 3, true);
            expect (! cleared (1));
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + hold / 2, false);   // released before 1.5 s
            expect (! cleared (1));
            expect (! editor->isKeyClearPending());
            editor->keyStateChanged (false);

            // auto-repeat: many key-downs while held do not clear and do not restart the hold
            expect (editor->keyPressed (key ('R', true)));                  // Shift+R: clear track 4
            const auto t0 = juce::Time::getMillisecondCounter();
            for (int i = 0; i < 40; ++i)
            {
                expect (editor->keyPressed (key ('R', true)));              // the OS key repeat
                expect (! cleared (4));
            }
            expectEquals (editor->getPendingKeyClearTrack(), 3);
            editor->updateKeyClearHold (t0 + hold / 2, true);
            expect (! cleared (4));
            // the hold fill shows on the STOP button of the surface and of the Live and Performance views
            auto* liveStop = editor->getLiveView() != nullptr ? editor->getLiveView()->getStopButton (3) : nullptr;
            auto* perfStop = editor->getPerformView() != nullptr ? editor->getPerformView()->getStopButton (3) : nullptr;
            expect (liveStop != nullptr && liveStop->getKeyHoldProgress() > 0.3f);
            expect (perfStop != nullptr && perfStop->getKeyHoldProgress() > 0.3f);
            expect (editor->getLiveView()->getStopButton (0)->getKeyHoldProgress() < 0.001f);   // other tracks: none
            editor->updateKeyClearHold (t0 + hold + 20, true);                // held past the threshold (from the first press)
            expect (cleared (4));
            expect (! editor->isKeyClearPending());
            expect (liveStop->getKeyHoldProgress() < 0.001f);
            // the key is still held: its repeat after the clear starts nothing new
            expect (editor->keyPressed (key ('R', true)));
            expect (! editor->isKeyClearPending());
            editor->keyStateChanged (false);                                 // (xvfb: no key is down) the key is up

            // switching the keyboard off while the key is held cancels
            expect (editor->keyPressed (key ('T', true)));
            p.getKeyMap().setEnabled (false);
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + hold + 20, true);
            expect (! cleared (5));
            expect (! editor->isKeyClearPending());
            p.getKeyMap().setEnabled (true);
            editor->keyStateChanged (false);

            // a key-up event cancels at once (no key is down under the test display)
            expect (editor->keyPressed (key ('E', true)));
            expect (editor->isKeyClearPending());
            editor->keyStateChanged (false);
            expect (! editor->isKeyClearPending());
            expect (! cleared (3));
        }

        beginTest ("Keyboard CLEAR ALL (v1.7.1) must be held too - no question, a tap does nothing");
        {
            LiveLooperProcessor p;
            p.setKeyboardControlAvailable (true);
            p.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* editor = dynamic_cast<LiveLooperEditor*> (ed.get());
            auto clearedAll = [&p] { return p.getState().getParameter ("clearall")->getValue() >= 0.5f; };
            const auto hold = LiveLooperEditor::kClearHoldMs;
            const int modalsBefore = juce::ModalComponentManager::getInstance()->getNumModalComponents();

            expect (editor->keyPressed (key (juce::KeyPress::backspaceKey, true)));
            expect (editor->isKeyClearAllPending());
            expectEquals (editor->getPendingKeyClearTrack(), -1);
            expectEquals (juce::ModalComponentManager::getInstance()->getNumModalComponents(), modalsBefore);   // no question
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + hold / 2, true);
            for (int t = 0; t < kNumTracks; ++t)
                expect (editor->getLiveView()->getStopButton (t)->getKeyHoldProgress() > 0.3f);
            expect (! clearedAll());
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + hold / 2 + 50, false);   // tap / early release
            expect (! clearedAll());
            expect (! editor->isKeyClearPending());
            for (int t = 0; t < kNumTracks; ++t)
                expect (editor->getLiveView()->getStopButton (t)->getKeyHoldProgress() < 0.001f);
            editor->keyStateChanged (false);

            // Return (TAP) or Space right after a tap can no longer confirm anything
            expect (editor->keyPressed (key (juce::KeyPress::backspaceKey, true)));
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + 100, false);
            editor->keyStateChanged (false);
            expect (editor->keyPressed (key (juce::KeyPress::returnKey)));
            editor->keyStateChanged (false);
            expect (! clearedAll());

            // held: auto-repeat keeps going, past 1.5 s everything is cleared
            expect (editor->keyPressed (key (juce::KeyPress::backspaceKey, true)));
            const auto t0 = juce::Time::getMillisecondCounter();
            for (int i = 0; i < 30; ++i)
                expect (editor->keyPressed (key (juce::KeyPress::backspaceKey, true)));
            expect (! clearedAll());
            editor->updateKeyClearHold (t0 + hold + 20, true);
            expect (clearedAll());
            expect (! editor->isKeyClearPending());

            // MIDI / host clear is unchanged: the parameter clears at once
            p.getState().getParameter ("clearall")->setValueNotifyingHost (0.0f);
            p.pressTrigger ("t2_clear");
            expect (p.getState().getParameter ("t2_clear")->getValue() >= 0.5f);
        }

        beginTest ("Keyboard UNDO (v1.7.1): an undo that empties the track must be held 1.5 s, other undos stay instant");
        {
            LiveLooperProcessor p;
            p.setKeyboardControlAvailable (true);
            p.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* editor = dynamic_cast<LiveLooperEditor*> (ed.get());
            auto on = [&p] (const juce::String& id) { return p.getState().getParameter (id)->getValue() >= 0.5f; };
            auto reset = [&p] (const juce::String& id) { p.getState().getParameter (id)->setValueNotifyingHost (0.0f); };
            // the engine says: track 1's next undo takes back its first recording (it would empty the track)
            auto& st1 = const_cast<LooperEngine::TrackStatus&> (p.getEngine().getTrackStatus (0));
            st1.canUndo.store (true);
            st1.undoEmpties.store (true);
            auto& st2 = const_cast<LooperEngine::TrackStatus&> (p.getEngine().getTrackStatus (1));
            st2.canUndo.store (true);           // track 2: undo of an overdub
            st2.undoEmpties.store (false);
            const auto hold = LiveLooperEditor::kClearHoldMs;

            // A (undo track 1) tapped: nothing, the hold is cancelled
            expect (editor->keyPressed (key ('A')));
            expectEquals (editor->getPendingKeyUndoTrack(), 0);
            expectEquals (editor->getPendingKeyClearTrack(), -1);
            expect (! on ("t1_undo"));
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + hold / 2, true);
            // the red hold fill shows on the UNDO buttons (surface, Live, Performance), not on STOP
            expect (editor->getStrip (0).getUndoButton().getKeyHoldProgress() > 0.3f);
            expect (editor->getLiveView()->getUndoButton (0)->getKeyHoldProgress() > 0.3f);
            expect (editor->getPerformView()->getUndoButton (0)->getKeyHoldProgress() > 0.3f);
            expect (editor->getLiveView()->getStopButton (0)->getKeyHoldProgress() < 0.001f);
            expect (editor->getLiveView()->getUndoButton (1)->getKeyHoldProgress() < 0.001f);
            editor->updateKeyClearHold (juce::Time::getMillisecondCounter() + hold / 2 + 30, false);   // released early
            expect (! on ("t1_undo"));
            expect (! editor->isKeyClearPending());
            expect (editor->getLiveView()->getUndoButton (0)->getKeyHoldProgress() < 0.001f);
            editor->keyStateChanged (false);

            // held (the auto-repeat does not restart the hold): undone after 1.5 s
            expect (editor->keyPressed (key ('A')));
            const auto t0 = juce::Time::getMillisecondCounter();
            for (int i = 0; i < 30; ++i)
                expect (editor->keyPressed (key ('A')));
            expect (! on ("t1_undo"));
            editor->updateKeyClearHold (t0 + hold + 20, true);
            expect (on ("t1_undo"));
            expect (! editor->isKeyClearPending());
            editor->keyStateChanged (false);
            reset ("t1_undo");

            // a released key cancels at once (key-up event)
            expect (editor->keyPressed (key ('A')));
            editor->keyStateChanged (false);
            expect (! editor->isKeyClearPending());
            expect (! on ("t1_undo"));

            // S (undo track 2, an overdub): instant, nothing pending
            expect (editor->keyPressed (key ('S')));
            expect (on ("t2_undo"));
            expect (! editor->isKeyClearPending());
            editor->keyStateChanged (false);

            // Shift + A (redo track 1): instant, unchanged
            expect (editor->keyPressed (key ('A', true)));
            expect (on ("t1_redo"));
            expect (! editor->isKeyClearPending());
            editor->keyStateChanged (false);

            // MIDI / host / controller undo: the parameter undoes at once
            p.pressTrigger ("t1_undo");
            expect (on ("t1_undo"));
        }

        beginTest ("Views (v1.7.1): a round UNDO per card - click = undo, Shift + click = redo, MIDI-learnable");
        {
            LiveLooperProcessor p;
            p.prepareToPlay (48000.0, 512);
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* editor = dynamic_cast<LiveLooperEditor*> (ed.get());
            auto on = [&p] (const juce::String& id) { return p.getState().getParameter (id)->getValue() >= 0.5f; };
            for (int t = 0; t < kNumTracks; ++t)
            {
                expect (editor->getLiveView()->getUndoButton (t) != nullptr);
                expect (editor->getPerformView()->getUndoButton (t) != nullptr);
                expectEquals (getParamId (*editor->getLiveView()->getUndoButton (t)), juce::String ("t") + juce::String (t + 1) + "_undo");
                expectEquals (getParamId (*editor->getPerformView()->getUndoButton (t)), juce::String ("t") + juce::String (t + 1) + "_undo");
            }
            editor->getLiveView()->getUndoButton (2)->click (false);
            expect (on ("t3_undo"));
            expect (! on ("t3_redo"));
            editor->getPerformView()->getUndoButton (3)->click (true);
            expect (on ("t4_redo"));
            expect (! on ("t4_undo"));
        }

        beginTest ("Tempo can be typed in");
        {
            LiveLooperProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* editor = dynamic_cast<LiveLooperEditor*> (ed.get());
            editor->showTempoEntry();
            expect (editor->isTempoEntryVisible());
            editor->getTempoEntry().setText ("97,5");
            editor->getTempoEntry().onReturnKey();
            expect (! editor->isTempoEntryVisible());
            expectWithinAbsoluteError (plain (p, "tempo"), 97.5f, 0.051f);

            editor->showTempoEntry();
            editor->getTempoEntry().setText ("999");
            editor->getTempoEntry().onReturnKey();
            expectWithinAbsoluteError (plain (p, "tempo"), 300.0f, 0.051f);

            editor->showTempoEntry();
            editor->getTempoEntry().setText ("");
            editor->getTempoEntry().onReturnKey();
            expectWithinAbsoluteError (plain (p, "tempo"), 300.0f, 0.051f);   // empty = unchanged
        }

        beginTest ("Mapping window: click a function, press a key");
        {
            LiveLooperProcessor p;
            KeyMapEditor w (p);
            const int row = w.getActions().indexOf ("t2_oneshot");
            expect (row >= 0);
            w.startCapture (row);
            expectEquals (w.getCaptureRow(), row);
            expect (w.keyPressed (key ('1')));
            expectEquals (p.getKeyMap().getActionFor (key ('1')), juce::String ("t2_oneshot"));
            expectEquals (w.getCaptureRow(), -1);

            w.startCapture (row);
            expect (w.keyPressed (key (juce::KeyPress::escapeKey)));   // cancel keeps the key
            expectEquals (p.getKeyMap().getActionFor (key ('1')), juce::String ("t2_oneshot"));
            expect (! w.keyPressed (key ('2')));                       // not capturing
        }
    }
};

static KeyboardTests keyboardTests;
