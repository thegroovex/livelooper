#include "MidiFeedback.h"

namespace livelooper
{
const int MidiFeedback::kOnValues[4] = { 127, 100, 64, 1 };
const int MidiFeedback::kOffValues[3] = { 0, 1, 7 };
const int MidiFeedback::kBlinkValues[4] = { -1, 2, 3, 64 };

int MidiFeedback::valueFor (const Item& item, bool blinkPhaseOn) const noexcept
{
    if (item.value >= 0)
        return juce::jlimit (0, 127, item.value);
    switch (item.led)
    {
        case Led::On:    return juce::jlimit (0, 127, profile.onValue);
        case Led::Blink: return profile.softBlink ? juce::jlimit (0, 127, blinkPhaseOn ? profile.onValue : profile.offValue)
                                                  : juce::jlimit (0, 127, profile.blinkValue);
        case Led::Off:   break;
    }
    return juce::jlimit (0, 127, profile.offValue);
}

juce::MidiMessage MidiFeedback::message (const MidiMap::Key& key, int value)
{
    const int ch = (key.channel & 15) + 1;
    if (key.isCC)
        return juce::MidiMessage::controllerEvent (ch, key.number & 127, juce::jlimit (0, 127, value));
    // note-on with the value as velocity (velocity 0 = off for most controllers)
    return juce::MidiMessage::noteOn (ch, key.number & 127, (juce::uint8) juce::jlimit (0, 127, value));
}

void MidiFeedback::render (const std::vector<Item>& items, bool blinkPhaseOn, std::vector<juce::MidiMessage>& out, bool full)
{
    for (const auto& item : items)
    {
        const int slot = (item.key.channel & 15) * 256 + (item.key.isCC ? 128 : 0) + (item.key.number & 127);
        const int v = valueFor (item, blinkPhaseOn);
        auto it = sent.find (slot);
        if (! full && it != sent.end() && it->second == v)
            continue;
        sent[slot] = v;
        out.push_back (message (item.key, v));
    }
}
} // namespace livelooper
