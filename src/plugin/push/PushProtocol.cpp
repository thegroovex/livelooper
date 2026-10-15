#include "PushProtocol.h"

namespace livelooper::push
{
namespace
{
    constexpr juce::uint8 kHeader[] = { 0x47, 0x7f, 0x15 };   // Ableton, all devices, Push 1
}

juce::MidiMessage displayLine (int line, const juce::String& text)
{
    juce::uint8 data[3 + 4 + kDisplayChars];
    std::memcpy (data, kHeader, 3);
    data[3] = (juce::uint8) (0x18 + juce::jlimit (0, kDisplayLines - 1, line));
    data[4] = 0x00;
    data[5] = 0x45;        // 69 bytes follow: offset + 68 characters
    data[6] = 0x00;        // start at character 0
    for (int i = 0; i < kDisplayChars; ++i)
    {
        const auto c = i < text.length() ? text[i] : (juce::juce_wchar) ' ';
        juce::uint8 b = '?';
        if (c >= 1 && c < 128)
            b = (juce::uint8) c;
        else
            switch (c)          // the Push 1 character set has the German umlauts at 10 .. 26
            {
                case 0xc4: b = 10; break;   // Ä
                case 0xd6: b = 12; break;   // Ö
                case 0xdc: b = 13; break;   // Ü
                case 0xdf: b = 14; break;   // ß
                case 0xe4: b = 16; break;   // ä
                case 0xf6: b = 23; break;   // ö
                case 0xfc: b = 26; break;   // ü
                default: break;
            }
        data[7 + i] = b;
    }
    return juce::MidiMessage::createSysExMessage (data, (int) sizeof (data));
}

juce::String levelBar (float value, int cells)
{
    const int steps = juce::jlimit (0, cells * 2, juce::roundToInt (value * (float) (cells * 2)));
    juce::String s;
    for (int i = 0; i < cells; ++i)
        s += juce::String::charToString (steps >= 2 * (i + 1) ? glyph::block : (steps == 2 * i + 1 ? glyph::barLeft : glyph::dash));
    return s;
}

juce::MidiMessage clearDisplayLine (int line)
{
    const juce::uint8 data[] = { kHeader[0], kHeader[1], kHeader[2],
                                 (juce::uint8) (0x1c + juce::jlimit (0, kDisplayLines - 1, line)), 0x00, 0x00 };
    return juce::MidiMessage::createSysExMessage (data, (int) sizeof (data));
}

juce::MidiMessage userModeSysex (bool userMode)
{
    const juce::uint8 data[] = { kHeader[0], kHeader[1], kHeader[2], 0x62, 0x00, 0x01, (juce::uint8) (userMode ? 1 : 0) };
    return juce::MidiMessage::createSysExMessage (data, (int) sizeof (data));
}

juce::MidiMessage aftertouchModeSysex (bool polyphonic)
{
    const juce::uint8 data[] = { kHeader[0], kHeader[1], kHeader[2], 0x5c, 0x00, 0x01, (juce::uint8) (polyphonic ? 1 : 0) };
    return juce::MidiMessage::createSysExMessage (data, (int) sizeof (data));
}

juce::String columns (const juce::StringArray& texts)
{
    // eight columns of 8 or 9 characters (17 per display segment), text centred
    juce::String line;
    for (int i = 0; i < 8; ++i)
    {
        const int width = (i % 2 == 0) ? 8 : 9;
        auto t = i < texts.size() ? texts[i].substring (0, width - 1) : juce::String();
        const int pad = width - t.length();
        line << juce::String::repeatedString (" ", pad / 2) << t << juce::String::repeatedString (" ", pad - pad / 2);
    }
    return line;
}

bool isPushDevice (const juce::String& name)
{
    return name.containsIgnoreCase ("Ableton Push");
}

bool isPushUserPort (const juce::String& name, int indexAmongPushPorts)
{
    if (! isPushDevice (name))
        return false;
    if (name.containsIgnoreCase ("User Port"))                                  // macOS
        return true;
    if (name.containsIgnoreCase ("Live Port"))
        return false;
    if (name.containsIgnoreCase ("MIDIIN2") || name.containsIgnoreCase ("MIDIOUT2"))   // Windows
        return true;
    if (name.containsIgnoreCase ("MIDI 2"))                                     // Linux (ALSA)
        return true;
    if (name.containsIgnoreCase ("MIDI 1"))
        return false;
    return indexAmongPushPorts == 1;                                           // unknown naming: 2nd port
}
} // namespace livelooper::push
