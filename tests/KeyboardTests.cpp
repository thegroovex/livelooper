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
