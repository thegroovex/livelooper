#include "KeyMap.h"
#include "Localisation.h"
#include "Parameters.h"

namespace livelooper
{
using i18n::tr;

KeyMap::KeyMap()
{
    setDefaults();
}

juce::StringArray KeyMap::getAvailableActions()
{
    using namespace params;
    juce::StringArray a;
    for (int t = 0; t < kNumTracks; ++t)
        for (auto* n : { kRecPlay, kStop, kClear, kUndo, kRedo, kReverse, kOneShot, kLoopSync, kTrackFx })
            a.add (trackId (t, n));

    a.addArray ({ gAllStartStop, gAllStart, gAllStop, gClearAll, gTap,
                  actEnterTempo, actTempoUp, actTempoDown, actTempoUpFine, actTempoDownFine,
                  gRhythmOn, gRhythmPattern, gClickSound, gQuantize, gSync, gFollow });

    for (int pass = 0; pass < 2; ++pass)
        for (int s = 0; s < kNumFxSlots; ++s)
            a.add (fxId (pass == 0, s, kFxOn));
    return a;
}

juce::String KeyMap::describeAction (const juce::String& action)
{
    if (action == actTempoUp)       return tr ("Tempo +1 BPM");
    if (action == actTempoDown)     return tr ("Tempo -1 BPM");
    if (action == actTempoUpFine)   return tr ("Tempo +0.1 BPM");
    if (action == actTempoDownFine) return tr ("Tempo -0.1 BPM");
    if (action == actEnterTempo)    return tr ("Type in tempo (BPM)");
    if (action == params::gRhythmPattern || action == params::gQuantize)
        return tr ("%1: next value", params::localisedName (action));
    return params::localisedName (action);
}

juce::KeyPress KeyMap::normalise (const juce::KeyPress& key)
{
    int code = key.getKeyCode();
    if (code >= 'a' && code <= 'z')
        code = code - 'a' + 'A';
    const juce::ModifierKeys mods (key.getModifiers().getRawFlags() & juce::ModifierKeys::allKeyboardModifiers);
    return juce::KeyPress (code, mods, 0);
}

juce::String KeyMap::describeKey (const juce::KeyPress& keyIn)
{
    if (! keyIn.isValid())
        return {};
    const auto key = normalise (keyIn);
    const auto mods = key.getModifiers();

    juce::StringArray parts;
   #if JUCE_MAC
    if (mods.isCtrlDown())    parts.add ("Ctrl");
    if (mods.isAltDown())     parts.add ("Option");
    if (mods.isShiftDown())   parts.add ("Shift");
    if (mods.isCommandDown()) parts.add ("Cmd");
   #else
    if (mods.isCtrlDown())    parts.add (tr ("Ctrl"));
    if (mods.isAltDown())     parts.add ("Alt");
    if (mods.isShiftDown())   parts.add ("Shift");
   #endif

    const int code = key.getKeyCode();
    juce::String name;
    if      (code == juce::KeyPress::spaceKey)     name = tr ("Space");
    else if (code == juce::KeyPress::returnKey)    name = tr ("Return");
    else if (code == juce::KeyPress::escapeKey)    name = "Esc";
    else if (code == juce::KeyPress::backspaceKey) name = tr ("Backspace");
    else if (code == juce::KeyPress::deleteKey)    name = tr ("Delete");
    else if (code == juce::KeyPress::tabKey)       name = "Tab";
    else if (code == juce::KeyPress::upKey)        name = tr ("Arrow Up");
    else if (code == juce::KeyPress::downKey)      name = tr ("Arrow Down");
    else if (code == juce::KeyPress::leftKey)      name = tr ("Arrow Left");
    else if (code == juce::KeyPress::rightKey)     name = tr ("Arrow Right");
    else if (code == juce::KeyPress::homeKey)      name = "Home";
    else if (code == juce::KeyPress::endKey)       name = "End";
    else if (code == juce::KeyPress::pageUpKey)    name = "Page Up";
    else if (code == juce::KeyPress::pageDownKey)  name = "Page Down";
    else if (code > 32 && code < 0x10000 && juce::CharacterFunctions::isPrintable ((juce::juce_wchar) code))
        name = juce::String::charToString ((juce::juce_wchar) code).toUpperCase();
    else
        name = juce::KeyPress (code).getTextDescription();

    parts.add (name);
    return parts.joinIntoString (" + ");
}

void KeyMap::clear()
{
    const juce::ScopedLock sl (lock);
    keys.clear();
}

void KeyMap::setDefaults()
{
    using namespace params;
    clear();
    const juce::ModifierKeys none, shift (juce::ModifierKeys::shiftModifier);
    const char rec[]  = { '1', '2', '3', '4', '5' };
    const char stop[] = { 'Q', 'W', 'E', 'R', 'T' };
    const char undo[] = { 'A', 'S', 'D', 'F', 'G' };
    for (int t = 0; t < kNumTracks; ++t)
    {
        assign (juce::KeyPress (rec[t], none, 0), trackId (t, kRecPlay));
        assign (juce::KeyPress (stop[t], none, 0), trackId (t, kStop));
        assign (juce::KeyPress (stop[t], shift, 0), trackId (t, kClear));
        assign (juce::KeyPress (undo[t], none, 0), trackId (t, kUndo));
        assign (juce::KeyPress (undo[t], shift, 0), trackId (t, kRedo));
    }
    assign (juce::KeyPress (juce::KeyPress::spaceKey, none, 0), gAllStartStop);
    assign (juce::KeyPress (juce::KeyPress::backspaceKey, shift, 0), gClearAll);
    assign (juce::KeyPress (juce::KeyPress::returnKey, none, 0), gTap);
    assign (juce::KeyPress ('B', none, 0), actEnterTempo);
    assign (juce::KeyPress (juce::KeyPress::upKey, none, 0), actTempoUp);
    assign (juce::KeyPress (juce::KeyPress::downKey, none, 0), actTempoDown);
    assign (juce::KeyPress (juce::KeyPress::upKey, shift, 0), actTempoUpFine);
    assign (juce::KeyPress (juce::KeyPress::downKey, shift, 0), actTempoDownFine);
    assign (juce::KeyPress ('M', none, 0), gRhythmOn);
    assign (juce::KeyPress ('N', none, 0), gRhythmPattern);
    assign (juce::KeyPress ('K', none, 0), gQuantize);
}

void KeyMap::assign (const juce::KeyPress& keyIn, const juce::String& action)
{
    const auto key = normalise (keyIn);
    if (! key.isValid() || action.isEmpty())
        return;
    const juce::ScopedLock sl (lock);
    for (auto it = keys.begin(); it != keys.end();)
        it = (it->second == key) ? keys.erase (it) : std::next (it);
    keys[action] = key;
}

void KeyMap::removeAction (const juce::String& action)
{
    const juce::ScopedLock sl (lock);
    keys.erase (action);
}

juce::String KeyMap::getActionFor (const juce::KeyPress& keyIn) const
{
    const auto key = normalise (keyIn);
    const juce::ScopedLock sl (lock);
    for (const auto& [action, k] : keys)
        if (k == key)
            return action;
    return {};
}

juce::KeyPress KeyMap::getKeyFor (const juce::String& action) const
{
    const juce::ScopedLock sl (lock);
    const auto it = keys.find (action);
    return it != keys.end() ? it->second : juce::KeyPress();
}

int KeyMap::getNumAssignments() const
{
    const juce::ScopedLock sl (lock);
    return (int) keys.size();
}

std::unique_ptr<juce::XmlElement> KeyMap::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("KEYMAP");
    xml->setAttribute ("enabled", enabled);
    const juce::ScopedLock sl (lock);
    for (const auto& [action, key] : keys)
    {
        auto* e = xml->createNewChildElement ("KEY");
        e->setAttribute ("action", action);
        e->setAttribute ("key", key.getTextDescription());
    }
    return xml;
}

void KeyMap::fromXml (const juce::XmlElement& xml)
{
    enabled = xml.getBoolAttribute ("enabled", true);
    const auto available = getAvailableActions();
    clear();
    for (auto* e : xml.getChildWithTagNameIterator ("KEY"))
    {
        const auto action = e->getStringAttribute ("action");
        const auto key = juce::KeyPress::createFromDescription (e->getStringAttribute ("key"));
        if (available.contains (action) && key.isValid())
            assign (key, action);
    }
}
} // namespace livelooper
