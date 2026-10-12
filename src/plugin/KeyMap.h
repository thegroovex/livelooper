#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

namespace livelooper
{
/**
    Computer-keyboard control (standalone app): maps keys to livelooper functions.

    An action is either a parameter id ("t1_recplay", "rhythm_on", "quantize") or one of the
    special actions below. Every key controls at most one action and every action has at most
    one key. Triggers are pressed, on/off parameters toggle, choice parameters step to the next
    value.
*/
class KeyMap
{
public:
    static inline const juce::String actTempoUp       { "@tempo_up" };
    static inline const juce::String actTempoDown     { "@tempo_down" };
    static inline const juce::String actTempoUpFine   { "@tempo_up_fine" };
    static inline const juce::String actTempoDownFine { "@tempo_down_fine" };
    static inline const juce::String actEnterTempo    { "@enter_tempo" };

    KeyMap();

    /** Every action a key can be given, in the order shown in the keyboard mapping window. */
    static juce::StringArray getAvailableActions();
    static bool isSpecialAction (const juce::String& action) { return action.startsWithChar ('@'); }
    /** True for actions that may repeat while a key is held down (tempo +/-). */
    static bool isRepeatingAction (const juce::String& action) { return action.startsWith ("@tempo_"); }

    /** Name of an action in the current UI language ("T1 Rec/Play", "Tempo +1 BPM"). */
    static juce::String describeAction (const juce::String& action);
    /** Name of a key in the current UI language ("Shift + Q", "Leertaste"). */
    static juce::String describeKey (const juce::KeyPress& key);

    /** Only the keyboard modifiers count (Shift, Ctrl, Alt/Option, Cmd); letters are upper case. */
    static juce::KeyPress normalise (const juce::KeyPress& key);

    void setDefaults();
    void clear();
    /** Gives the key to the action; any other action using that key loses it. */
    void assign (const juce::KeyPress& key, const juce::String& action);
    void removeAction (const juce::String& action);

    juce::String getActionFor (const juce::KeyPress& key) const;   // empty if none
    juce::KeyPress getKeyFor (const juce::String& action) const;   // invalid if none
    int getNumAssignments() const;

    bool isEnabled() const noexcept { return enabled; }
    void setEnabled (bool e) noexcept { enabled = e; }

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

private:
    mutable juce::CriticalSection lock;
    std::map<juce::String, juce::KeyPress> keys;   // action -> key
    bool enabled = true;
};
} // namespace livelooper
