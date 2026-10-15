#!/usr/bin/env python3
"""v1.6 K.O. II / punch FX / send FX / Push 1 additions / Controller GUI texts, English -> German.
Writes src/plugin/Localisation_KO2.inc (UTF-8 as escapes, safe for every compiler).
    python3 tools/i18n/ko2_de.py
"""
import os

T = [
    # v1.6 Controller GUI (Push 1 / K.O. II drawn from photos of the real devices)
    ("Hatched: sends no MIDI - not used by livelooper", "Schraffiert: sendet kein MIDI \u2013 von livelooper nicht genutzt"),
    ("BEAT - / +", "BEAT - / +"),
    ("PATTERN - / +", "PATTERN - / +"),
    ("BEAT -", "BEAT -"), ("BEAT +", "BEAT +"), ("PATTERN -", "PATTERN -"), ("PATTERN +", "PATTERN +"),
    ("PITCH -", "TON -"), ("PITCH +", "TON +"),
    ("BFX ON", "BFX AN"), ("BFX HOLD", "BFX HALTEN"),
    ("REPEAT", "REPEAT"), ("STUTTER", "STUTTER"), ("MUTE", "STUMM"), ("SOLO", "SOLO"), ("SELECT", "AUSWAHL"),
    ("USER", "USER"), ("ALL START", "ALLE START"), ("TAPE STOP", "BANDSTOPP"),
    # ---- parameters, choices
    ("Punch FX", "Punch-FX"), ("Controller", "Controller"),
    ("Send FX Type", "Send-FX-Typ"), ("Send FX X", "Send-FX X"), ("Send FX Y", "Send-FX Y"), ("Send FX Amount", "Send-FX-Anteil"),
    ("Punch FX Amount", "Punch-FX-Stärke"), ("Punch FX X", "Punch-FX X"), ("Punch FX Y", "Punch-FX Y"),
    ("Punch FX Target", "Punch-FX-Ziel"), ("Punch FX Latch", "Punch-FX halten"), ("Controller Audio", "Controller-Audio"),
    ("Resample Controller Pattern", "Controller-Pattern resamplen"), ("Punch FX %1 (Hold)", "Punch-FX %1 (halten)"),
    ("Distortion", "Verzerrung"), ("Chorus", "Chorus"), ("Compressor", "Kompressor"), ("Filter", "Filter"),
    ("Loops", "Loops"), ("Drums", "Drums"), ("Off", "Aus"),
    ("Controller only", "Nur Controller"), ("Record controller audio", "Controller-Audio aufnehmen"),
    # ---- punch FX names and their X / Y
    ("Pitch Scatter", "Tonhöhen-Zufall"), ("Slice Swap", "Slice-Tausch"), ("Grains", "Granular"), ("Beat Repeat", "Beat-Repeat"),
    ("Tape Stop", "Bandstopp"), ("Filter Wobble", "Filter-Wobble"), ("Low-Pass", "Tiefpass"), ("High-Pass", "Hochpass"),
    ("Send Throw", "Send-Wurf"), ("Tremolo", "Tremolo"), ("Octave Drop", "Oktave runter"), ("Decimator", "Dezimierer"),
    ("Slice size", "Slice-Größe"), ("Density", "Dichte"), ("Gate", "Gate"), ("Stop time", "Stoppzeit"), ("Resonance", "Resonanz"),
    ("Probability", "Wahrscheinlichkeit"), ("Source", "Quelle"), ("Spray", "Streuung"), ("Decay", "Abklingen"),
    ("Restart", "Anlauf"), ("Shape", "Form"), ("Interval", "Intervall"), ("Glide", "Gleiten"), ("Sample rate", "Samplerate"),
    ("Length", "Länge"), ("Color", "Farbe"), ("Modulation", "Modulation"), ("Speed", "Geschwindigkeit"),
    # ---- processor notices
    ("K.O. II connected", "K.O. II verbunden"), ("K.O. II disconnected", "K.O. II getrennt"), ("No free track", "Keine freie Spur"),
    ("Resample into track %1", "Resample in Spur %1"),
    ("K.O. II in KEYS mode - press KEYS to leave", "K.O. II im KEYS-Modus – KEYS drücken zum Verlassen"),
    # ---- Push 1
    ("Set recording on", "Set-Aufnahme an"), ("Set recording off", "Set-Aufnahme aus"),
    ("Encoders: Volume", "Encoder: Lautstärke"), ("Encoders: Pan", "Encoder: Panorama"), ("Encoders: Pitch", "Encoder: Tonhöhe"),
    ("Encoders: Feedback", "Encoder: Feedback"), ("Encoders: FX", "Encoder: FX"), ("Encoders: Beat FX", "Encoder: Beat-FX"),
    ("Encoders: Track colour", "Encoder: Spurfarbe"),
    # ---- Controller GUI
    ("REC SET", "SET-REC"), ("RESAMPLE", "RESAMPLE"), ("ROLE (+ENTER)", "ROLLE (+ENTER)"), ("no clock", "keine Clock"),
    ("sends no MIDI - not used", "sendet kein MIDI – nicht belegt"), ("LOOPS", "LOOPS"), ("PUNCH FX", "PUNCH-FX"),
    ("SCENES", "SZENEN"), ("COMPACT", "KOMPAKT"),
    ("Group buttons switch the page on the K.O. II. Click a tab to look at another page.",
     "Die Gruppentasten wechseln die Seite am K.O. II. Klick auf einen Reiter, um eine andere Seite anzusehen."),
    ("BEAT", "BEAT"), ("\u25c0 Standard", "\u25c0 Standard"), ("Controller view", "Controller-Ansicht"),
    ("Show Push", "Push zeigen"), ("Show K.O. II", "K.O. II zeigen"),
    ("DELETE", "LÖSCHEN"), ("DOUBLE", "DOPPELN"), ("QUANTIZE", "QUANTIS."), ("LENGTH", "LÄNGE"), ("AUTO REC", "AUTO-REC"),
    ("DUPLICATE", "DUPLIZ."), ("METERS", "PEGEL"), ("VOLUME", "LAUTST."), ("PITCH", "TONHÖHE"), ("FEEDBACK", "FEEDBACK"),
    ("LOOP", "LOOP"), ("XY FX", "XY-FX"), ("STEP", "STEP"), ("SHIFT", "SHIFT"), ("ALL STOP", "ALLE STOP"), ("PLAY", "PLAY"),
    # ---- FX panel: PUNCH FX tab
    ("SEND FX", "SEND-FX"), ("SCATTER", "ZUFALL"), ("SWAP", "TAUSCH"), ("GRAINS", "GRANULAR"), ("TAPE STOP", "BANDSTOPP"),
    ("WOBBLE", "WOBBLE"), ("LOW-PASS", "TIEFPASS"), ("HIGH-PASS", "HOCHPASS"), ("SEND", "SEND"), ("TREMOLO", "TREMOLO"),
    ("OCT DROP", "OKT. RUNTER"), ("DECIMATE", "DEZIMIER."), ("Amount", "Stärke"), ("Send", "Send"),
    ("Punch FX %1: %2 - hold to play, right-click: MIDI learn", "Punch-FX %1: %2 – halten zum Spielen, Rechtsklick: MIDI-Learn"),
    ("Where the punch FX work: loops, input, master or drums", "Wo die Punch-FX wirken: Loops, Eingang, Master oder Drums"),
    ("Send FX on the loops: SEND = how much goes in, X / Y = its two knobs", "Send-FX auf den Loops: SEND = wie viel hineingeht, X / Y = seine zwei Regler"),
    ("Latch: a press switches a punch FX on, the next press off", "Latch: ein Druck schaltet einen Punch-FX ein, der nächste aus"),
    # ---- header chip, question, status
    ("Record", "Aufnahme"), ("K.O. II: role, resample, clock - click for options", "K.O. II: Rolle, Resample, Clock – Klick für Optionen"),
    ("Record K.O. II", "K.O. II aufnehmen"), ("Ignore", "Ignorieren"), ("Settings ...", "Einstellungen …"),
    ("K.O. II connected - how should livelooper use it?", "K.O. II erkannt – wie soll livelooper ihn nutzen?"),
    ("K.O. II: RECORDING INPUT", "K.O. II: AUFNAHME-EINGANG"), ("K.O. II: IGNORED", "K.O. II: IGNORIERT"),
    ("K.O. II: CONTROLLER ONLY", "K.O. II: NUR CONTROLLER"),
    ("IN THE PLUG-IN: CHOOSE THE K.O. II AS AUDIO INPUT IN YOUR DAW", "IM PLUG-IN: DEN K.O. II IN DER DAW ALS AUDIO-EINGANG WÄHLEN"),
    # ---- SETUP > Controllers
    ("Resample K.O. pattern", "K.O.-Pattern resamplen"), ("Send clock to K.O. II", "Clock an K.O. II senden"),
    ("Controller GUI", "Controller-Oberfläche"),
    ("Record K.O. II in the plug-in: choose 'EP-133 ...' as audio input in your DAW's audio settings and set livelooper's track input.",
     "K.O. II im Plug-in aufnehmen: In den Audio-Einstellungen der DAW 'EP-133 …' als Audio-Eingang wählen und in livelooper den Spur-Eingang setzen."),
    ("Control group %1", "Steuer-Gruppe %1"), ("Default control groups for the role", "Standard-Steuergruppen der Rolle"),
    ("Any", "Alle"), ("K.O. II MIDI channel", "K.O.-II-MIDI-Kanal"), ("Automatic (reported latency)", "Automatisch (gemeldete Latenz)"),
    ("Latency offset (recording)", "Latenz-Ausgleich (Aufnahme)"), ("Ask again when connected", "Beim Verbinden wieder fragen"),
    ("not confirmed", "nicht bestätigt"), ("No K.O. II found", "Kein K.O. II gefunden"),
    ("Experimental – please test (see the K.O. II page of the docs)", "Experimentell – bitte testen (siehe K.O.-II-Seite der Doku)"),
    ("On the K.O. II: SHIFT + ERASE, type 1 0 1, ENTER (clock in)", "Am K.O. II: SHIFT + ERASE, 1 0 1 eingeben, ENTER (Clock-Eingang)"),
    ("K.O. II", "K.O. II"), ("Standard", "Standard"), ("Controller view: K.O. II", "Controller-Ansicht: K.O. II"),
    ("Controller view: Push", "Controller-Ansicht: Push"), ("GUI", "Oberfläche"),
    ("No K.O. II audio device found - check the USB connection", "Kein K.O.-II-Audiogerät gefunden – USB-Verbindung prüfen"),
    ("K.O. II: back to %1", "K.O. II: zurück zu %1"),
    # ---- help guide
    ("Plug in the K.O. II: livelooper finds it and asks once how to use it - as controller only or to record it.",
     "Schließ den K.O. II an: livelooper findet ihn und fragt einmal, wie er genutzt werden soll – nur als Controller oder zum Aufnehmen."),
    ("Its four pad groups are four pages: A loops, B track edit, C punch FX, D scenes.",
     "Seine vier Pad-Gruppen sind vier Seiten: A Loops, B Spur bearbeiten, C Punch-FX, D Szenen."),
    ("Set the K.O. II to clock in (SHIFT + ERASE, 1 0 1, ENTER): livelooper starts it on the bar.",
     "Stell den K.O. II auf Clock-Eingang (SHIFT + ERASE, 1 0 1, ENTER): livelooper startet ihn auf dem Takt."),
    ("Experimental - please test and report.", "Experimentell – bitte testen und Rückmeldung geben."),
    ("Group A pads 1 - 8: REC / PLAY / DUB track 1 - 8", "Gruppe A, Pads 1 – 8: REC / PLAY / DUB Spur 1 – 8"),
    ("Pad 9: all start / stop - ENTER: capture", "Pad 9: alle Start / Stop – ENTER: Capture"),
    ("'.': stop (hold: clear) - '0': undo (hold: redo)", "'.': Stop (halten: löschen) – '0': Rückgängig (halten: Wiederholen)"),
    ("Group buttons A - D: the pages", "Gruppentasten A – D: die Seiten"),
    ("Group C: the 12 punch FX - hit harder for more", "Gruppe C: die 12 Punch-FX – fester anschlagen = stärker"),
    ("Group D: '.' + ENTER held 1 s = Controller only / Record K.O. II", "Gruppe D: '.' + ENTER 1 s halten = Nur Controller / K.O. II aufnehmen"),
    ("Hatched: knobs, fader and function buttons send no MIDI - not used", "Schraffiert: Regler, Fader und Funktionstasten senden kein MIDI - nicht genutzt"),
    ("Display: shows nothing from livelooper - the screen mirrors it", "Display: zeigt nichts von livelooper – der Bildschirm spiegelt ihn"),
    ("Top-right pad (5 tracks): flashes on every beat", "Pad oben rechts (5 Spuren): blinkt auf jedem Schlag"),
    ("Pad below it: set recording on / off - blinks while recording", "Pad darunter: Set-Aufnahme an / aus – blinkt während der Aufnahme"),
    ("Press a loop pad harder: that loop chops faster (1/4 to 1/32). Let go: normal playback.",
     "Drück ein Loop-Pad fester: dieser Loop wird schneller zerhackt (1/4 bis 1/32). Loslassen: normale Wiedergabe."),
    ("Double-touch an encoder: next page - volume, pan, pitch, feedback, track colour.",
     "Encoder zweimal berühren: nächste Seite – Lautstärke, Panorama, Tonhöhe, Feedback, Spurfarbe."),
    ("SHIFT + RECORD: set recording. PLAY blinks in time.", "SHIFT + RECORD: Set-Aufnahme. PLAY blinkt im Takt."),
    ("XY FX pad: SHIFT + top buttons pick beat FX, colour FX, repeat, stutter and the punch FX.",
     "XY-FX-Pad: SHIFT + obere Tasten wählen Beat-FX, Color-FX, Repeat, Stutter und die Punch-FX."),
    ("PUNCH FX: hold one of 12 quick effects - several at once combine. SEND FX adds echo, reverb and more to the loops.",
     "PUNCH-FX: halte einen von 12 schnellen Effekten – mehrere zugleich kombinieren sich. SEND-FX gibt den Loops Echo, Hall und mehr."),
    # v1.6 K.O. II: output stays on the computer in "Record K.O. II", hold to intensify (no pad pressure)
    ("Role", "Rolle"),
    ("Computer (default)", "Computer (Standard)"),
    ("K.O. II audio output", "K.O.-II-Audioausgang"),
    ("Hold to intensify (punch pads)", "Halten = st\u00e4rker (Punch-Pads)"),
    ("1 beat", "1 Schlag"), ("2 beats", "2 Schl\u00e4ge"),
    ("HOLD", "HALTEN"),
    ("Punch FX Hold Ramp", "Punch-FX Halte-Rampe"),
    ("K.O. II audio output: K.O. II sends livelooper's output (metronome, loops) into the K.O. II - it comes back on its input and is recorded again. Computer (default): only the K.O. II is recorded.",
     "K.O.-II-Audioausgang: K.O. II schickt die Ausgabe von livelooper (Metronom, Loops) in den K.O. II \u2013 sie kommt \u00fcber seinen Eingang zur\u00fcck und wird mit aufgenommen. Computer (Standard): nur der K.O. II wird aufgenommen."),
    ("The K.O. II sends no pad pressure: hold a punch pad and the amount rises from the hit to 100 %.",
     "Der K.O. II sendet keinen Pad-Druck: Punch-Pad halten, und die St\u00e4rke steigt vom Anschlag bis 100 %."),
    ("In the plug-in: the K.O. II as the track's audio input in the DAW, the DAW's output on the computer / interface, input monitoring off (Help > K.O. II).",
     "Im Plug-in: den K.O. II als Audio-Eingang der Spur in der DAW w\u00e4hlen, die DAW-Ausgabe auf Computer / Interface, Eingangs-Monitoring aus (Hilfe > K.O. II)."),
    ("K.O. II AUDIO OUTPUT: K.O. II - METRONOME AND LOOPS ARE RECORDED AGAIN", "K.O.-II-AUSGANG: K.O. II \u2013 METRONOM UND LOOPS WERDEN MIT AUFGENOMMEN"),
    ("K.O. II AUDIO OUTPUT: COMPUTER", "K.O.-II-AUSGANG: COMPUTER"),
    ("K.O. II: no other audio output found - metronome and loops are silent", "K.O. II: kein anderer Audioausgang gefunden \u2013 Metronom und Loops sind stumm"),
    ("K.O. II: output to the K.O. II - metronome and loops are recorded again", "K.O. II: Ausgabe zum K.O. II \u2013 Metronom und Loops werden mit aufgenommen"),
    ("K.O. II: input from the K.O. II, output on %1", "K.O. II: Eingang vom K.O. II, Ausgabe auf %1"),
    ("Record K.O. II: the K.O. II is the input, the output stays on the computer - metronome and loops are not recorded again.",
     "K.O. II aufnehmen: der K.O. II ist der Eingang, der Ausgang bleibt am Computer \u2013 Metronom und Loops werden nicht noch einmal aufgenommen."),
    ("In the plug-in: the K.O. II as the track's audio input in the DAW, the DAW's output on the computer / interface, input monitoring off.",
     "Im Plug-in: der K.O. II als Audio-Eingang der Spur in der DAW, die DAW-Ausgabe auf Computer / Interface, Eingangs-Monitoring aus."),
    ("The K.O. II sends no pad pressure: hold a punch pad (group C) and the amount rises to 100 % (SETUP > Controllers).",
     "Der K.O. II sendet keinen Pad-Druck: ein Punch-Pad (Gruppe C) halten, und die St\u00e4rke steigt bis 100 % (SETUP > Controller)."),
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
with open(os.path.join(root, 'src', 'plugin', 'Localisation_KO2.inc'), 'w', encoding='utf-8') as f:
    f.write('// Generated by tools/i18n/ko2_de.py - v1.6 K.O. II / punch FX / Push 1 additions (English -> German).\n')
    for en, de in T:
        if en in seen:
            continue
        seen.add(en)
        f.write('        { "%s", "%s" },\n' % (esc(en), esc(de)))
print(len(seen), 'entries')
