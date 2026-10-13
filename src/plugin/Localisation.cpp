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
        { "Drag the waveform onto an audio track in Live to export the loop", "Wellenform auf eine Audiospur in Live ziehen, um den Loop zu exportieren" },

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
        { "In Live: press USER on Push to switch between Live and livelooper", "In Live: USER auf Push wechselt zwischen Live und livelooper" },
        { "Ableton Push", "Ableton Push" },
        { "Connected: %1", "Verbunden: %1" },
        { "Push found, but its User port is in use (Windows: switch it off in Live's MIDI preferences)", "Push gefunden, aber sein User-Port ist belegt (Windows: in Lives MIDI-Einstellungen ausschalten)" },
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
