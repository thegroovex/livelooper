// Tests of the MIDI learn table: learning, the four parameter kinds, CC edge
// detection and saving / loading the mapping.

#include "engine/MidiMap.h"

using namespace livelooper;
using Kind = MidiMap::ParamKind;
using Op = MidiMap::Action::Op;

class MidiMapTests : public juce::UnitTest
{
public:
    MidiMapTests() : juce::UnitTest ("MIDI Map", "midi") {}

    void runTest() override
    {
        auto makeMap = []
        {
            auto m = std::make_unique<MidiMap>();
            m->setParamKinds ({ Kind::Trigger, Kind::Toggle, Kind::Choice, Kind::Continuous });
            return m;
        };

        beginTest ("MIDI learn assigns the next note or CC");
        {
            auto map = makeMap();
            map->startLearn (0);
            auto a = map->handle (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100));
            expect (a.op == Op::None, "learning consumes the message");
            expectEquals (map->popLearnedParam(), 0);
            expectEquals (map->getLearnTarget(), -1);

            a = map->handle (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100));
            expect (a.op == Op::Trigger);
            expectEquals (a.paramIndex, 0);

            a = map->handle (juce::MidiMessage::noteOn (2, 36, (juce::uint8) 100));
            expect (a.op == Op::None, "other channel is not mapped");
        }

        beginTest ("Triggers fire on note-on and on rising CC edges only");
        {
            auto map = makeMap();
            map->assign ({ 0, true, 64 }, 0);
            expect (map->handle (juce::MidiMessage::controllerEvent (1, 64, 127)).op == Op::Trigger);
            expect (map->handle (juce::MidiMessage::controllerEvent (1, 64, 127)).op == Op::None, "held: no repeat");
            expect (map->handle (juce::MidiMessage::controllerEvent (1, 64, 0)).op == Op::None, "release: nothing");
            expect (map->handle (juce::MidiMessage::controllerEvent (1, 64, 100)).op == Op::Trigger);
            map->assign ({ 0, false, 40 }, 0);
            expect (map->handle (juce::MidiMessage::noteOff (1, 40)).op == Op::None);
            expect (map->handle (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 1)).op == Op::Trigger);
        }

        beginTest ("Toggles flip, choices step or select, continuous follows the CC");
        {
            auto map = makeMap();
            map->assign ({ 0, false, 50 }, 1);
            map->assign ({ 0, false, 51 }, 2);
            map->assign ({ 0, true, 7 }, 2 + 1);
            expect (map->handle (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 90)).op == Op::Toggle);
            expect (map->handle (juce::MidiMessage::noteOn (1, 51, (juce::uint8) 90)).op == Op::StepChoice);
            auto a = map->handle (juce::MidiMessage::controllerEvent (1, 7, 64));
            expect (a.op == Op::SetNormalized);
            expectWithinAbsoluteError (a.value, 64.0f / 127.0f, 1.0e-6f);
        }

        beginTest ("One controller per parameter and one parameter per controller");
        {
            auto map = makeMap();
            map->assign ({ 0, true, 1 }, 3);
            map->assign ({ 0, true, 2 }, 3);
            expectEquals ((int) map->getAllMappings().size(), 1);
            MidiMap::Key k;
            expect (map->getKeyForParam (3, k) && k.number == 2);
            map->assign ({ 0, true, 2 }, 1);
            expect (! map->getKeyForParam (3, k), "controller moved to the other parameter");
            map->removeParam (1);
            expect (map->getAllMappings().empty());
        }

        beginTest ("Mapping is saved by parameter id and restored");
        {
            auto map = makeMap();
            map->assign ({ 3, false, 60 }, 0);
            map->assign ({ 0, true, 21 }, 3);
            const juce::StringArray ids { "t1_recplay", "t1_reverse", "quantize", "t1_level" };
            auto xml = map->toXml ([&] (int i) { return ids[i]; });

            auto restored = makeMap();
            restored->fromXml (*xml, [&] (const juce::String& id) { return ids.indexOf (id); });
            expectEquals ((int) restored->getAllMappings().size(), 2);
            MidiMap::Key k;
            expect (restored->getKeyForParam (0, k) && k.channel == 3 && ! k.isCC && k.number == 60);
            expect (restored->getKeyForParam (3, k) && k.isCC && k.number == 21);
        }
    }
};

static MidiMapTests midiMapTests;
