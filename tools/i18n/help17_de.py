#!/usr/bin/env python3
"""v1.7 in-app help: illustrated guide pages (loop length, Live view, display, latency, controllers),
the new Push 1 help line and a v1.6 tooltip that had no German text, English -> German.
Writes src/plugin/Localisation_Help17.inc (UTF-8 as escapes, safe for every compiler).
    python3 tools/i18n/help17_de.py
"""
import os

T = [
    # ---- page titles ("Display", "Live view", "Latency", "Controllers", "Loop length" exist already)
    # ---- Loop length
    ("Every track has a loop length pill next to its name: AUTO or 1 - 16 bars.",
     "Jede Spur hat neben ihrem Namen eine Loop-Länge-Pille: AUTO oder 1 – 16 Takte."),
    ("AUTO: your next press ends the first recording. 1 - 16 BARS: the recording ends by itself on the bar line.",
     "AUTO: dein nächster Druck beendet die erste Aufnahme. 1 – 16 TAKTE: die Aufnahme endet von selbst auf dem Taktstrich."),
    ("Click the pill to choose, scroll or drag to change it, right-click for MIDI learn.",
     "Klick auf die Pille zum Wählen, Scrollen oder Ziehen zum Ändern, Rechtsklick für MIDI-Learn."),
    ("Dimmed while the track holds a loop: the length applies to the next new recording (after CLEAR).",
     "Gedimmt, solange die Spur einen Loop enthält: die Länge gilt für die nächste neue Aufnahme (nach LÖSCHEN)."),
    ("Push 1: hold FIXED LENGTH and press a pad in rows 3 - 8 (AUTO - 16 bars) or turn the track encoder. FIXED LENGTH alone sets the capture length.",
     "Push 1: FIXED LENGTH halten und ein Pad in Reihe 3 – 8 drücken (AUTO – 16 Takte) oder den Spur-Encoder drehen. FIXED LENGTH allein stellt die Capture-Länge ein."),
    ("Just once (up to 32 bars): Loop Length Once - on Push hold the REC pad and press a position pad.",
     "Nur einmal (bis 32 Takte): Loop-Länge einmalig – am Push das REC-Pad halten und ein Positions-Pad drücken."),
    # ---- Live view
    ("LIVE next to the logo opens the Live view: one large card per track, made for the stage.",
     "LIVE neben dem Logo öffnet die Live-Ansicht: eine große Karte pro Spur, gemacht für die Bühne."),
    ("Each card: state, length, waveform, the big REC / PLAY / DUB pad, level and STOP.",
     "Jede Karte: Status, Länge, Wellenform, das große REC / PLAY / DUB-Pad, Pegel und STOP."),
    ("The performance FX strip: filter, beat FX, punch, master, echo, repeat, stutter, fill, ALL START / STOP and capture.",
     "Die Performance-FX-Leiste: Filter, Beat-FX, Punch, Master, Echo, Repeat, Stutter, Fill, ALLE START / STOP und Capture."),
    ("SHIFT + MASTER on Push 1 cycles Full - Live - Live without FX. MIDI learn: host parameters View, Live View, Live View FX Bar. Esc or EXIT LIVE returns.",
     "SHIFT + MASTER am Push 1 schaltet Voll – Live – Live ohne FX weiter. MIDI-Learn: Host-Parameter Ansicht, Live-Ansicht, Live-Ansicht FX-Leiste. Esc oder LIVE BEENDEN führt zurück."),
    ("SETUP > Display > Open Live view (fullscreen): a borderless window kept on top over the whole screen - handy on a second monitor.",
     "SETUP > Anzeige > Live-Ansicht öffnen (Vollbild): ein randloses Fenster, immer im Vordergrund, über den ganzen Bildschirm – praktisch auf einem zweiten Monitor."),
    # ---- Display
    ("SETUP > Display: dark or light theme, interface size 75 - 200 %, stage mode and waveforms.",
     "SETUP > Anzeige: dunkles oder helles Farbschema, Oberflächengröße 75 – 200 %, Bühnenmodus und Wellenformen."),
    ("Ctrl + / Ctrl - changes the size (Mac: Cmd), Ctrl 0 = 100 %.",
     "Strg + / Strg − ändert die Größe (Mac: Cmd), Strg 0 = 100 %."),
    ("Stage mode: high contrast and larger text for dark stages.",
     "Bühnenmodus: hoher Kontrast und größere Schrift für dunkle Bühnen."),
    ("Waveforms in the loop tracks are off by default - switch them on for large waveforms with the overdub layer.",
     "Wellenformen in den Loop-Spuren sind anfangs aus – schalte sie ein für große Wellenformen mit der Overdub-Schicht."),
    # ---- Latency
    ("Every audio interface delays what you hear and what you record. livelooper measures it and puts every recording back on the beat.",
     "Jedes Audio-Interface verzögert, was du hörst und was du aufnimmst. livelooper misst das und setzt jede Aufnahme zurück auf den Schlag."),
    ("SETUP > Audio > Latency > Measure latency: connect an output to an input with a cable, or hold a microphone to the speaker.",
     "SETUP > Audio > Latenz > Latenz messen: einen Ausgang mit einem Kabel an einen Eingang anschließen oder ein Mikrofon an den Lautsprecher halten."),
    ("The result is stored per audio device. Compensation is on by default; fine-tune it with the manual offset.",
     "Das Ergebnis wird pro Audiogerät gespeichert. Der Ausgleich ist standardmäßig an; fein einstellen mit dem manuellen Versatz."),
    ("In a DAW: measure inside the DAW, on the looper track with input monitoring on.",
     "In einer DAW: in der DAW messen, auf der Looper-Spur mit eingeschaltetem Input-Monitoring."),
    ("Follow host transport is on by default: stopping your DAW stops the loops (SETUP > Looper).",
     "Host-Transport folgen ist standardmäßig an: stoppt deine DAW, stoppen auch die Loops (SETUP > Looper)."),
    # ---- Controllers
    ("Velocity curves: soft, normal, hard or fixed - separately for Push 1, K.O. II and learned MIDI notes.",
     "Anschlagkurven: weich, normal, hart oder fest – getrennt für Push 1, K.O. II und gelernte MIDI-Noten."),
    ("MIDI feedback (experimental): learned controls light up - livelooper sends their state to a MIDI output you choose.",
     "MIDI-Feedback (experimentell): gelernte Bedienelemente leuchten – livelooper sendet ihren Zustand an einen MIDI-Ausgang deiner Wahl."),
    ("Push 1 display: a loop progress bar per track and the amount bar of the held punch FX.",
     "Push-1-Display: ein Loop-Fortschrittsbalken pro Spur und der Stärke-Balken des gehaltenen Punch-FX."),
    ("Everything is in SETUP > Controllers. Every function can be MIDI-learned with a right-click.",
     "Alles unter SETUP > Controller. Jede Funktion lässt sich per Rechtsklick per MIDI-Learn zuweisen."),
    # ---- Push 1 page
    ("SHIFT + MASTER: cycles the views on the computer (Full, Live, Live without FX). Hold FIXED LENGTH + pad or encoder: loop length of a track.",
     "SHIFT + MASTER: schaltet die Ansichten am Computer weiter (Voll, Live, Live ohne FX). FIXED LENGTH halten + Pad oder Encoder: Loop-Länge einer Spur."),
    # ---- v1.6 tooltip without German text (Controller view XY pad)
    ("XY: hold to play the selected punch FX, move for X / Y. Right-click: MIDI learn of X",
     "XY: halten spielt den gewählten Punch-FX, bewegen für X / Y. Rechtsklick: MIDI-Learn von X"),
]

def esc(s):
    out = ''
    for b in s.encode('utf-8'):
        if b < 0x80 and chr(b) not in '"\\':
            out += chr(b)
        elif chr(b) in '"\\':
            out += '\\' + chr(b)
        else:
            out += '\\x%02x" "' % b
    return out.replace('" ""', '"').replace('" "', '" "')

root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
seen = set()
with open(os.path.join(root, 'src', 'plugin', 'Localisation_Help17.inc'), 'w', encoding='utf-8') as f:
    f.write('// Generated by tools/i18n/help17_de.py - v1.7 in-app help pages (English -> German).\n')
    for en, de in T:
        if en in seen:
            continue
        seen.add(en)
        f.write('        { "%s", "%s" },\n' % (esc(en), esc(de)))
print(len(seen), 'entries')
