#pragma once

#include "engine/MidiMap.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <map>
#include <vector>

namespace livelooper
{
/**
    v1.7 generic MIDI feedback profile (experimental): for every MIDI-learned control livelooper sends
    the state back to a chosen MIDI output - a note-on / CC with the same channel and number, using
    configurable values for on, off and blink (most controllers light, dim or flash an LED by value).
    Continuous parameters learned on a CC send their value (0 .. 127, LED rings / motor faders).

    Plain data: render() produces only the messages that changed since the last call.
*/
class MidiFeedback
{
public:
    enum class Led { Off = 0, On, Blink };

    struct Profile
    {
        int onValue = 127, offValue = 0, blinkValue = 2;
        bool softBlink = false;     // true: livelooper blinks (on / off with the beat) instead of sending blinkValue
        bool operator== (const Profile& o) const noexcept
        {
            return onValue == o.onValue && offValue == o.offValue && blinkValue == o.blinkValue && softBlink == o.softBlink;
        }
    };

    struct Item
    {
        MidiMap::Key key;
        Led led = Led::Off;
        int value = -1;             // >= 0: continuous value 0 .. 127 (sent as is), otherwise the LED state
    };

    void setProfile (const Profile& p)  { if (! (p == profile)) { profile = p; sent.clear(); } }
    const Profile& getProfile() const noexcept { return profile; }

    /** The value an item sends with the current profile. */
    int valueFor (const Item& item, bool blinkPhaseOn) const noexcept;
    /** Appends the messages whose value changed (full = all of them). */
    void render (const std::vector<Item>& items, bool blinkPhaseOn, std::vector<juce::MidiMessage>& out, bool full);
    /** Forget what was sent (new output device): the next render sends everything. */
    void invalidate() { sent.clear(); }

    static juce::MidiMessage message (const MidiMap::Key& key, int value);

    /** Choices offered in SETUP (values a controller commonly understands). */
    static const int kOnValues[4];
    static const int kOffValues[3];
    static const int kBlinkValues[4];   // -1 = soft blink (livelooper flashes the LED itself)

private:
    Profile profile;
    std::map<int, int> sent;    // key slot -> last value sent
};
} // namespace livelooper
