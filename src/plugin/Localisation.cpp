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
        { "%1 TRACK LOOP STATION", "%1-SPUR LOOP STATION" },
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
        { "Trap", "Trap" },
        { "Drum & Bass", "Drum & Bass" },
        { "Afrobeat", "Afrobeat" },
        { "Dembow", "Dembow" },
        { "UK Garage", "UK Garage" },
        { "Neo Soul", "Neo Soul" },
        { "Disco", "Disco" },
        { "Afro House", "Afro House" },

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
        { "Clear all tracks", "Alle Spuren l\xc3\xb6schen" },
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
        { "Drag the waveform onto an audio track in your DAW to export the loop", "Wellenform auf eine Audiospur in deiner DAW ziehen, um den Loop zu exportieren" },

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
        { "PLAY / DUB", "PLAY / DUB" },
        { "IN", "IN" },
        { "OUT", "OUT" },
        { "Beep", "Piep" },
        { "Woodblock", "Holzblock" },
        { "Click", "Klick" },
        { "Drum kit: Studio, 808, 909 or Lo-Fi", "Drum-Kit: Studio, 808, 909 oder Lo-Fi" },
        { "Swing: 50 % = straight, 66 % = triplet feel", "Swing: 50 % = gerade, 66 % = Triolen-Feeling" },
        { "Humanize: small variations in timing and velocity", "Humanisierung: kleine Schwankungen in Timing und Anschlagst\xc3\xa4rke" },
        { "Rimshot", "Rimshot" },
        { "Drum Kit", "Drum-Kit" },
        { "Swing", "Swing" },
        { "Humanize", "Humanisierung" },
        { "Feel", "Feel" },
        { "Kit", "Kit" },
        { "Studio", "Studio" },
        { "808", "808" },
        { "909", "909" },
        { "Control from Ableton Push (User port)", "Steuerung mit Ableton Push (User-Port)" },
        { "Press USER on Push to switch between your DAW and livelooper", "USER auf Push wechselt zwischen deiner DAW und livelooper" },
        { "Ableton Push", "Ableton Push" },
        { "Connected: %1", "Verbunden: %1" },
        { "Push found, but its User port is in use (Windows: switch it off in your DAW's MIDI settings)", "Push gefunden, aber sein User-Port ist belegt (Windows: in den MIDI-Einstellungen deiner DAW ausschalten)" },
        { "No Push found", "Kein Push gefunden" },
        { "Push is controlled by another livelooper instance", "Push wird von einer anderen livelooper-Instanz gesteuert" },
        { "Resample", "Resample" },
        { "Main", "Haupt" },
        { "Direct", "Direkt" },
        { "Main + Direct", "Haupt + Direkt" },
        { "Rhythm Kick", "Rhythmus-Kick" },
        { "Track 1", "Spur 1" },
        { "Capture", "Capture" },
        { "Ducking", "Ducking" },
        { "Output", "Ausgang" },
        { "Pitch", "Tonh\xc3\xb6he" },
        { "Follow Tempo", "Tempo folgen" },
        { "Capture Bars", "Capture-Takte" },
        { "Auto Record", "Auto-Aufnahme" },
        { "Auto Record Threshold", "Auto-Aufnahme-Schwelle" },
        { "Ducking Source", "Ducking-Quelle" },
        { "Store Scene", "Szene speichern" },
        { "Scene %1", "Szene %1" },
        { "SCENES", "SZENEN" },
        { "STORE", "SPEICHERN" },
        { "CAPTURE", "CAPTURE" },
        { "FOLLOW TEMPO", "TEMPO FOLGEN" },
        { "AUTO REC", "AUTO REC" },
        { "AUTO REC ON", "AUTO REC AN" },
        { "AUTO REC OFF", "AUTO REC AUS" },
        { "Feedback: how much of the loop remains with every overdub pass (100 % = all)", "Feedback: wie viel vom Loop bei jedem Overdub-Durchlauf erhalten bleibt (100 % = alles)" },
        { "Ducking: lowers this track while the ducking source (SETUP) plays", "Ducking: senkt diese Spur ab, w\xc3\xa4hrend die Ducking-Quelle (SETUP) spielt" },
        { "Pitch in semitones (the length stays the same)", "Tonh\xc3\xb6he in Halbt\xc3\xb6nen (die L\xc3\xa4nge bleibt gleich)" },
        { "Follow tempo: the loop follows tempo changes without changing its pitch (time-stretch)", "Tempo folgen: der Loop folgt Tempo\xc3\xa4nderungen, ohne seine Tonh\xc3\xb6he zu \xc3\xa4ndern (Time-Stretch)" },
        { "Capture: turns the last bars you played into a loop on this (empty) track", "Capture: macht aus den zuletzt gespielten Takten einen Loop auf dieser (leeren) Spur" },
        { "Capture: turns the last %1 bars you played into a loop on the first empty track (no need to press record)", "Capture: macht aus den zuletzt gespielten %1 Takten einen Loop auf der ersten leeren Spur (ohne vorher Aufnahme zu dr\xc3\xbc" "cken)" },
        { "Scene %1: starts its tracks and stops the others (quantized)", "Szene %1: startet ihre Spuren und stoppt die anderen (quantisiert)" },
        { "Scene %1 is empty: click to store which tracks are playing now", "Szene %1 ist leer: Klick speichert, welche Spuren gerade spielen" },
        { "Store: the next scene button you press stores which tracks are playing now", "Speichern: der n\xc3\xa4" "chste Szenen-Knopf speichert, welche Spuren gerade spielen" },
        { "5 tracks", "5 Spuren" },
        { "8 tracks", "8 Spuren" },
        { "%1 bar", "%1 Takt" },
        { "%1 bars", "%1 Takte" },
        { "Auto record (armed tracks wait for the input)", "Auto-Aufnahme (scharfe Spuren warten auf das Eingangssignal)" },
        { "Threshold", "Schwelle" },
        { "Set the track's OUTPUT to Direct in its SETUP", "Den AUSGANG der Spur in ihrem SETUP auf Direkt stellen" },
        { "Export stems (one WAV per track) ...", "Stems exportieren (eine WAV pro Spur) ..." },
        { "Tracks", "Spuren" },
        { "Capture length", "Capture-L\xc3\xa4nge" },
        { "Auto record", "Auto-Aufnahme" },
        { "Ducking source", "Ducking-Quelle" },
        { "Direct track outputs", "Direkte Spurausg\xc3\xa4nge" },
        { "Export stems to folder", "Stems in Ordner exportieren" },
        { "%1 STEMS EXPORTED", "%1 STEMS EXPORTIERT" },
        { "NOTHING TO EXPORT", "NICHTS ZU EXPORTIEREN" },
        { "Metronome Sound", "Metronomklang" },
        { "Metronome sound", "Metronomklang" },
        { "METRONOME: %1", "METRONOM: %1" },
        { "Outputs %1 / %2", "Ausg\xc3\xa4nge %1 / %2" },
        { "This is not a livelooper set.", "Das ist kein livelooper-Set." },
        { "Cue output (outputs of the audio interface)", "Cue-Ausgang (Ausg\xc3\xa4nge des Audio-Interfaces)" },
        { "Rhythm / metronome on", "Rhythmus / Metronom auf" },
        { "Main output", "Hauptausgang" },
        { "Cue output only", "Nur Cue-Ausgang" },
        { "Main and cue output", "Haupt- und Cue-Ausgang" },
        { "Enable the outputs in Options > Audio/MIDI Settings", "Ausg\xc3\xa4nge unter Options > Audio/MIDI Settings aktivieren" },
        { "Cue output (click / rhythm)", "Cue-Ausgang (Click / Rhythmus)" },
        { "CUE: %1", "CUE: %1" },
        { "Save set ...", "Set speichern ..." },
        { "Load set ...", "Set laden ..." },
        { "Save set", "Set speichern" },
        { "Load set", "Set laden" },
        { "SET SAVED: %1", "SET GESPEICHERT: %1" },
        { "SET COULD NOT BE SAVED", "SET KONNTE NICHT GESPEICHERT WERDEN" },
        { "SET LOADED: %1", "SET GELADEN: %1" },
        { "LOAD FAILED: %1", "LADEN FEHLGESCHLAGEN: %1" },
        { "REC > PLAY > DUB", "REC > PLAY > DUB" },
        { "REC > DUB > PLAY", "REC > DUB > PLAY" },
        { "REC BUTTON: %1", "REC-TASTE: %1" },
        { "HOLD KEY TO CLEAR TRACK %1", "TASTE HALTEN: SPUR %1 L\xc3\x96SCHEN" },
        { "CLEAR CANCELLED - HOLD THE KEY 1.5 S", "NICHT GEL\xc3\x96SCHT - TASTE 1,5 S HALTEN" },
        { "TRACK %1 CLEARED", "SPUR %1 GEL\xc3\x96SCHT" },
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

        // ---------------------------------------------------------------- computer keyboard / tempo entry
        { "Tempo +1 BPM", "Tempo +1 BPM" },
        { "Tempo -1 BPM", "Tempo -1 BPM" },
        { "Tempo +0.1 BPM", "Tempo +0,1 BPM" },
        { "Tempo -0.1 BPM", "Tempo -0,1 BPM" },
        { "Type in tempo (BPM)", "Tempo eintippen (BPM)" },
        { "%1: next value", "%1: n\xc3\xa4" "chster Wert" },
        { "Ctrl", "Strg" },
        { "Space", "Leertaste" },
        { "Return", "Eingabe" },
        { "Backspace", "R\xc3\xbc" "cktaste" },
        { "Delete", "Entf" },
        { "Arrow Up", "Pfeil hoch" },
        { "Arrow Down", "Pfeil runter" },
        { "Arrow Left", "Pfeil links" },
        { "Arrow Right", "Pfeil rechts" },
        { "KEYBOARD CONTROL", "TASTATURSTEUERUNG" },
        { "DEFAULTS", "STANDARD" },
        { "REMOVE ALL", "ALLE ENTFERNEN" },
        { "Switch control by the computer keyboard on or off", "Steuerung \xc3\xbc" "ber die Computertastatur ein- oder ausschalten" },
        { "Load the default keyboard mapping", "Standard-Tastaturbelegung laden" },
        { "Default keyboard mapping loaded", "Standard-Tastaturbelegung geladen" },
        { "Remove every key assignment", "Alle Tastenzuweisungen entfernen" },
        { "All key assignments removed", "Alle Tastenzuweisungen entfernt" },
        { "Keyboard mapping", "Tastaturbelegung" },
        { "KEYBOARD MAPPING", "TASTATURBELEGUNG" },
        { "Click a function, then press its new key. Esc cancels, right-click removes the key.", "Funktion anklicken, dann die neue Taste dr\xc3\xbc" "cken. Esc bricht ab, Rechtsklick entfernt die Taste." },
        { "Keyboard control is on", "Tastatursteuerung ist an" },
        { "Keyboard control is off", "Tastatursteuerung ist aus" },
        { "press a key ...", "Taste dr\xc3\xbc" "cken ..." },
        { "Key removed: %1", "Taste entfernt: %1" },
        { "%1 moved from: %2", "%1 \xc3\xbc" "bernommen von: %2" },
        { "%1 assigned", "%1 zugewiesen" },
        { "Click the tempo to type in the BPM", "Auf das Tempo klicken, um die BPM einzutippen" },
        { "Type in the tempo (40 - 300 BPM), Return sets it, Esc cancels", "Tempo eintippen (40 - 300 BPM), Eingabe \xc3\xbc" "bernimmt, Esc bricht ab" },
        { "Internal tempo - right-click to type in the BPM, or click the tempo on the display", "Internes Tempo - Rechtsklick zum Eintippen der BPM, oder auf das Tempo im Display klicken" },
        { "KEY %1 > %2", "TASTE %1 > %2" },
        { "TEMPO %1 BPM", "TEMPO %1 BPM" },
        { "TYPE IN TEMPO (40 - 300), RETURN", "TEMPO EINTIPPEN (40 - 300), EINGABE" },
        { "INTERNAL TEMPO %1 BPM (HOST TEMPO IN USE)", "INTERNES TEMPO %1 BPM (HOST-TEMPO AKTIV)" },
        { "Type in tempo (BPM) ...", "Tempo eintippen (BPM) ..." },
        { "Computer keyboard control", "Steuerung mit der Computertastatur" },
        { "Keyboard mapping ...", "Tastaturbelegung ..." },
        { "KEYBOARD CONTROL ON", "TASTATURSTEUERUNG AN" },
        { "KEYBOARD CONTROL OFF", "TASTATURSTEUERUNG AUS" },
        // ---------------------------------------------------------------- v1.4 performance
        { "Slice 1", "Slice 1" },
        { "Slice 2", "Slice 2" },
        { "Slice 3", "Slice 3" },
        { "Slice 4", "Slice 4" },
        { "Slice 5", "Slice 5" },
        { "Slice 6", "Slice 6" },
        { "Back in Sync", "Zur\xc3\xbc" "ck in Sync" },
        { "Mute", "Stumm" },
        { "Solo", "Solo" },
        { "Double Loop", "Loop verdoppeln" },
        { "Halve Loop", "Loop halbieren" },
        { "Octave Up", "Oktave hoch" },
        { "Octave Down", "Oktave runter" },
        { "Copy to Empty Track", "In leere Spur kopieren" },
        { "Rec Length", "Aufnahmel\xc3\xa4nge" },
        { "1 Bar", "1 Takt" },
        { "2 Bars", "2 Takte" },
        { "4 Bars", "4 Takte" },
        { "8 Bars", "8 Takte" },
        { "16 Bars", "16 Takte" },
        { "32 Bars", "32 Takte" },
        { "Tape Stop", "Tape-Stopp" },
        { "Performance Filter", "Performance-Filter" },
        // v1.5 parameter names (as the DAW shows them with the German interface)
        { "Beat FX On", "Beat-FX an" },
        { "Beat FX Hold", "Beat-FX halten" },
        { "Beat FX Type", "Beat-FX-Typ" },
        { "Beat FX Beat", "Beat-FX-Schlag" },
        { "Beat FX Level/Depth", "Beat-FX Pegel/Tiefe" },
        { "Beat FX Channel", "Beat-FX-Kanal" },
        { "Color FX Type", "Color-FX-Typ" },
        { "Color FX", "Color-FX" },
        { "Beat Repeat Hold", "Beat Repeat halten" },
        { "Beat Repeat Latch", "Beat Repeat Latch" },
        { "Beat Repeat Timing", "Beat-Repeat-Timing" },
        { "Stutter Hold", "Stutter halten" },
        { "Stutter Latch", "Stutter Latch" },
        { "Stutter Timing", "Stutter-Timing" },
        { "Metronome On", "Metronom an" },
        { "Metronome Level", "Metronom-Pegel" },
        { "Metronome Accent", "Metronom-Betonung" },
        { "Link Start/Stop Sync", "Link Start/Stopp-Sync" },
        // Push display messages (v1.4)
        { "Rec length: 1 bar", "Aufnahmel\xc3\xa4nge: 1 Takt" },
        { "Rec length: %1 bars", "Aufnahmel\xc3\xa4nge: %1 Takte" },
        { "T%1 loop x2", "S%1 Loop x2" },
        { "T%1 loop /2", "S%1 Loop /2" },
        { "T%1 octave up", "S%1 Oktave hoch" },
        { "T%1 octave down", "S%1 Oktave runter" },
        { "Copy T%1 to T%2", "Kopiere S%1 nach S%2" },
        { "Copy T%1 to the next empty track", "Kopiere S%1 in die n\xc3\xa4" "chste leere Spur" },
        { "DUPLICATE: press the track to copy, then an empty track", "DUPLICATE: Spur zum Kopieren, dann eine leere Spur" },
        { "Touch strip: tape stop", "Touchstrip: Tape-Stopp" },
        { "Touch strip: filter", "Touchstrip: Filter" },
        { "Meters on", "Pegelanzeige an" },
        { "Meters off", "Pegelanzeige aus" },
        { "T%1 cleared", "S%1 gel\xc3\xb6scht" },
        { "T%1 back in sync", "S%1 wieder synchron" },
        { "T%1 selected", "S%1 ausgew\xc3\xa4hlt" },
        // ---------------------------------------------------------------- v1.5
        { "Ableton Link", "Ableton Link" },
        { "Ableton Link (tempo and beat with other apps)", "Ableton Link (Tempo und Beat mit anderen Apps)" },
        { "About livelooper", "\xc3\x9c" "ber livelooper" },
        { "About livelooper ...", "\xc3\x9c" "ber livelooper ..." },
        { "Accent on the first beat of every bar", "Betonung auf dem ersten Schlag jedes Takts" },
        { "App: replaced when you quit livelooper", "App: wird beim Beenden von livelooper ersetzt" },
        { "Audio interface, inputs / outputs, sample rate, MIDI inputs", "Audio-Interface, Ein-/Ausg\xc3\xa4" "nge, Samplerate, MIDI-Eing\xc3\xa4" "nge" },
        { "Audio/MIDI Settings", "Audio-/MIDI-Einstellungen" },
        { "Audio/MIDI Settings ...", "Audio-/MIDI-Einstellungen ..." },
        { "Automatic means: a new version is downloaded and checked in the background and replaces the app when you quit livelooper. Plug-ins are never replaced while a DAW uses them.", "Automatisch hei\xc3\x9f" "t: Eine neue Version wird im Hintergrund geladen und gepr\xc3\xbc" "ft und ersetzt die App, wenn du livelooper beendest. Plug-ins werden nie ersetzt, solange eine DAW sie benutzt." },
        { "Automatic means: a new version is downloaded and checked in the background. A plug-in is never replaced while your DAW runs - install it with the DAW closed (button above, or with the standalone app).", "Automatisch hei\xc3\x9f" "t: Eine neue Version wird im Hintergrund geladen und gepr\xc3\xbc" "ft. Ein Plug-in wird nie ersetzt, solange deine DAW l\xc3\xa4" "uft - installiere es bei geschlossener DAW (Knopf oben oder mit der Standalone-App)." },
        { "BEAT FX", "BEAT-FX" },
        { "BEAT REPEAT / STUTTER", "BEAT REPEAT / STUTTER" },
        { "BPM   LINK", "BPM   LINK" },
        { "Beat FX on / off", "Beat-FX an / aus" },
        // v1.5 Push 1: performance FX, metronome, Link, capture length
        { "Beat FX on", "Beat-FX an" },
        { "Beat FX off", "Beat-FX aus" },
        { "Beat FX beat %1", "Beat-FX Schlag %1" },
        { "Beat repeat %1", "Beat Repeat %1" },
        { "Beat repeat latch on", "Beat Repeat Latch an" },
        { "Beat repeat latch off", "Beat Repeat Latch aus" },
        { "Stutter %1", "Stutter %1" },
        { "Stutter latch on", "Stutter Latch an" },
        { "Stutter latch off", "Stutter Latch aus" },
        { "Metronome on", "Metronom an" },
        { "Metronome off", "Metronom aus" },
        { "Capture: 1 bar", "Capture: 1 Takt" },
        { "Capture: %1 bars", "Capture: %1 Takte" },
        { "Rhythm groove", "Rhythmus-Groove" },
        { "Rhythm fills", "Rhythmus-Fills" },
        { "GROOVE: %1", "GROOVE: %1" },
        { "FILLS: %1", "FILLS: %1" },
        { "Beat FX type (synced to the tempo)", "Beat-FX-Typ (zum Tempo synchron)" },
        { "Beat FX while held (momentary) - map it to a pad", "Beat-FX solange gedr\xc3\xbc" "ckt (moment\xc3\xa4" "r) - ideal auf einem Pad" },
        { "Beat fraction of the beat FX in beats: 1/32 beat ... 16 beats (4 bars)", "Notenwert des Beat-FX in Schl\xc3\xa4" "gen: 1/32 Schlag ... 16 Schl\xc3\xa4" "ge (4 Takte)" },
        { "Beat repeat latched on (until pressed again)", "Beat Repeat eingerastet (bis zum n\xc3\xa4" "chsten Druck)" },
        { "Beat repeat timing (note value)", "Beat-Repeat-Timing (Notenwert)" },
        { "Beat repeat while held: repeats the next 1/2 ... 1/32 note from the next grid line, back in sync on release", "Beat Repeat solange gedr\xc3\xbc" "ckt: wiederholt die n\xc3\xa4" "chste 1/2 ... 1/32 Note ab der n\xc3\xa4" "chsten Rasterlinie, beim Loslassen wieder synchron" },
        { "COLOR", "COLOR" },
        { "COLOR FX", "COLOR-FX" },
        { "Check automatically on start", "Beim Start automatisch pr\xc3\xbc" "fen" },
        { "Check for updates", "Nach Updates suchen" },
        { "Check for updates now ...", "Jetzt nach Updates suchen ..." },
        { "Checking GitHub for a new version ...", "Suche auf GitHub nach einer neuen Version ..." },
        { "Clear list", "Liste leeren" },
        { "Close every DAW that uses livelooper first - a plug-in is never replaced while a DAW has it loaded.\n\nThe new VST3 (and AU on macOS) will be copied to your user plug-in folder.", "Schlie\xc3\x9f" "e zuerst jede DAW, die livelooper benutzt - ein Plug-in wird nie ersetzt, solange eine DAW es geladen hat.\n\nDas neue VST3 (und AU unter macOS) wird in deinen Benutzer-Plug-in-Ordner kopiert." },
        { "Color FX of this track: centre = off, turn left / right for more (effect chosen in COLOR FX); double-click = off", "Color-FX dieser Spur: Mitte = aus, nach links / rechts f\xc3\xbc" "r mehr (Effekt in COLOR-FX gew\xc3\xa4" "hlt); Doppelklick = aus" },
        { "Color FX type for every track's COLOR knob", "Color-FX-Typ f\xc3\xbc" "r den COLOR-Regler jeder Spur" },
        { "Download", "Herunterladen" },
        { "Download and install", "Herunterladen und installieren" },
        { "Downloading %1 ... %2 %", "Lade %1 ... %2 %" },
        { "Enable the outputs in Audio/MIDI Settings (button above the looper)", "Ausg\xc3\xa4" "nge in den Audio-/MIDI-Einstellungen aktivieren (Knopf \xc3\xbc" "ber dem Looper)" },
        { "FX channel: where beat FX, beat repeat and stutter work - master, all loops, the input (recorded with the effect) or one track", "FX-Kanal: wo Beat-FX, Beat Repeat und Stutter wirken - Master, alle Loops, Eingang (wird mit Effekt aufgenommen) oder eine Spur" },
        { "Free software under the GNU Affero General Public License v3 (AGPLv3), without any warranty. Uses JUCE (AGPLv3) and Ableton Link (GPLv2+).", "Freie Software unter der GNU Affero General Public License v3 (AGPLv3), ohne jede Gew\xc3\xa4" "hrleistung. Nutzt JUCE (AGPLv3) und Ableton Link (GPLv2+)." },
        { "Help ...", "Hilfe ..." },
        { "Help: every function explained (searchable)", "Hilfe: jede Funktion erkl\xc3\xa4" "rt (durchsuchbar)" },
        { "How should livelooper start from now on?\n\nLast settings: every start (and every new plug-in without a saved project) uses the settings you used last - effects, rhythm, metronome, MIDI and keyboard mapping, tracks, Push.\nDefault settings: always start fresh.\n\nYou can change this any time in SETUP > Settings on start.", "Wie soll livelooper ab jetzt starten?\n\nLetzte Einstellungen: Jeder Start (und jedes neue Plug-in ohne gespeichertes Projekt) nutzt die zuletzt verwendeten Einstellungen - Effekte, Rhythmus, Metronom, MIDI- und Tastatur-Zuordnung, Spuren, Push.\nStandard-Einstellungen: immer frisch starten.\n\nDu kannst das jederzeit unter SETUP > Einstellungen beim Start \xc3\xa4" "ndern." },
        { "In a DAW, HOST SYNC has priority - switch it off to follow Link", "In einer DAW hat HOST-SYNC Vorrang - schalte es aus, um Link zu folgen" },
        { "Install", "Installieren" },
        { "Install failed: %1", "Installation fehlgeschlagen: %1" },
        { "Install plug-ins", "Plug-ins installieren" },
        { "Install updates automatically", "Updates automatisch installieren" },
        { "Installed: %1", "Installiert: %1" },
        { "LINK %1 PEERS", "LINK %1 TEILNEHMER" },
        { "LINK (HOST SYNC HAS PRIORITY)", "LINK: HOST-SYNC ZUERST" },
        { "LINK 1 PEER", "LINK 1 TEILNEHMER" },
        { "LINK ON", "LINK AN" },
        { "LINK OFF", "LINK AUS" },
        { "Last check: %1", "Letzte Pr\xc3\xbc" "fung: %1" },
        { "Last used sets", "Zuletzt verwendete Sets" },
        { "License", "Lizenz" },
        { "License (AGPLv3)", "Lizenz (AGPLv3)" },
        { "Link is not available in this build", "Link ist in diesem Build nicht verf\xc3\xbc" "gbar" },
        { "Link off", "Link aus" },
        { "Link: %1 peers, %2 BPM", "Link: %1 Teilnehmer, %2 BPM" },
        { "Link: 1 peer, %2 BPM", "Link: 1 Teilnehmer, %2 BPM" },
        { "Load a saved state ...", "Gespeicherten Zustand laden ..." },
        { "Longer beat fraction", "L\xc3\xa4" "ngerer Notenwert" },
        { "METRONOME", "METRONOM" },
        { "Metronome on / off (independent of the rhythm)", "Metronom an / aus (unabh\xc3\xa4" "ngig vom Rhythmus)" },
        { "No access to the microphone", "Kein Zugriff auf das Mikrofon" },
        { "No sets used yet", "Noch keine Sets verwendet" },
        { "Nothing found.", "Nichts gefunden." },
        { "Performance FX", "Performance-FX" },
        { "Plug-in: downloaded and prepared, you install it with your DAW closed", "Plug-in: wird geladen und vorbereitet, du installierst es bei geschlossener DAW" },
        { "Quit and install now", "Beenden und jetzt installieren" },
        { "Release page", "Release-Seite" },
        { "Reset all settings to the defaults now (keeps the loops)", "Alle Einstellungen jetzt zur\xc3\xbc" "cksetzen (Loops bleiben)" },
        { "Reset to default state", "Auf Standardzustand zur\xc3\xbc" "cksetzen" },
        { "SETTINGS RESET TO DEFAULTS", "EINSTELLUNGEN ZUR\xc3\x9c" "CKGESETZT" },
        { "Save current state ...", "Aktuellen Zustand speichern ..." },
        { "Search the help (e.g. stutter, Link, capture)", "Hilfe durchsuchen (z. B. Stutter, Link, Capture)" },
        { "Settings on start", "Einstellungen beim Start" },
        { "Settings, sets, Link, updates, about, language", "Einstellungen, Sets, Link, Updates, \xc3\x9c" "ber, Sprache" },
        { "Shorter beat fraction", "K\xc3\xbc" "rzerer Notenwert" },
        { "Show folder", "Ordner zeigen" },
        { "Start with the default settings", "Mit Standard-Einstellungen starten" },
        { "Start with the last used settings", "Mit den zuletzt verwendeten Einstellungen starten" },
        { "Stutter latched on (until pressed again)", "Stutter eingerastet (bis zum n\xc3\xa4" "chsten Druck)" },
        { "Stutter timing (note value)", "Stutter-Timing (Notenwert)" },
        { "Stutter while held: chopped repeats of the timing, back in sync on release", "Stutter solange gedr\xc3\xbc" "ckt: zerhackte Wiederholungen im Timing, beim Loslassen wieder synchron" },
        { "Sync start / stop with Link", "Start / Stopp mit Link synchronisieren" },
        { "Third-party notices", "Hinweise zu Drittkomponenten" },
        { "Tip: hover over any control for a short explanation, right-click it for MIDI learn.", "Tipp: Maus \xc3\xbc" "ber ein Bedienelement f\xc3\xbc" "r eine kurze Erkl\xc3\xa4" "rung, Rechtsklick f\xc3\xbc" "r MIDI Learn." },
        { "UPDATE %1 DOWNLOADED - SEE SETUP > ABOUT", "UPDATE %1 GELADEN - SIEHE SETUP > \xc3\x9c" "BER" },
        { "UPDATE %1 IS INSTALLED WHEN YOU QUIT", "UPDATE %1 WIRD BEIM BEENDEN INSTALLIERT" },
        { "UPDATE AVAILABLE: LIVELOOPER %1", "UPDATE VERF\xc3\x9c" "GBAR: LIVELOOPER %1" },
        { "Update check: %1", "Update-Pr\xc3\xbc" "fung: %1" },
        { "Updates", "Updates" },
        { "Use default settings", "Standard-Einstellungen verwenden" },
        { "Use last settings", "Letzte Einstellungen verwenden" },
        { "Version %1", "Version %1" },
        { "Version %1 is available.", "Version %1 ist verf\xc3\xbc" "gbar." },
        { "Version %1 is downloaded and verified.", "Version %1 ist geladen und gepr\xc3\xbc" "ft." },
        { "Version %1 is downloaded and verified: %2", "Version %1 ist geladen und gepr\xc3\xbc" "ft: %2" },
        { "Version %1 replaces this app when you quit livelooper.", "Version %1 ersetzt diese App, wenn du livelooper beendest." },
        { "Welcome to livelooper", "Willkommen bei livelooper" },
        { "build %1", "Build %1" },
        { "designed by groovex", "designed by groovex" },
        { "livelooper help", "livelooper Hilfe" },
        { "livelooper is up to date (latest: %1)", "livelooper ist aktuell (neueste: %1)" },
        { "macOS does not allow livelooper to record. Allow it in System Settings > Privacy & Security > Microphone, then restart livelooper.", "macOS erlaubt livelooper keine Aufnahme. Erlaube es unter Systemeinstellungen > Datenschutz & Sicherheit > Mikrofon und starte livelooper neu." },
        { "never", "nie" },
        { "Ping Pong", "Ping Pong" },
        { "Spiral", "Spiral" },
        { "Trans", "Trans" },
        { "Roll", "Roll" },
        { "Slip Roll", "Slip Roll" },
        { "Reverse Roll", "Reverse Roll" },
        { "Vinyl Brake", "Vinyl-Bremse" },
        { "Helix", "Helix" },
        { "Noise", "Rauschen" },
        { "Echo", "Echo" },
        { "Filter", "Filter" },
        { "Dub Echo", "Dub-Echo" },
        { "Crush", "Crush" },
        { "Sweep", "Sweep" },
        { "Loops", "Loops" },
        { "Every 4 Bars", "Alle 4 Takte" },
        { "Every 8 Bars", "Alle 8 Takte" },
        { "Straight", "Gerade" },
        { "Natural", "Nat\xc3\xbc" "rlich" },
        { "Laid Back", "Laid Back" },
        { "Pushed", "Treibend" },
        { "Drunk", "Drunk" },
        { "909 Style", "909-Stil" },
        { "Groove", "Groove" },
        { "Fills", "Fills" },
        { "Track 2", "Spur 2" },
        { "Track 3", "Spur 3" },
        { "Track 4", "Spur 4" },
        { "Track 5", "Spur 5" },
        { "Track 6", "Spur 6" },
        { "Track 7", "Spur 7" },
        { "Track 8", "Spur 8" },
        { "No release published yet", "Noch kein Release ver\xc3\xb6" "ffentlicht" },
        { "No connection to GitHub (offline, or not available in this build)", "Keine Verbindung zu GitHub (offline oder in diesem Build nicht verf\xc3\xbc" "gbar)" },
        { "Unexpected answer from GitHub", "Unerwartete Antwort von GitHub" },
        { "The download is incomplete", "Der Download ist unvollst\xc3\xa4" "ndig" },
        { "Checksum mismatch - the download was damaged or changed", "Pr\xc3\xbc" "fsumme stimmt nicht - der Download ist besch\xc3\xa4" "digt oder ver\xc3\xa4" "ndert" },
        { "The download is not a valid ZIP file", "Der Download ist keine g\xc3\xbc" "ltige ZIP-Datei" },
        { "Download failed", "Download fehlgeschlagen" },
        { "Could not unpack the download", "Download konnte nicht entpackt werden" },
        { "The new app was not found in the download", "Die neue App wurde im Download nicht gefunden" },
        { "Could not write the update helper", "Update-Helfer konnte nicht geschrieben werden" },
        { "Could not start the update helper", "Update-Helfer konnte nicht gestartet werden" },
        { "Automatic installation is not available on this platform", "Automatische Installation ist auf dieser Plattform nicht verf\xc3\xbc" "gbar" },
        { "No plug-ins found in the download", "Keine Plug-ins im Download gefunden" },
        { "Nothing downloaded yet", "Noch nichts heruntergeladen" },
        // ---------------------------------------------------------------- v1.6 features (branch v1.6-wip)
        { "Automatic (by MIDI port name)", "Automatisch (nach MIDI-Portname)" },
        { "CLIP EXPORT FAILED: %1", "CLIP-EXPORT FEHLGESCHLAGEN: %1" },
        { "CLIP FRAMES + WAV SAVED (NO VIDEO ENCODER): %1", "CLIP-EINZELBILDER + WAV GESPEICHERT (KEIN VIDEO-ENCODER): %1" },
        { "CLIP SAVED: %1", "CLIP GESPEICHERT: %1" },
        { "Choose recordings folder", "Ordner f\xc3\xbc" "r Aufnahmen w\xc3\xa4" "hlen" },
        { "Choose recordings folder ...", "Ordner f\xc3\xbc" "r Aufnahmen w\xc3\xa4" "hlen ..." },
        { "Clip export 9:16 (Reels, TikTok, Shorts)", "Clip-Export 9:16 (Reels, TikTok, Shorts)" },
        { "Controller found, but its MIDI port is in use (Windows: switch it off in your DAW's MIDI settings)", "Controller gefunden, aber sein MIDI-Port ist belegt (Windows: in den MIDI-Einstellungen deiner DAW ausschalten)" },
        { "Controller templates (grid controllers)", "Controller-Vorlagen (Grid-Controller)" },
        { "Controller templates (grid controllers, experimental)", "Controller-Vorlagen (Grid-Controller, experimentell)" },
        { "Experimental \xe2\x80\x93 untested on hardware, feedback welcome", "Experimentell \xe2\x80\x93 nicht auf Hardware getestet, Feedback willkommen" },
        { "experimental", "experimentell" },
        { "DEMO SET LOADED - PRESS ALL START / STOP", "DEMO-SET GELADEN - ALLE START / STOP DR\xc3\x9c" "CKEN" },
        { "EMPTY SET", "LEERES SET" },
        { "EXPORTING 9:16 CLIP ...", "9:16-CLIP WIRD EXPORTIERT ..." },
        { "Empty set (clear everything)", "Leeres Set (alles l\xc3\xb6" "schen)" },
        { "Export the last 20 seconds as 9:16 video ...", "Die letzten 20 Sekunden als 9:16-Video exportieren ..." },
        { "Export the last performance recording as 9:16 video ...", "Die letzte Performance-Aufnahme als 9:16-Video exportieren ..." },
        { "Guided tour (60 seconds)", "Gef\xc3\xbc" "hrte Tour (60 Sekunden)" },
        { "Help (every function, searchable) ...", "Hilfe (alle Funktionen, durchsuchbar) ..." },
        { "Help: every function explained (searchable), guided tour", "Hilfe: alle Funktionen erkl\xc3\xa4" "rt (durchsuchbar), gef\xc3\xbc" "hrte Tour" },
        { "Load demo set", "Demo-Set laden" },
        { "Loop Tricks & Recorder", "Loop-Tricks & Recorder" },
        { "NO PERFORMANCE RECORDING YET", "NOCH KEINE PERFORMANCE-AUFNAHME" },
        { "NOTHING TO EXPORT YET - PLAY SOMETHING FIRST", "NOCH NICHTS ZU EXPORTIEREN - ERST ETWAS SPIELEN" },
        { "No supported controller found", "Kein unterst\xc3\xbc" "tzter Controller gefunden" },
        { "Performance recorder", "Performance-Recorder" },
        { "Performance recorder: records everything livelooper plays (master mix) as a WAV file into %1", "Performance-Recorder: nimmt alles, was livelooper spielt (Master-Mix), als WAV-Datei auf - in %1" },
        { "RECORDING FOLDER IS NOT WRITABLE", "AUFNAHME-ORDNER NICHT BESCHREIBBAR" },
        { "RECORDING SAVED: %1", "AUFNAHME GESPEICHERT: %1" },
        { "Recording the livelooper output - click to stop. File: %1", "Nimmt die livelooper-Ausgabe auf - Klick stoppt. Datei: %1" },
        { "STEP %1 / %2", "SCHRITT %1 / %2" },
        { "Save set to share", "Set zum Teilen speichern" },
        { "Save set to share (.livelooper, one file with all audio) ...", "Set zum Teilen speichern (.livelooper, eine Datei mit allem Audio) ..." },
        { "Show recordings folder", "Aufnahme-Ordner zeigen" },
        { "Start performance recording", "Performance-Aufnahme starten" },
        { "Stop performance recording", "Performance-Aufnahme stoppen" },
        { "T%1 double speed", "S%1 doppelte Geschwindigkeit" },
        { "T%1 half speed", "S%1 halbe Geschwindigkeit" },
        { "T%1 loop reversed", "S%1 Loop umgedreht" },
        { "The set is incomplete: %1 is missing.", "Das Set ist unvollst\xc3\xa4" "ndig: %1 fehlt." },
        { "live loop performance", "Live-Loop-Performance" },
        { "Half Speed", "Halbe Geschwindigkeit" },
        { "Double Speed", "Doppelte Geschwindigkeit" },
        { "Reverse Loop Audio", "Loop-Audio umdrehen" },
        { "Performance Recorder", "Performance-Recorder" },
        { "Export 9:16 Clip", "9:16-Clip exportieren" },
        { "NEXT", "WEITER" },
        { "BACK", "ZUR\xc3\x9c" "CK" },
        { "SKIP TOUR", "TOUR \xc3\x9c" "BERSPRINGEN" },
        { "DONE", "FERTIG" },
        { "RECORD", "AUFNEHMEN" },
        { "OVERDUB", "OVERDUB" },
        { "GROOVE", "GROOVE" },
        { "SCENE", "SZENE" },
        { "Press REC/PLAY on a track and play something. Press it again: the loop closes and plays right away. The demo set already holds three loops - press ALL START / STOP to hear them.", "Dr\xc3\xbc" "ck REC/PLAY auf einer Spur und spiel etwas. Nochmal dr\xc3\xbc" "cken: Der Loop schlie\xc3\x9f" "t sich und l\xc3\xa4" "uft sofort. Das Demo-Set hat schon drei Loops - dr\xc3\xbc" "ck ALLE START / STOP, um sie zu h\xc3\xb6" "ren." },
        { "Press REC/PLAY while the loop plays to record on top of it. Press it again to stop overdubbing. UNDO removes the last layer, REDO brings it back.", "Dr\xc3\xbc" "ck REC/PLAY, w\xc3\xa4" "hrend der Loop l\xc3\xa4" "uft, um dar\xc3\xbc" "ber aufzunehmen. Nochmal dr\xc3\xbc" "cken beendet den Overdub. ZUR\xc3\x9c" "CK entfernt die letzte Schicht, VOR holt sie wieder." },
        { "RHYTHM starts a drum groove in time with your loops. Choose pattern and kit; SWING and HUMANIZE make it feel played.", "RHYTHMUS startet einen Drum-Groove im Takt deiner Loops. W\xc3\xa4" "hle Pattern und Kit; SWING und HUMANIZE machen ihn lebendig." },
        { "Pick a beat FX and press ON - or hold HOLD for a short effect. The FX channel decides where it works: master, loops, input or one track.", "W\xc3\xa4" "hle einen Beat-FX und dr\xc3\xbc" "ck ON - oder halte HOLD f\xc3\xbc" "r einen kurzen Effekt. Der FX-Kanal bestimmt, wo er wirkt: Master, Loops, Eingang oder eine Spur." },
        { "Scenes remember which tracks play. Press STORE, then a scene button. Later that button starts exactly those tracks, in time. Have fun - the tour is always in the help menu (?).", "Szenen merken sich, welche Spuren laufen. Dr\xc3\xbc" "ck STORE, dann eine Szenen-Taste. Sp\xc3\xa4" "ter startet diese Taste genau diese Spuren, im Takt. Viel Spa\xc3\x9f" " - die Tour findest du jederzeit im Hilfe-Men\xc3\xbc" " (?)." },
        { "Not enough audio for a clip yet.", "Noch nicht genug Audio f\xc3\xbc" "r einen Clip." },
        { "The video encoder of the system failed - writing PNG frames instead.", "Der Video-Encoder des Systems ist fehlgeschlagen - stattdessen werden PNG-Einzelbilder geschrieben." },
        { "Cancelled", "Abgebrochen" },
        // ---------------------------------------------------------------- end v1.6 features
    };

    // ---------------------------------------------------------------- v1.6 rhythm (branch v1.6-rhythm)
    // kits, BEATS step sequencer, fill on demand, drum FX routing, sound packs. Kept as its own
    // table so it merges cleanly with other v1.6 work.
    const char* const kGermanRhythm16[][2] = {
#include "Localisation_Rhythm16.inc"
    };

    const std::map<juce::String, juce::String>& table()
    {
        static const std::map<juce::String, juce::String> map = [] {
            std::map<juce::String, juce::String> m;
            for (const auto& entry : kGerman)
                m.emplace (juce::String::fromUTF8 (entry[0]), juce::String::fromUTF8 (entry[1]));
            for (const auto& entry : kGermanRhythm16)
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
juce::String upper (const juce::String& text)
{
    return text.toUpperCase()
               .replace (juce::CharPointer_UTF8 ("\xc3\xa4"), juce::CharPointer_UTF8 ("\xc3\x84"))
               .replace (juce::CharPointer_UTF8 ("\xc3\xb6"), juce::CharPointer_UTF8 ("\xc3\x96"))
               .replace (juce::CharPointer_UTF8 ("\xc3\xbc"), juce::CharPointer_UTF8 ("\xc3\x9c"));
}

} // namespace livelooper::i18n
