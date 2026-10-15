#!/usr/bin/env python3
"""v1.6 UI polish texts (help guide, SETUP window, FX tabs ...) English -> German.
Writes src/plugin/Localisation_UI16b.inc (UTF-8 as escapes, safe for every compiler).
    python3 tools/i18n/ui16b_de.py
"""
import os

T = [
    # ---- BEATS header groups
    ("PATTERN", "PATTERN"), ("SWITCH", "WECHSEL"), ("GRID", "RASTER"), ("KIT", "KIT"), ("FILE", "DATEI"), ("EDIT", "BEARBEITEN"), ("BAR", "TAKT"),

    # ---- SETUP window, FX tabs
    ("Sets", "Sets"),
    ("Looper", "Looper"),
    ("Rhythm", "Rhythmus"),
    ("Audio", "Audio"),
    ("Controllers", "Controller"),
    ("Sync", "Sync"),
    ("App", "App"),
    ("General", "Allgemein"),
    ("Choose ...", "Auswählen …"),
    ("Track meters", "Pegelanzeigen pro Spur"),
    ("REPEAT / STUTTER", "REPEAT / STUTTER"),
    ("COLOR FX", "COLOR-FX"),
    ("BEAT FX", "BEAT-FX"),
    ("Show this group of effects", "Diese Effektgruppe zeigen"),
    ("Turn COLOR on a track to blend in the chosen effect - centre = off.", "Dreh COLOR an einer Spur, um den gewählten Effekt einzublenden – Mitte = aus."),

    # ---- help guide
    ("Guide", "Einstieg"),
    ("Welcome", "Willkommen"),
    ("Loops", "Loops"),
    ("FX", "FX"),
    ("BEATS", "BEATS"),
    ("Recording", "Aufnahme"),
    ("Link", "Link"),
    ("Push 1", "Push 1"),
    ("Push 1: STEP", "Push 1: STEP"),
    ("livelooper is a loop station you play like an instrument.", "livelooper ist eine Loop-Station, die du wie ein Instrument spielst."),
    ("Record a phrase - it repeats as a loop. Add layers on top.", "Nimm eine Phrase auf – sie wiederholt sich als Loop. Leg weitere Schichten darüber."),
    ("Shape the loops with FX, a drum groove and scenes.", "Forme die Loops mit Effekten, einem Drum-Groove und Szenen."),
    ("Every control reacts at once and lands on the beat.", "Jedes Bedienelement reagiert sofort und landet auf dem Schlag."),
    ("Press the big pad of a track to record.", "Drück das große Pad einer Spur, um aufzunehmen."),
    ("Press it again: the loop plays. Once more: record a new layer (overdub).", "Noch einmal: Der Loop spielt. Ein weiteres Mal: eine neue Schicht aufnehmen (Overdub)."),
    ("STOP stops the track. Hold STOP to clear it.", "STOP hält die Spur an. STOP halten löscht sie."),
    ("Drop audio files on a track to import them. Drag the waveform out to export it.", "Zieh Audiodateien auf eine Spur, um sie zu importieren. Zieh die Wellenform heraus, um sie zu exportieren."),
    ("INPUT FX shape what you play in. TRACK FX shape the loops with FX on.", "EINGANGS-FX formen, was du einspielst. SPUR-FX formen die Loops mit eingeschaltetem FX."),
    ("Switch a slot A - D on and click its name to edit it.", "Schalte einen Slot A – D ein und klick auf seinen Namen, um ihn zu bearbeiten."),
    ("BEAT FX, BEAT REPEAT / STUTTER and COLOR FX are made for quick live moves.", "BEAT-FX, BEAT REPEAT / STUTTER und COLOR-FX sind für schnelle Live-Moves gemacht."),
    ("Every FX switch and knob can be mapped to MIDI.", "Jeder FX-Schalter und jeder Regler lässt sich per MIDI steuern."),
    ("BEATS opens the drum grid: eight rows, up to four bars.", "BEATS öffnet das Drum-Raster: acht Reihen, bis zu vier Takte."),
    ("Click steps to build a groove. Right-click a step for softer or harder hits.", "Klick Schritte an, um einen Groove zu bauen. Rechtsklick auf einen Schritt: leiser oder lauter."),
    ("Choose kit, pattern, length and swing at the top.", "Kit, Pattern, Länge und Swing wählst du oben."),
    ("SAVE keeps your pattern. Drop your own sample on a row.", "SPEICHERN behält dein Pattern. Zieh ein eigenes Sample auf eine Reihe."),
    ("The round REC button records everything livelooper plays as a WAV file.", "Der runde REC-Knopf nimmt alles, was livelooper spielt, als WAV-Datei auf."),
    ("It blinks with the beat while it records. Click again to stop.", "Während der Aufnahme blinkt er im Takt. Noch ein Klick stoppt."),
    ("SETUP shows the recordings folder and turns a recording into a 9:16 video clip.", "SETUP zeigt den Aufnahmeordner und macht aus einer Aufnahme einen 9:16-Videoclip."),
    ("Ableton Link keeps tempo and beat in time with other apps and devices on your network.", "Ableton Link hält Tempo und Takt mit anderen Apps und Geräten in deinem Netzwerk im Gleichschritt."),
    ("Switch it on in SETUP > Sync. The display shows how many others are connected.", "Einschalten unter SETUP > Sync. Das Display zeigt, wie viele andere verbunden sind."),
    ("In a DAW, HOST SYNC comes first - switch it off to follow Link.", "In einer DAW hat HOST-SYNC Vorrang – schalte es aus, um Link zu folgen."),
    ("Connect Push and press USER: livelooper takes over the pads, buttons and display.", "Push anschließen und USER drücken: livelooper übernimmt Pads, Tasten und Display."),
    ("Each pad column is one track.", "Jede Pad-Spalte ist eine Spur."),
    ("Bottom pad row: REC / PLAY / DUB", "Unterste Pad-Reihe: REC / PLAY / DUB"),
    ("Second pad row: STOP", "Zweite Pad-Reihe: STOP"),
    ("Pads 3 - 8: loop position - tap to jump, hold to stutter", "Pads 3 – 8: Loop-Position – tippen springt, halten stottert"),
    ("Top button row: track FX on / off", "Obere Tastenreihe: Spur-FX an / aus"),
    ("Second button row: reverse - SHIFT: reverse the loop audio", "Zweite Tastenreihe: rückwärts – SHIFT: Loop-Audio umdrehen"),
    ("Touch strip: tape stop - SHIFT: filter", "Touch-Strip: Tape-Stop – SHIFT: Filter"),
    ("Scene buttons: scenes 1 - 8 - SHIFT: store", "Szenen-Tasten: Szenen 1 – 8 – SHIFT: speichern"),
    ("PLAY: all start / stop", "PLAY: alle Start / Stop"),
    ("RECORD: capture the last bars as a loop", "RECORD: die letzten Takte als Loop übernehmen"),
    ("UNDO - SHIFT: redo", "UNDO – SHIFT: Wiederholen"),
    ("METRONOME: rhythm on / off - SHIFT: Link", "METRONOME: Rhythmus an / aus – SHIFT: Link"),
    ("SCALES: STEP sequencer", "SCALES: STEP-Sequencer"),
    ("SHIFT: second function of a button", "SHIFT: Zweitfunktion einer Taste"),
    ("REPEAT / ACCENT: beat repeat / stutter while held", "REPEAT / ACCENT: Beat Repeat / Stutter, solange gehalten"),
    ("MUTE / SOLO + pad: mute or solo while held", "MUTE / SOLO + Pad: stumm oder solo, solange gehalten"),
    ("IN: fill - OUT: drums through the beat FX", "IN: Fill – OUT: Drums durch die Beat-FX"),
    ("Encoders: level, pan, pitch, FX (mode buttons)", "Encoder: Pegel, Panorama, Tonhöhe, FX (Modus-Tasten)"),
    ("Display: names, values, tempo and meters", "Display: Namen, Werte, Tempo und Pegel"),
    ("OCTAVE: pitch - SHIFT: half / double speed", "OCTAVE: Tonhöhe – SHIFT: halbe / doppelte Geschwindigkeit"),
    ("SESSION: loop mode - NOTE: XY FX pad", "SESSION: Loop-Modus – NOTE: XY-FX-Pad"),
    ("SCALES turns the pads into a step sequencer for the drums.", "SCALES macht aus den Pads einen Step-Sequencer für die Drums."),
    ("Pad rows are the drum voices: kick at the bottom, tom on top", "Pad-Reihen sind die Drum-Stimmen: Kick unten, Tom oben"),
    ("Columns are steps - the green / white column plays now", "Spalten sind Schritte – die grüne / weiße Spalte spielt gerade"),
    ("Top buttons: row mute", "Obere Tasten: Reihe stumm"),
    ("Lower buttons: row solo", "Untere Tasten: Reihe solo"),
    ("SCALES: STEP mode on / off", "SCALES: STEP-Modus an / aus"),
    ("LEFT / RIGHT: previous / next 8 steps", "LINKS / RECHTS: vorherige / nächste 8 Schritte"),
    ("IN: fill while held", "IN: Fill, solange gehalten"),
    ("OUT: drums through the beat FX", "OUT: Drums durch die Beat-FX"),
    ("Encoders: row level - SHIFT: pitch", "Encoder: Pegel der Reihe – SHIFT: Tonhöhe"),
    ("SHIFT + pad: accent", "SHIFT + Pad: Akzent"),
]

def esc(s):
    out = ''
    for ch in s:
        if ord(ch) < 128:
            out += '\\"' if ch == '"' else ch
        else:
            out += ''.join('\\x%02x' % b for b in ch.encode('utf-8')) + '" "'
    return out.replace('" ""', '"')

def main():
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
    seen = set()
    lines = ['// Generated by tools/i18n/ui16b_de.py - v1.6 UI polish texts (English -> German).']
    for en, de in T:
        if en in seen or en == de:
            continue
        seen.add(en)
        lines.append('        { "%s", "%s" },' % (esc(en), esc(de)))
    with open(os.path.join(root, 'src', 'plugin', 'Localisation_UI16b.inc'), 'w') as f:
        f.write('\n'.join(lines) + '\n')

if __name__ == '__main__':
    main()
