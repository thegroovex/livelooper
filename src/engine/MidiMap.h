#pragma once

#include "Types.h"

#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>

namespace livelooper
{
/**
    MIDI-learn table: maps a MIDI note or CC (per channel) to a parameter index.

    The lookup table is lock-free (atomics), so the audio thread reads it while the UI
    edits it. Each parameter has at most one controller, each controller drives at most
    one parameter.

    How a message drives a parameter depends on the parameter kind:
      Trigger    (Rec/Play, Stop, Undo ...)  note-on, or CC rising above 63
      Toggle     (Reverse, FX on ...)        note-on or CC rising edge flips the value
      Choice     (FX type, quantize ...)     note-on steps to the next choice, CC selects directly
      Continuous (level, pan, FX knobs ...)  CC sets the value, note-on sets velocity/127
*/
class MidiMap
{
public:
    enum class ParamKind : uint8_t { Trigger = 0, Toggle, Choice, Continuous };

    struct Key
    {
        int channel = 0;      // 0..15
        bool isCC = false;
        int number = 0;       // 0..127
        bool operator== (const Key& o) const noexcept { return channel == o.channel && isCC == o.isCC && number == o.number; }
    };

    struct Action
    {
        enum class Op { None = 0, Trigger, Toggle, StepChoice, SetNormalized };
        Op op = Op::None;
        int paramIndex = -1;
        float value = 0.0f;
    };

    MidiMap();

    void setParamKinds (std::vector<ParamKind> kinds);
    int  getNumParams() const noexcept { return (int) kinds.size(); }

    // ---- editing (message thread)
    void assign (const Key& key, int paramIndex);
    void removeParam (int paramIndex);
    void clear();
    bool getKeyForParam (int paramIndex, Key& keyOut) const;
    std::vector<std::pair<Key, int>> getAllMappings() const;

    /** Arms MIDI learn: the next note-on / CC is assigned to this parameter (-1 cancels). */
    void startLearn (int paramIndex) noexcept   { learnTarget.store (paramIndex); }
    int  getLearnTarget() const noexcept         { return learnTarget.load(); }
    /** Returns the parameter that was learned since the last call (or -1). */
    int  popLearnedParam() noexcept              { return learned.exchange (-1); }

    // ---- audio thread
    /** Translates one MIDI message into a parameter action (or Op::None). */
    Action handle (const juce::MidiMessage& m) noexcept;

    // ---- persistence (parameter ids instead of indices, so the mapping survives updates)
    std::unique_ptr<juce::XmlElement> toXml (const std::function<juce::String (int)>& idForIndex) const;
    void fromXml (const juce::XmlElement& xml, const std::function<int (const juce::String&)>& indexForId);

    static juce::String describe (const Key& key);

private:
    static int slot (const Key& k) noexcept { return (k.channel & 15) * 256 + (k.isCC ? 128 : 0) + (k.number & 127); }

    std::array<std::atomic<int>, 16 * 256> table;
    std::vector<ParamKind> kinds;
    std::array<uint8_t, 16 * 128> ccHigh {};
    std::atomic<int> learnTarget { -1 };
    std::atomic<int> learned { -1 };
};
} // namespace livelooper
