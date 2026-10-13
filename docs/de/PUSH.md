# Steuerung mit Ableton Push

livelooper spricht **direkt** mit einem Ableton Push (1) über dessen **User-Port**, sowohl im Plug-in (VST3 / AU) als auch in der Standalone-App. Die Pads leuchten mit Status und Loop-Position jeder Spur, das Display zeigt Namen, Werte und Tempo.

Mit einem Push wird livelooper zu einem eigenständigen Instrument: keine Maus, kein Bildschirm, nur Pads und Encoder – so, wie es live gespielt werden soll.

![livelooper auf Ableton Push](../images/livelooper-live-push.jpg)

Push 2 und Push 3 senden dieselben Pad- und Tasten-Nachrichten, die Bedienung sollte dort also auch funktionieren (noch nicht getestet). Die LED-Farben können dort abweichen, und ihr Display wird nicht genutzt.

## Einrichtung

**In Ableton Live**
1. Push wie gewohnt eingerichtet lassen (Bedienoberfläche „Push“ in Lives MIDI-Einstellungen).
2. livelooper laden. **SETUP → Ableton Push** zeigt „Verbunden“.
3. **USER** auf Push drücken: Push wechselt zu livelooper. Erneut **USER** drücken, und du bist zurück in Live.

**Windows:** Einen MIDI-Port kann immer nur ein Programm benutzen. Lass in Lives MIDI-Einstellungen den Ein- und Ausgang **„MIDIIN2 (Ableton Push)“ / „MIDIOUT2 (Ableton Push)“** ausgeschaltet (Spur, Sync und Fern aus). Sonst kann livelooper den Port nicht öffnen. Unter macOS ist das nicht nötig.

**Standalone-App:** Push anschließen und livelooper starten. Die App schaltet Push selbst in den User-Modus.

Push wird immer nur von einer livelooper-Instanz gesteuert. **SETUP → Ableton Push** schaltet die Steuerung ein oder aus.

## Belegung

### LOOP-Modus (Taste SESSION)

Jede der 8 Pad-Spalten ist eine Spur (bei 5 Spuren bleiben die Spalten 6 bis 8 dunkel).

| Pad-Reihe | Funktion | LED |
|---|---|---|
| 1 (unten) | REC / PLAY / DUB | rot = Aufnahme, orange = Overdub, grün = spielt, dunkelgrün = gestoppt, blinkend = wartet auf den Takt |
| 2 | STOP | gelb = spielt, dunkel = gestoppt |
| 3 – 8 | Loop-Position (Drücken: Spur auswählen) | die Abspielmarke läuft die Spalte hoch, gestoppte Spuren zeigen die Geister-Abspielmarke |

| Taste | Funktion |
|---|---|
| obere Reihe 1 – 8 | Spur-FX ein / aus |
| zweite Reihe 1 – 8 | Reverse ein / aus |
| Szenen-Tasten (rechts) | Szenen 1 – 8 (oben = 1). **SHIFT** + Szene = speichern |
| PLAY | alle starten / stoppen |
| STOP | alle stoppen |
| RECORD | Capture (die letzten Takte werden ein Loop) |
| METRONOME | Rhythmus ein / aus |
| TAP TEMPO | Tap-Tempo |
| UNDO | Undo der ausgewählten Spur (**SHIFT** = Redo) |
| DELETE + Pad | Spur löschen |
| MUTE + Pad | Spur stoppen |
| SHIFT + Reihe 1 / Reihe 2 | Undo / Redo dieser Spur |
| AUTOMATION | Auto-Aufnahme ein / aus |
| NEW | Speichermodus für Szenen |
| QUANTIZE | nächster Quantisierungswert |

### FX-PAD-Modus (Taste NOTE)

Die 8 × 8 Pads werden ein **XY-Pad** für einen Effekt: X (links nach rechts) = Parameter 1, Y (unten nach oben) = Parameter 2. Berühren schaltet den Effekt ein. Beim Loslassen **springt er zurück** auf die vorherigen Einstellungen. Halte beim Loslassen **SHIFT**, um den Effekt so zu behalten. Die oberen Tasten 1 – 4 wählen Eingangs-FX A – D, die Tasten 5 – 8 Spur-FX A – D. **SESSION** führt zurück in den LOOP-Modus.

### Encoder

| Modustaste | Encoder 1 – 8 |
|---|---|
| VOLUME | Pegel der Spuren 1 – 8 |
| PAN & SEND | Panorama |
| TRACK | Tonhöhe (Halbtöne) |
| CLIP | Feedback |
| DEVICE | Parameter 1 der Eingangs-FX A – D (1 – 4) und der Spur-FX A – D (5 – 8). **SHIFT** = Mix |

Master-Encoder = Gesamtpegel · Tempo-Encoder = Tempo (1 BPM, mit **SHIFT** 0,1 BPM). Mit **SHIFT** wird fein eingestellt.

**Master-Sektion**: Bei bis zu 6 sichtbaren Spuren sind die beiden Encoder ganz rechts eine Master-Sektion: **Encoder 7 = Gesamtpegel**, **Encoder 8 = Tempo** (BPM, mit **SHIFT** 0,1 BPM); das Display zeigt darüber *Master* und *Tempo*. Bei 8 Spuren (und im DEVICE-Modus) steuern alle acht Encoder Spuren bzw. Effekte; dann gelten der Master-Encoder (oben rechts) und der Tempo-Encoder (oben links), und Zeile 4 zeigt den Gesamtpegel. Mit **HOST SYNC** bestimmt Live das Tempo (Display: *Host*); das interne Tempo gilt nur ohne Host-Sync.

Swing-Encoder = **Swing** des Rhythmus, **SHIFT** + Swing-Encoder = Rhythmus-Pegel.

### Display

Zeile 1: was die Encoder steuern · Zeile 2: ihre Werte · Zeile 3: Status der Spuren (`>` = ausgewählte Spur) · Zeile 4: Tempo, letzte Szene, Auto-Aufnahme, XY-Effekt.
