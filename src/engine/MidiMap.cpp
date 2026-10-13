#include "MidiMap.h"

namespace livelooper
{
MidiMap::MidiMap()
{
    for (auto& t : table)
        t.store (-1);
}

void MidiMap::setParamKinds (std::vector<ParamKind> k)
{
    kinds = std::move (k);
}

void MidiMap::assign (const Key& key, int paramIndex)
{
    removeParam (paramIndex);
    table[(size_t) slot (key)].store (paramIndex);
}

void MidiMap::removeParam (int paramIndex)
{
    for (auto& t : table)
        if (t.load() == paramIndex)
            t.store (-1);
}

void MidiMap::clear()
{
    for (auto& t : table)
        t.store (-1);
}

bool MidiMap::getKeyForParam (int paramIndex, Key& keyOut) const
{
    for (int s = 0; s < (int) table.size(); ++s)
        if (table[(size_t) s].load() == paramIndex)
        {
            keyOut.channel = s / 256;
            keyOut.isCC = (s % 256) >= 128;
            keyOut.number = s % 128;
            return true;
        }
    return false;
}

std::vector<std::pair<MidiMap::Key, int>> MidiMap::getAllMappings() const
{
    std::vector<std::pair<Key, int>> result;
    for (int s = 0; s < (int) table.size(); ++s)
    {
        const int p = table[(size_t) s].load();
        if (p >= 0)
            result.push_back ({ Key { s / 256, (s % 256) >= 128, s % 128 }, p });
    }
    return result;
}

MidiMap::Action MidiMap::handle (const juce::MidiMessage& m) noexcept
{
    Action a;
    const bool isNoteOn = m.isNoteOn() && m.getVelocity() > 0;
    const bool isCC = m.isController();
    if (m.isNoteOff (true) && learnTarget.load() < 0)
    {
        // note off only releases momentary parameters
        const int p = table[(size_t) slot ({ m.getChannel() - 1, false, m.getNoteNumber() })].load();
        if (p >= 0 && p < (int) kinds.size() && kinds[(size_t) p] == ParamKind::Momentary)
        {
            a.paramIndex = p;
            a.op = Action::Op::SetNormalized;
            a.value = 0.0f;
        }
        return a;
    }
    if (! isNoteOn && ! isCC)
        return a;

    Key key { m.getChannel() - 1, isCC, isCC ? m.getControllerNumber() : m.getNoteNumber() };
    const int ccValue = isCC ? m.getControllerValue() : 0;

    // CC edge tracking (also while learning, so the release of the learned switch is not a press)
    bool risingEdge = isNoteOn;
    if (isCC)
    {
        auto& high = ccHigh[(size_t) ((key.channel & 15) * 128 + key.number)];
        const bool nowHigh = ccValue >= 64;
        risingEdge = nowHigh && high == 0;
        high = nowHigh ? 1 : 0;
    }

    const int target = learnTarget.load();
    if (target >= 0)
    {
        if (isNoteOn || (isCC && ccValue > 0))
        {
            for (auto& t : table)
                if (t.load() == target)
                    t.store (-1);
            table[(size_t) slot (key)].store (target);
            learnTarget.store (-1);
            learned.store (target);
        }
        return a;
    }

    const int p = table[(size_t) slot (key)].load();
    if (p < 0 || p >= (int) kinds.size())
        return a;

    a.paramIndex = p;
    switch (kinds[(size_t) p])
    {
        case ParamKind::Trigger:
            if (risingEdge) a.op = Action::Op::Trigger;
            break;
        case ParamKind::Toggle:
            if (risingEdge) a.op = Action::Op::Toggle;
            break;
        case ParamKind::Choice:
            if (isNoteOn) a.op = Action::Op::StepChoice;
            else { a.op = Action::Op::SetNormalized; a.value = (float) ccValue / 127.0f; }
            break;
        case ParamKind::Momentary:
            if (isNoteOn || risingEdge) { a.op = Action::Op::SetNormalized; a.value = 1.0f; }
            else if (isCC && ccValue < 64) { a.op = Action::Op::SetNormalized; a.value = 0.0f; }
            break;
        case ParamKind::Continuous:
            a.op = Action::Op::SetNormalized;
            a.value = isNoteOn ? (float) m.getVelocity() / 127.0f : (float) ccValue / 127.0f;
            break;
    }
    return a;
}

std::unique_ptr<juce::XmlElement> MidiMap::toXml (const std::function<juce::String (int)>& idForIndex) const
{
    auto xml = std::make_unique<juce::XmlElement> ("MIDIMAP");
    for (const auto& [key, param] : getAllMappings())
    {
        auto* e = xml->createNewChildElement ("MAP");
        e->setAttribute ("param", idForIndex (param));
        e->setAttribute ("channel", key.channel + 1);
        e->setAttribute ("type", key.isCC ? "cc" : "note");
        e->setAttribute ("number", key.number);
    }
    return xml;
}

void MidiMap::fromXml (const juce::XmlElement& xml, const std::function<int (const juce::String&)>& indexForId)
{
    clear();
    for (auto* e : xml.getChildWithTagNameIterator ("MAP"))
    {
        const int p = indexForId (e->getStringAttribute ("param"));
        if (p < 0)
            continue;
        Key k { juce::jlimit (0, 15, e->getIntAttribute ("channel", 1) - 1),
                e->getStringAttribute ("type") == "cc",
                juce::jlimit (0, 127, e->getIntAttribute ("number")) };
        assign (k, p);
    }
}

juce::String MidiMap::describe (const Key& key)
{
    if (key.isCC)
        return "CC " + juce::String (key.number) + " / Ch " + juce::String (key.channel + 1);
    return juce::MidiMessage::getMidiNoteName (key.number, true, true, 3)
           + " (" + juce::String (key.number) + ") / Ch " + juce::String (key.channel + 1);
}
} // namespace livelooper
