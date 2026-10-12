#include "Localisation.h"

#include <juce_data_structures/juce_data_structures.h>

#include <atomic>
#include <map>

namespace livelooper::i18n
{
namespace
{
    // English UI text -> German. Every visible text of livelooper goes through tr().
    // Keep the English side identical to the text used in the code.
    const char* const kGerman[][2] = {
        // ---------------------------------------------------------------- panel
        { "5 TRACK LOOP STATION", "5-SPUR LOOP STATION" },
        { "INPUT FX", "EINGANGS-FX" },
        { "TRACK FX", "SPUR-FX" },
        { "EDIT", "BEARBEITEN" },
        { "RHYTHM", "RHYTHMUS" },
        { "MASTER", "MASTER" },
        { "Right-click any control for MIDI learn   |   hold STOP to clear   |   drop audio files on a track to import",
          "Rechtsklick auf ein Bedienelement: MIDI Learn   |   STOP halten: Spur l\xc3\xb6schen   |   Audiodateien auf eine Spur ziehen: Import" },

        // ---------------------------------------------------------------- buttons
        { "MIDI LEARN", "MIDI LEARN" },
        { "MIDI MAP", "MIDI MAP" },
        { "SETUP", "SETUP" },
        { "ALL START / STOP", "ALLE START / STOP" },
        { "CLEAR ALL", "ALLE L\xc3\x96SCHEN" },
        { "TAP", "TAP" },
        { "HOST SYNC", "HOST-SYNC" },
        { "REV", "R\xc3\x9c" "CKW." },
        { "1SHOT", "1SHOT" },
        { "FX", "FX" },
        { "UNDO", "ZUR\xc3\x9c" "CK" },
        { "REDO", "VOR" },
        { "LOOP SYNC", "LOOP-SYNC" },
        { "CLEAR", "L\xc3\x96SCHEN" },
        { "EXPORT WAV", "WAV EXPORT" },
        { "IMPORT", "IMPORT" },

        // ---------------------------------------------------------------- knobs / captions
        { "P1", "P1" },
        { "P2", "P2" },
        { "Mix", "Mix" },
        { "Level", "Pegel" },
        { "Tempo", "Tempo" },
        { "Master", "Master" },
        { "Input", "Eingang" },
        { "Fade", "Fade" },
        { "PAN", "PAN" },
        { "Start", "Start" },
        { "Stop", "Stop" },
        { "Dub mode", "Dub-Modus" },
        { "Measures", "Takte" },

        // ---------------------------------------------------------------- effects
        { "LPF", "Tiefpass" },
        { "HPF", "Hochpass" },
        { "BPF", "Bandpass" },
        { "Phaser", "Phaser" },
        { "Flanger", "Flanger" },
        { "Delay", "Delay" },
        { "Reverb", "Hall" },
        { "Slicer", "Slicer" },
        { "Tremolo", "Tremolo" },
        { "Auto Pan", "Auto-Pan" },
        { "Lo-Fi", "Lo-Fi" },
        { "Overdrive", "Overdrive" },
        { "Cutoff", "Cutoff" },
        { "Depth", "Tiefe" },
        { "Feedback", "Feedback" },
        { "Size", "Gr\xc3\xb6\xc3\x9f" "e" },
        { "Bits", "Bits" },
        { "Drive", "Drive" },
        { "Reso", "Reso" },
        { "Rate", "Rate" },
        { "Time", "Zeit" },
        { "Damp", "D\xc3\xa4mpfung" },
        { "Pattern", "Muster" },
        { "Tone", "Klang" },

        // ---------------------------------------------------------------- rhythm patterns
        { "Metronome", "Metronom" },
        { "Half Time", "Half Time" },

        // ---------------------------------------------------------------- choices
        { "Off", "Aus" },
        { "Beat", "Schlag" },
        { "Measure", "Takt" },
        { "Stereo", "Stereo" },
        { "Left", "Links" },
        { "Right", "Rechts" },
        { "Immediate", "Sofort" },
        { "Fade In", "Einblenden" },
        { "Fade Out", "Ausblenden" },
        { "Loop End", "Loop-Ende" },
        { "Overdub", "Overdub" },
        { "Replace", "Ersetzen" },
        { "Auto", "Auto" },

        // ---------------------------------------------------------------- tooltips
        { "MIDI learn: click a control, then move a knob / press a pad on your controller",
          "MIDI Learn: Bedienelement anklicken, dann am Controller einen Regler bewegen / ein Pad dr\xc3\xbc" "cken" },
        { "Show / remove MIDI assignments, load the default map",
          "MIDI-Zuweisungen anzeigen / entfernen, Standard-Map laden" },
        { "Tap tempo (internal tempo, used when host sync is off)",
          "Tap Tempo (internes Tempo, gilt wenn Host-Sync aus ist)" },
        { "Follow the tempo, bar grid and transport of the host",
          "Tempo, Taktraster und Transport des Hosts \xc3\xbc" "bernehmen" },
        { "Quantize: start / end recordings and starts on the beat or bar",
          "Quantisierung: Aufnahmen und Starts auf den Schlag oder Takt legen" },
        { "Start / stop all tracks", "Alle Spuren starten / stoppen" },
        { "Clear all five tracks", "Alle f\xc3\xbc" "nf Spuren l\xc3\xb6schen" },
        { "Settings, language", "Einstellungen, Sprache" },
        { "REC / DUB / PLAY  (right-click: MIDI learn)", "REC / DUB / PLAY  (Rechtsklick: MIDI Learn)" },
        { "STOP  (hold 1.5 s: clear track, right-click: MIDI learn)",
          "STOP  (1,5 s halten: Spur l\xc3\xb6schen, Rechtsklick: MIDI Learn)" },
        { "Reverse playback", "R\xc3\xbc" "ckw\xc3\xa4rts abspielen" },
        { "One shot: play once, then stop", "One Shot: einmal abspielen, dann stoppen" },
        { "Send this track through the track FX", "Diese Spur durch die Spur-FX schicken" },
        { "Undo the last recording / overdub", "Letzte Aufnahme / letzten Overdub r\xc3\xbc" "ckg\xc3\xa4ngig machen" },
        { "Redo", "Wiederherstellen" },
        { "Track settings, clear, import / export", "Spur-Einstellungen, l\xc3\xb6schen, Import / Export" },
        { "Keep the loop locked to the bar grid of the timeline", "Loop am Taktraster der Timeline festhalten" },
        { "Input FX %1 on/off", "Eingangs-FX %1 ein/aus" },
        { "Track FX %1 on/off", "Spur-FX %1 ein/aus" },

        // ---------------------------------------------------------------- dialogs
        { "Clear all tracks?", "Alle Spuren l\xc3\xb6schen?" },
        { "All five loops will be deleted.", "Alle f\xc3\xbc" "nf Loops werden gel\xc3\xb6scht." },
        { "Clear all", "Alle l\xc3\xb6schen" },
        { "Cancel", "Abbrechen" },
        { "Export track %1", "Spur %1 exportieren" },
        { "Import audio to track %1", "Audio in Spur %1 importieren" },

        // ---------------------------------------------------------------- track display
        { "TRACK %1", "SPUR %1" },
        { "TRACK %1 SETUP", "SPUR %1 SETUP" },
        { "EMPTY", "LEER" },
        { "REC", "REC" },
        { "DUB", "DUB" },
        { "PLAY", "PLAY" },
        { "STOP", "STOP" },
        { "STOPPING", "STOPPT" },
        { "REC WAIT", "REC WARTET" },
        { "PLAY WAIT", "PLAY WARTET" },
        { "REC END", "REC ENDE" },
        { "CLEARED", "GEL\xc3\x96SCHT" },
        { "DUB / PLAY", "DUB / PLAY" },
        { "%1 BAR", "%1 TAKT" },
        { "%1 BARS", "%1 TAKTE" },
        { "LVL %1", "PEG %1" },

        // ---------------------------------------------------------------- main display
        { "BAR  -- . --", "TAKT  -- . --" },
        { "BAR %1 . %2", "TAKT %1 . %2" },
        { "BPM   HOST", "BPM   HOST" },
        { "BPM   HOST (STOPPED) / INT CLOCK", "BPM   HOST (GESTOPPT) / INT. CLOCK" },
        { "BPM   INTERNAL", "BPM   INTERN" },
        { "QUANTIZE %1", "QUANT. %1" },
        { "LEARN: %1  >  send note / CC", "LEARN: %1  >  Note / CC senden" },
        { "MIDI LEARN: click a control", "MIDI LEARN: Bedienelement anklicken" },
        { "MEMORY %1 S / TRACK", "SPEICHER %1 S / SPUR" },
        { "LEARN: %1", "LEARN: %1" },
        { "MAPPING REMOVED: %1", "ZUWEISUNG ENTFERNT: %1" },
        { "DEFAULT MIDI MAP LOADED", "STANDARD-MIDI-MAP GELADEN" },
        { "ALL MIDI ASSIGNMENTS REMOVED", "ALLE MIDI-ZUWEISUNGEN ENTFERNT" },
        { "MAX LOOP LENGTH %1 S", "MAX. LOOP-L\xc3\x84NGE %1 S" },
        { "EXPORTED %1", "EXPORTIERT: %1" },
        { "EXPORT FAILED (TRACK EMPTY?)", "EXPORT FEHLGESCHLAGEN (SPUR LEER?)" },
        { "IMPORTED TO TRACK %1", "IN SPUR %1 IMPORTIERT" },
        { "IMPORT FAILED: %1", "IMPORT FEHLGESCHLAGEN: %1" },
        { "Unsupported audio file", "Audioformat wird nicht unterst\xc3\xbctzt" },
        { "LANGUAGE: ENGLISH", "SPRACHE: DEUTSCH" },

        // ---------------------------------------------------------------- menus
        { "MIDI Learn", "MIDI Learn" },
        { "Cancel MIDI Learn", "MIDI Learn abbrechen" },
        { "Remove MIDI mapping  (%1)", "MIDI-Zuweisung entfernen  (%1)" },
        { "No MIDI mapping", "Keine MIDI-Zuweisung" },
        { "MIDI assignments (click to remove)", "MIDI-Zuweisungen (zum Entfernen anklicken)" },
        { "No assignments yet", "Noch keine Zuweisungen" },
        { "Load default map (notes C1..B1, CC 20..25)", "Standard-Map laden (Noten C1..B1, CC 20..25)" },
        { "Remove all assignments", "Alle Zuweisungen entfernen" },
        { "livelooper setup", "livelooper Setup" },
        { "Record action", "Aufnahme-Aktion" },
        { "Follow host transport (start / stop with the host)", "Host-Transport folgen (Start / Stop mit dem Host)" },
        { "Maximum loop length", "Maximale Loop-L\xc3\xa4nge" },
        { "%1 seconds per track", "%1 Sekunden pro Spur" },
        { "Save loops with the project", "Loops im Projekt speichern" },
        { "Automatic (system language)", "Automatisch (Systemsprache)" },
        { "Parameter names in the host change after reloading the plug-in",
          "Parameternamen im Host \xc3\xa4ndern sich nach dem Neuladen des Plug-ins" },

        // ---------------------------------------------------------------- parameter names
        { "T%1 %2", "S%1 %2" },
        { "Track %1", "Spur %1" },
        { "Global", "Global" },
        { "Input FX", "Eingangs-FX" },
        { "Track FX", "Spur-FX" },
        { "In FX %1 %2", "Ein-FX %1 %2" },
        { "Trk FX %1 %2", "Sp-FX %1 %2" },
        { "Rec/Play", "Rec/Play" },
        { "Clear", "L\xc3\xb6schen" },
        { "Undo", "R\xc3\xbc" "ckg\xc3\xa4ngig" },
        { "Pan", "Panorama" },
        { "Reverse", "R\xc3\xbc" "ckw\xc3\xa4rts" },
        { "One Shot", "One Shot" },
        { "Loop Sync", "Loop-Sync" },
        { "Track FX", "Spur-FX" },
        { "Start Mode", "Startmodus" },
        { "Stop Mode", "Stoppmodus" },
        { "Dub Mode", "Dub-Modus" },
        { "All Start/Stop", "Alle Start/Stop" },
        { "All Start", "Alle Start" },
        { "All Stop", "Alle Stop" },
        { "Clear All", "Alle l\xc3\xb6schen" },
        { "Tap Tempo", "Tap Tempo" },
        { "Master Level", "Master-Pegel" },
        { "Input Monitor", "Eingangsmonitor" },
        { "Quantize", "Quantisierung" },
        { "Rec Action", "Aufnahme-Aktion" },
        { "Sync to Host", "Host-Sync" },
        { "Follow Transport", "Transport folgen" },
        { "Internal Tempo", "Internes Tempo" },
        { "Fade Time", "Fade-Zeit" },
        { "Rhythm On", "Rhythmus an" },
        { "Rhythm Level", "Rhythmus-Pegel" },
        { "Rhythm Pattern", "Rhythmus-Muster" },
        { "On", "An" },
        { "Type", "Typ" },
        { "Param 1", "Parameter 1" },
        { "Param 2", "Parameter 2" },
    };

    const std::map<juce::String, juce::String>& table()
    {
        static const std::map<juce::String, juce::String> map = [] {
            std::map<juce::String, juce::String> m;
            for (const auto& entry : kGerman)
                m.emplace (juce::String::fromUTF8 (entry[0]), juce::String::fromUTF8 (entry[1]));
            return m;
        }();
        return map;
    }

    std::atomic<int> setting { -1 };   // -1 = not loaded yet

    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "livelooper";
        o.filenameSuffix = ".settings";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = ".config/livelooper";
       #else
        o.folderName = "livelooper";
       #endif
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return std::make_unique<juce::PropertiesFile> (o);
    }

    juce::Array<Listener*>& listeners()
    {
        static juce::Array<Listener*> l;
        return l;
    }

    Language parse (const juce::String& s)
    {
        const auto v = s.trim().toLowerCase();
        if (v.startsWith ("de") || v == "german" || v == "deutsch") return Language::German;
        if (v.startsWith ("en") || v == "english") return Language::English;
        return Language::Automatic;
    }

    Language systemLanguage()
    {
        const auto lang = juce::SystemStats::getUserLanguage() + " " + juce::SystemStats::getDisplayLanguage();
        return lang.trim().toLowerCase().startsWith ("de") ? Language::German : Language::English;
    }
}

Language getSetting()
{
    if (setting.load() < 0)
    {
        auto props = openSettings();
        setting = (int) parse (props->getValue ("language", "auto"));
    }
    return (Language) setting.load();
}

void setSetting (Language language, bool persist)
{
    setting = (int) language;
    if (persist)
    {
        auto props = openSettings();
        props->setValue ("language", language == Language::German ? "de" : (language == Language::English ? "en" : "auto"));
        props->saveIfNeeded();
    }
    const auto copy = listeners();
    for (auto* l : copy)
        if (listeners().contains (l))
            l->languageChanged();
}

Language getEffective()
{
    const auto env = parse (juce::SystemStats::getEnvironmentVariable ("LIVELOOPER_LANGUAGE", {}));
    if (env != Language::Automatic)
        return env;
    const auto s = getSetting();
    return s == Language::Automatic ? systemLanguage() : s;
}

bool isGerman()
{
    return getEffective() == Language::German;
}

juce::String germanFor (const juce::String& english)
{
    const auto& t = table();
    const auto it = t.find (english);
    return it != t.end() ? it->second : juce::String();
}

juce::String tr (const juce::String& english)
{
    if (! isGerman())
        return english;
    const auto de = germanFor (english);
    return de.isNotEmpty() ? de : english;
}

juce::String tr (const juce::String& english, const juce::String& arg1)
{
    return tr (english).replace ("%1", arg1);
}

juce::String tr (const juce::String& english, const juce::String& arg1, const juce::String& arg2)
{
    return tr (english).replace ("%1", arg1).replace ("%2", arg2);
}

int getNumTranslations()
{
    return (int) table().size();
}

void addListener (Listener* l)    { listeners().addIfNotAlreadyThere (l); }
void removeListener (Listener* l) { listeners().removeAllInstancesOf (l); }
} // namespace livelooper::i18n
