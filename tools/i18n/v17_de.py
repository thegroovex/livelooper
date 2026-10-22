#!/usr/bin/env python3
"""v1.7 latency measurement / compensation, velocity curves, generic MIDI feedback, Push 1 display texts, English -> German.
Writes src/plugin/Localisation_V17.inc (UTF-8 as escapes, safe for every compiler).
    python3 tools/i18n/v17_de.py
"""
import os

T = [
    # ---- parameters, choices, group
    ("Latency & Controllers", "Latenz & Controller"),
    ("Latency Compensation", "Latenzausgleich"), ("Latency Offset", "Latenz-Versatz"), ("Measure Latency", "Latenz messen"),
    ("Push 1 Velocity Curve", "Push-1-Anschlagkurve"), ("Sampler Pad Velocity Curve", "Sampler-Pad-Anschlagkurve"),
    ("MIDI Velocity Curve", "MIDI-Anschlagkurve"), ("MIDI Feedback (experimental)", "MIDI-Feedback (experimentell)"),
    ("Push 1 Loop Progress Bars", "Push 1 Loop-Fortschrittsbalken"),
    ("Soft", "Weich"), ("Normal", "Normal"), ("Hard", "Hart"), ("Fixed", "Fest"),
    # ---- SETUP > Audio > Latency
    ("Latency", "Latenz"),
    ("Measure latency ...", "Latenz messen ..."), ("Measuring ...", "Misst ..."),
    ("Measured: %1 ms (%2 samples)", "Gemessen: %1 ms (%2 Samples)"),
    ("Not measured yet for this audio device", "F\u00fcr dieses Audioger\u00e4t noch nicht gemessen"),
    ("Last measurement: no ping detected - connect an output to an input with a cable, or hold a microphone to the speakers, and turn the input up",
     "Letzte Messung: kein Ping erkannt \u2013 einen Ausgang per Kabel mit einem Eingang verbinden oder ein Mikrofon an die Lautsprecher halten und den Eingang aufdrehen"),
    ("Last measurement: the runs disagree (%1 ms apart) - use a cable instead of a microphone, close other audio apps",
     "Letzte Messung: die Durchl\u00e4ufe weichen ab (%1 ms) \u2013 Kabel statt Mikrofon verwenden, andere Audio-Apps schlie\u00dfen"),
    ("Plays a few clicks on the output and listens on the input - keep the cable / microphone in place",
     "Spielt einige Klicks auf dem Ausgang und h\u00f6rt auf den Eingang \u2013 Kabel / Mikrofon nicht bewegen"),
    ("Latency compensation (all recordings, overdubs and captures)", "Latenzausgleich (alle Aufnahmen, Overdubs und Captures)"),
    ("Manual offset", "Manueller Versatz"),
    ("Forget the measurement for this audio device", "Messung f\u00fcr dieses Audioger\u00e4t vergessen"),
    ("In use: %1 ms (%2 samples) - %3", "Aktiv: %1 ms (%2 Samples) \u2013 %3"),
    ("measured", "gemessen"), ("reported by the audio device", "vom Audioger\u00e4t gemeldet"),
    ("K.O. II latency offset", "K.O.-II-Latenzversatz"), ("nothing measured", "nichts gemessen"),
    ("Plug-in: hosts give plug-ins no audio device latency - measure inside the DAW (cable from an output to the track's input, input monitoring on, nothing else playing)",
     "Plug-in: Hosts geben Plug-ins keine Latenz des Audioger\u00e4ts \u2013 in der DAW messen (Kabel von einem Ausgang zum Eingang der Spur, Eingangsmonitor an, sonst nichts abspielen)"),
    ("Fine-tune with the manual offset or the host parameter \"Latency Offset\" (-50 .. +50 ms)",
     "Feinabstimmung mit dem manuellen Versatz oder dem Host-Parameter \u201eLatenz-Versatz\u201c (-50 .. +50 ms)"),
    ("Latency: %1 ms (%2 samples)", "Latenz: %1 ms (%2 Samples)"),
    ("Latency measurement unstable - not saved", "Latenzmessung unsicher \u2013 nicht gespeichert"),
    ("No ping detected - connect an output to an input (cable) or hold a microphone to the speakers",
     "Kein Ping erkannt \u2013 einen Ausgang mit einem Eingang verbinden (Kabel) oder ein Mikrofon an die Lautsprecher halten"),
    # ---- SETUP > Controllers
    ("Velocity curves", "Anschlagkurven"), ("Push 1", "Push 1"), ("K.O. II", "K.O. II"), ("MIDI (learned notes)", "MIDI (gelernte Noten)"),
    ("Soft: light hits sound louder. Hard: hit harder for full level. Fixed: every hit is full velocity.",
     "Weich: leichte Schl\u00e4ge klingen lauter. Hart: f\u00fcr vollen Pegel fester schlagen. Fest: jeder Schlag mit voller Anschlagst\u00e4rke."),
    ("Push 1 display", "Push-1-Display"), ("Loop progress bars on the display", "Loop-Fortschrittsbalken im Display"),
    ("Line 3: state letter and loop position per track. Punch FX / XY amount as a bar on line 4 while played.",
     "Zeile 3: Statusbuchstabe und Loop-Position pro Spur. Punch-FX- / XY-St\u00e4rke als Balken in Zeile 4, solange gespielt."),
    ("MIDI feedback (experimental)", "MIDI-Feedback (experimentell)"),
    ("Send LED feedback for learned controls", "LED-Feedback f\u00fcr gelernte Bedienelemente senden"),
    ("MIDI output", "MIDI-Ausgang"), ("None", "Keiner"),
    ("On value", "Wert für an"), ("Off value", "Wert für aus"), ("Blink value", "Wert für blinken"),
    ("Blink in time (livelooper switches on / off)", "Im Takt blinken (livelooper schaltet an / aus)"),
    ("Sends a note-on / CC with the learned channel and number: on = playing / switched on, blink = recording / waiting, off. Knobs learned on a CC get their value.",
     "Sendet Note-on / CC mit gelerntem Kanal und Nummer: an = spielt / eingeschaltet, blinken = nimmt auf / wartet, aus. Auf einen CC gelernte Regler bekommen ihren Wert."),
    ("Experimental \u2013 please test with your controller", "Experimentell \u2013 bitte mit deinem Controller testen"),
    # ---- v1.7 display: SETUP > Display, Live view, host parameters
    ("Display", "Anzeige"), ("Dark", "Dunkel"), ("Light", "Hell"),
    ("Colour theme", "Farbschema"), ("Interface size", "Oberfl\u00e4chengr\u00f6\u00dfe"),
    ("75 %, 100 %, 125 %, 150 % or 200 % - also Ctrl / Cmd + and -", "75 %, 100 %, 125 %, 150 % oder 200 % \u2013 auch Strg / Cmd + und -"),
    ("Stage mode (high contrast, larger text)", "B\u00fchnenmodus (hoher Kontrast, gr\u00f6\u00dfere Schrift)"),
    ("Show waveforms in the loop tracks", "Wellenformen in den Loop-Spuren zeigen"),
    ("Live view", "Live-Ansicht"), ("Open Live view (fullscreen)", "Live-Ansicht \u00f6ffnen (Vollbild)"),
    ("Waveforms in the track displays", "Wellenformen in den Spur-Displays"), ("Performance FX strip", "Performance-FX-Leiste"),
    ("Also with the LIVE button in the header, the host parameter \"Live View\" and on the controllers (see the docs)",
     "Auch mit der LIVE-Taste oben, dem Host-Parameter \u201eLive-Ansicht\u201c und an den Controllern (siehe Anleitung)"),
    ("Interface size and theme are remembered on this computer", "Gr\u00f6\u00dfe und Farbschema merkt sich livelooper auf diesem Computer"),
    ("Live View", "Live-Ansicht"), ("Stage Mode", "B\u00fchnenmodus"), ("Colour Theme", "Farbschema"), ("Show Waveforms", "Wellenformen zeigen"),
    ("Live view: large track pads for the stage (Esc returns)", "Live-Ansicht: gro\u00dfe Spur-Pads f\u00fcr die B\u00fchne (Esc f\u00fchrt zur\u00fcck)"),
    ("INTERFACE SIZE %1 %", "GR\u00d6SSE %1 %"),
    ("Esc or LIVE to return to the full surface", "Esc oder LIVE f\u00fchrt zur vollen Oberfl\u00e4che zur\u00fcck"),
    ("BAR . BEAT", "TAKT . SCHLAG"), ("PERFORMANCE FX", "PERFORMANCE-FX"), ("LEVEL", "PEGEL"), ("NO LOOP", "KEIN LOOP"),
    ("EXIT LIVE", "LIVE BEENDEN"), ("Back to the full surface (Esc)", "Zur\u00fcck zur vollen Oberfl\u00e4che (Esc)"),
    ("Live view on", "Live-Ansicht an"), ("Live view off", "Live-Ansicht aus"),
    ("Beat FX on / off (type and beat fraction as on the full surface)", "Beat-FX an / aus (Typ und Notenwert wie auf der vollen Oberfl\u00e4che)"),
    ("Beat repeat while held", "Beat Repeat solange gehalten"), ("Stutter while held", "Stutter solange gehalten"),
    ("Capture: the last bars you played become a loop on the next free track",
     "Capture: die zuletzt gespielten Takte werden ein Loop auf der n\u00e4chsten freien Spur"),
    ("FILTER", "FILTER"), ("PUNCH", "PUNCH"),
    # ---- v1.7 views (host parameters 502 - 503, Push SHIFT + MASTER)
    ("Views", "Ansichten"), ("Live View FX Bar", "Live-Ansicht FX-Leiste"), ("View", "Ansicht"),
    ("Full", "Voll"), ("Live", "Live"), ("Live without FX", "Live ohne FX"),
    ("Full view", "Volle Ansicht"), ("Live view without FX", "Live-Ansicht ohne FX"),
    ("Also with the LIVE button in the header, the host parameters \"View\", \"Live View\" and \"Live View FX Bar\" (MIDI learn) and SHIFT + MASTER on Push 1 (see the docs)",
     "Auch mit der LIVE-Taste oben, den Host-Parametern \u201eAnsicht\u201c, \u201eLive-Ansicht\u201c und \u201eLive-Ansicht FX-Leiste\u201c (MIDI-Learn) und SHIFT + MASTER am Push 1 (siehe Anleitung)"),
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
with open(os.path.join(root, 'src', 'plugin', 'Localisation_V17.inc'), 'w', encoding='utf-8') as f:
    f.write('// Generated by tools/i18n/v17_de.py - v1.7 latency, velocity curves, MIDI feedback, Push 1 display (English -> German).\n')
    for en, de in T:
        if en in seen:
            continue
        seen.add(en)
        f.write('        { "%s", "%s" },\n' % (esc(en), esc(de)))
print(len(seen), 'entries')
