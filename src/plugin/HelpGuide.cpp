// v1.6 in-app help: illustrated guide pages (Apple-style: short sentences, big pictures)
// shown above the full user guide in the help window. Pictures: resources/help/*.png,
// drawn by tools/illustrations/make_illustrations.py (numbered badges = legends below).

#include "InfoWindows.h"
#include "Localisation.h"

namespace livelooper
{
using i18n::tr;

namespace
{
    struct GuideDef
    {
        const char* title;
        const char* image;
        std::vector<const char*> lines;
        std::vector<const char*> legend;
    };

    const std::vector<GuideDef>& guideDefs()
    {
        static const std::vector<GuideDef> defs = {
            { "Welcome", "welcome-hero.png",
              { "livelooper is a loop station you play like an instrument.",
                "Record a phrase - it repeats as a loop. Add layers on top.",
                "Shape the loops with FX, a drum groove and scenes.",
                "Every control reacts at once and lands on the beat." }, {} },
            { "Loops", "help-loops.png",
              { "Press the big pad of a track to record.",
                "Press it again: the loop plays. Once more: record a new layer (overdub).",
                "STOP stops the track. Hold STOP to clear it.",
                "Drop audio files on a track to import them. Drag the waveform out to export it." }, {} },
            { "FX", "help-fx.png",
              { "INPUT FX shape what you play in. TRACK FX shape the loops with FX on.",
                "Switch a slot A - D on and click its name to edit it.",
                "BEAT FX, BEAT REPEAT / STUTTER and COLOR FX are made for quick live moves.",
                "PUNCH FX: hold one of 12 quick effects - several at once combine. SEND FX adds echo, reverb and more to the loops.",
                "Every FX switch and knob can be mapped to MIDI." }, {} },
            { "BEATS", "help-beats.png",
              { "BEATS opens the drum grid: eight rows, up to four bars.",
                "Click steps to build a groove. Right-click a step for softer or harder hits.",
                "Choose kit, pattern, length and swing at the top.",
                "SAVE keeps your pattern. Drop your own sample on a row." }, {} },
            { "Recording", "help-rec.png",
              { "The round REC button records everything livelooper plays as a WAV file.",
                "It blinks with the beat while it records. Click again to stop.",
                "SETUP shows the recordings folder and turns a recording into a 9:16 video clip." }, {} },
            { "Link", "help-link.png",
              { "Ableton Link keeps tempo and beat in time with other apps and devices on your network.",
                "Switch it on in SETUP > Sync. The display shows how many others are connected.",
                "In a DAW, HOST SYNC comes first - switch it off to follow Link." }, {} },
            // v1.7 pages (pictures: real v1.7 screenshots, tools/illustrations/make_help17.py)
            { "Loop length", "help-looplen.png",
              { "Every track has a loop length pill next to its name: AUTO or 1 - 16 bars.",
                "AUTO: your next press ends the first recording. 1 - 16 BARS: the recording ends by itself on the bar line.",
                "Click the pill to choose, scroll or drag to change it, right-click for MIDI learn.",
                "Dimmed while the track holds a loop: the length applies to the next new recording (after CLEAR).",
                "Push 1: hold FIXED LENGTH and press a pad in rows 3 - 8 (AUTO - 16 bars) or turn the track encoder. FIXED LENGTH alone sets the capture length.",
                "Just once (up to 32 bars): Loop Length Once - on Push hold the REC pad and press a position pad." }, {} },
            { "Live view", "help-live.png",
              { "LIVE next to the logo opens the Live view: one large card per track, made for the stage.",
                "Each card: state, length, waveform, the big REC / PLAY / DUB pad, level and STOP.",
                "The performance FX strip: filter, beat FX, punch, master, echo, repeat, stutter, fill, ALL START / STOP and capture.",
                "Also with SHIFT + MASTER on Push 1 or any MIDI control (host parameter Live View). Esc or EXIT LIVE returns.",
                "SETUP > Display > Open Live view (fullscreen): a borderless window kept on top over the whole screen - handy on a second monitor." }, {} },
            { "Display", "help-display.png",
              { "SETUP > Display: dark or light theme, interface size 75 - 200 %, stage mode and waveforms.",
                "Ctrl + / Ctrl - changes the size (Mac: Cmd), Ctrl 0 = 100 %.",
                "Stage mode: high contrast and larger text for dark stages.",
                "Waveforms in the loop tracks are off by default - switch them on for large waveforms with the overdub layer." }, {} },
            { "Latency", "help-latency.png",
              { "Every audio interface delays what you hear and what you record. livelooper measures it and puts every recording back on the beat.",
                "SETUP > Audio > Latency > Measure latency: connect an output to an input with a cable, or hold a microphone to the speaker.",
                "The result is stored per audio device. Compensation is on by default; fine-tune it with the manual offset.",
                "In a DAW: measure inside the DAW, on the looper track with input monitoring on.",
                "Follow host transport is on by default: stopping your DAW stops the loops (SETUP > Looper)." }, {} },
            { "Controllers", "help-controllers.png",
              { "Velocity curves: soft, normal, hard or fixed - separately for Push 1, K.O. II and learned MIDI notes.",
                "MIDI feedback (experimental): learned controls light up - livelooper sends their state to a MIDI output you choose.",
                "Push 1 display: a loop progress bar per track and the amount bar of the held punch FX.",
                "Everything is in SETUP > Controllers. Every function can be MIDI-learned with a right-click." }, {} },
            { "Backup", "help-backup.png",
              { "livelooper saves the whole set in the background: every few minutes and after every recording - only when something changed.",
                "The audio keeps playing. The last 3 backups stay in the app data folder (SETUP > Sets > Auto backup > Open backup folder).",
                "App: the last session loads on start. After a crash the newest backup is restored - a notice lets you start empty instead.",
                "Plug-in: the DAW project comes first. After a crash a notice offers a newer backup - nothing is replaced without asking.",
                "SETUP > Sets > Auto backup: on / off, interval, auto load, restore a backup." }, {} },
            { "Push 1", "help-push1.png",
              { "Connect Push and press USER: livelooper takes over the pads, buttons and display.",
                "Each pad column is one track.",
                "Press a loop pad harder: that loop chops faster (1/4 to 1/32). Let go: normal playback.",
                "Double-touch an encoder: next page - volume, pan, pitch, feedback, track colour.",
                "SHIFT + RECORD: set recording. PLAY blinks in time.",
                "XY FX pad: SHIFT + top buttons pick beat FX, colour FX, repeat, stutter and the punch FX.",
                "SHIFT + MASTER: Live view on the computer. Hold FIXED LENGTH + pad or encoder: loop length of a track." },
              { "Bottom pad row: REC / PLAY / DUB",
                "Second pad row: STOP",
                "Pads 3 - 8: loop position - tap to jump, hold to stutter",
                "Top button row: track FX on / off",
                "Second button row: reverse - SHIFT: reverse the loop audio",
                "Touch strip: tape stop - SHIFT: filter",
                "Scene buttons: scenes 1 - 8 - SHIFT: store",
                "PLAY: all start / stop",
                "RECORD: capture the last bars as a loop",
                "UNDO - SHIFT: redo",
                "METRONOME: rhythm on / off - SHIFT: Link",
                "SCALES: STEP sequencer",
                "SHIFT: second function of a button",
                "REPEAT / ACCENT: beat repeat / stutter while held",
                "MUTE / SOLO + pad: mute or solo while held",
                "IN: fill - OUT: drums through the beat FX",
                "Encoders: level, pan, pitch, FX (mode buttons)",
                "Display: names, values, tempo and meters",
                "OCTAVE: pitch - SHIFT: half / double speed",
                "SESSION: loop mode - NOTE: XY FX pad",
                "Top-right pad (5 tracks): flashes on every beat",
                "Pad below it: set recording on / off - blinks while recording" } },
            { "Push 1: STEP", "help-push1-step.png",
              { "SCALES turns the pads into a step sequencer for the drums." },
              { "Pad rows are the drum voices: kick at the bottom, tom on top",
                "Columns are steps - the green / white column plays now",
                "Top buttons: row mute",
                "Lower buttons: row solo",
                "SCALES: STEP mode on / off",
                "LEFT / RIGHT: previous / next 8 steps",
                "IN: fill while held",
                "OUT: drums through the beat FX",
                "Encoders: row level - SHIFT: pitch",
                "SHIFT + pad: accent" } },
            { "K.O. II", "help-ko2.png",
              { "Plug in the K.O. II: livelooper finds it and asks once how to use it - as controller only or to record it.",
                "Its four pad groups are four pages: A loops, B track edit, C punch FX, D scenes.",
                "Set the K.O. II to clock in (SHIFT + ERASE, 1 0 1, ENTER): livelooper starts it on the bar.",
                "Record K.O. II: the K.O. II is the input, the output stays on the computer - metronome and loops are not recorded again.",
                "In the plug-in: the K.O. II as the track's audio input in the DAW, the DAW's output on the computer / interface, input monitoring off.",
                "The K.O. II sends no pad pressure: hold a punch pad (group C) and the amount rises to 100 % (SETUP > Controllers).",
                "Experimental - please test and report." },
              { "Group A pads 1 - 8: REC / PLAY / DUB track 1 - 8",
                "Pad 9: all start / stop - ENTER: capture",
                "'.': stop (hold: clear) - '0': undo (hold: redo)",
                "Group buttons A - D: the pages",
                "Group C: the 12 punch FX - hit harder for more",
                "Group D: '.' + ENTER held 1 s = Controller only / Record K.O. II",
                "Hatched: knobs, fader and function buttons send no MIDI - not used",
                "Display: shows nothing from livelooper - the screen mirrors it" } },
        };
        return defs;
    }

    juce::Font titleFont (float h)  { return LooperLookAndFeel::font (h, true); }
}

juce::Array<HelpComponent::Section> HelpComponent::guidePages()
{
    juce::Array<Section> out;
    for (const auto& d : guideDefs())
    {
        Section s;
        s.document = tr ("Guide");
        s.title = tr (d.title);
        s.level = 0;
        s.image = d.image;
        juce::StringArray lines;
        for (auto* l : d.lines)
            lines.add (tr (l));
        s.text = lines.joinIntoString ("\n");
        for (auto* l : d.legend)
            s.legend.add (tr (l));
        out.add (s);
    }
    return out;
}

//============================================================================== GuidePage
void GuidePage::setPage (const HelpComponent::Section& s)
{
    section = s;
    image = {};
    if (i18n::isGerman())          // a German variant of the picture (name-de.png), if there is one
        image = getEmbeddedImage (s.image.upToLastOccurrenceOf (".", false, false) + "-de.png");
    if (! image.isValid())
        image = getEmbeddedImage (s.image);
    setSize (getWidth(), getHeightForWidth (juce::jmax (300, getWidth())));
    repaint();
}

int GuidePage::getHeightForWidth (int w) const
{
    const int inner = w - 2 * kMargin;
    int y = kMargin + 44;                       // title
    if (image.isValid())
        y += juce::roundToInt ((float) inner * (float) image.getHeight() / (float) image.getWidth()) + 24;
    const auto f = LooperLookAndFeel::font (16.0f);
    for (const auto& line : juce::StringArray::fromLines (section.text))
    {
        juce::GlyphArrangement ga;
        ga.addJustifiedText (f, line, 0.0f, 0.0f, (float) inner - 18.0f, juce::Justification::topLeft, 1.2f);
        y += juce::roundToInt (ga.getBoundingBox (0, -1, true).getHeight()) + 10;
    }
    if (! section.legend.isEmpty())
        y += 18 + ((section.legend.size() + 1) / 2) * kLegendRow;
    return y + kMargin;
}

void GuidePage::paint (juce::Graphics& g)
{
    const int inner = getWidth() - 2 * kMargin;
    float y = (float) kMargin;
    g.setColour (juce::Colours::white);
    g.setFont (titleFont (30.0f));
    g.drawText (section.title, kMargin, (int) y, inner, 38, juce::Justification::centredLeft);
    y += 44.0f;

    if (image.isValid())
    {
        const float h = (float) inner * (float) image.getHeight() / (float) image.getWidth();
        const juce::Rectangle<float> r ((float) kMargin, y, (float) inner, h);
        // soft shadow + rounded clip (continuous-looking corners)
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.05f));
            g.fillRoundedRectangle (r.expanded ((float) i * 2.0f).translated (0.0f, 6.0f), 18.0f + (float) i * 2.0f);
        }
        juce::Path clip;
        clip.addRoundedRectangle (r, 18.0f);
        g.saveState();
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImage (image, r, juce::RectanglePlacement::stretchToFit);
        g.restoreState();
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.strokePath (clip, juce::PathStrokeType (1.0f));
        y += h + 24.0f;
    }

    const auto f = LooperLookAndFeel::font (16.0f);
    for (const auto& line : juce::StringArray::fromLines (section.text))
    {
        juce::GlyphArrangement ga;
        ga.addJustifiedText (f, line, (float) kMargin + 18.0f, y + f.getAscent(), (float) inner - 18.0f, juce::Justification::topLeft, 1.2f);
        const auto bb = ga.getBoundingBox (0, -1, true);
        juce::ColourGradient dot (juce::Colour (0xffff7a18), (float) kMargin, y, juce::Colour (0xffb14bff), (float) kMargin + 8.0f, y + 8.0f, false);
        g.setGradientFill (dot);
        g.fillEllipse ((float) kMargin + 1.0f, y + f.getAscent() * 0.5f - 1.0f, 7.0f, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.86f));
        ga.draw (g);
        y += bb.getHeight() + 10.0f;
    }

    if (! section.legend.isEmpty())
    {
        y += 18.0f;
        const int colW = inner / 2;
        const int rows = (section.legend.size() + 1) / 2;
        for (int i = 0; i < section.legend.size(); ++i)
        {
            const int col = i / rows, row = i % rows;
            const float x = (float) (kMargin + col * colW), ry = y + (float) (row * kLegendRow);
            const juce::Rectangle<float> badge (x, ry + 3.0f, 22.0f, 22.0f);
            juce::ColourGradient brand (juce::Colour (0xffff7a18), badge.getX(), badge.getY(), juce::Colour (0xffb14bff), badge.getRight(), badge.getBottom(), false);
            brand.addColour (0.5, juce::Colour (0xffff2d6f));
            g.setGradientFill (brand);
            g.fillEllipse (badge);
            g.setColour (juce::Colours::white);
            g.setFont (LooperLookAndFeel::font (11.5f, true));
            g.drawText (juce::String (i + 1), badge, juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.86f));
            g.setFont (LooperLookAndFeel::font (13.5f));
            g.drawFittedText (section.legend[i], juce::Rectangle<int> ((int) x + 30, (int) ry, colW - 40, kLegendRow - 2),
                              juce::Justification::centredLeft, 2, 0.85f);
        }
    }
}
} // namespace livelooper
