# Steuerung mit Ableton Push

livelooper spricht **direkt** mit einem Ableton Push (1) über dessen **User-Port**, sowohl im Plug-in (VST3 / AU) als auch in der Standalone-App. Die Pads leuchten mit Status und Loop-Position jeder Spur, das Display zeigt Namen, Werte und Tempo.

Mit einem Push wird livelooper zu einem eigenständigen Instrument: keine Maus, kein Bildschirm, nur Pads und Encoder – so, wie es live gespielt werden soll.

![livelooper auf Ableton Push](../images/livelooper-live-push.jpg)

Push 2 und Push 3 senden dieselben Pad- und Tasten-Nachrichten, die Bedienung sollte dort also auch funktionieren (noch nicht getestet). Die LED-Farben können dort abweichen, und ihr Display wird nicht genutzt. Touchstrip, Fußschalter und Display-Funktionen sind für Push 1 gemacht.

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
| 3 – 8 | Loop-Position, **Loop-Cutting** (siehe unten) | die Abspielmarke läuft die Spalte hoch (cyan = geschnitten, dunkelrot = stumm); gestoppte Spuren zeigen die Geister-Abspielmarke |

#### Loop-Cutting auf den Positions-Pads

Die sechs Positions-Pads einer Spalte sind die sechs Sechstel des Loops dieser Spur.

| Geste | Spielende Spur | Gestoppte / leere Spur |
|---|---|---|
| Positions-Pad **antippen** | die Wiedergabe **springt** zur nächsten 16tel-Note in dieses Sechstel und spielt von dort weiter | Spur auswählen |
| Positions-Pad **halten** (> ¼ s) | das Stück **stottert** (wiederholt eine ¼-, ⅛- oder 1/16-Note); **Loslassen** springt dorthin zurück, wo der Loop jetzt wäre – wieder synchron, als wäre nie gesprungen worden | Spur auswählen |
| **SHIFT** + Positions-Pad | zurück in Sync (beendet einen Schnitt) | – |
| **SELECT** + beliebiges Pad | wählt die Spur aus, sonst nichts | Spur auswählen |
| **REC-Pad** einer leeren / aufnehmenden Spur halten + Positions-Pad 1 – 6 | – | feste Länge für diese Aufnahme: **1, 2, 4, 8, 16, 32 Takte** (von unten nach oben); während der Aufnahme füllt sich die Spalte als Fortschrittsbalken |

Die Anfänge der Stücke liegen auf 16tel-Noten des Loops, ein Schnitt bleibt also im Raster. Jeder Sprung und jede Rückkehr wird übergeblendet (3 ms) – kein Knacken. Mit Quantisierung *Aus* wird sofort gesprungen. Eine gestoppte und neu gestartete Spur startet immer wieder synchron.

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
| MUTE + Pad | **antippen**: Spur stoppen (wie bisher) · **halten** (> ¼ s): die Spur ist **nur stumm, solange das Pad gehalten wird** (sie läuft weiter: ein Drop) |
| SOLO + Pad | die Spur ist **solo, solange das Pad gehalten wird** (die anderen sind sofort stumm, ohne Quantisierung) |
| DUPLICATE + Pad + Pad | kopiert den Loop der ersten Spur in die zweite (leere) Spur. DUPLICATE + ein Pad (oder DUPLICATE allein: die ausgewählte Spur) kopiert in die nächste leere Spur. Die Kopie ist gestoppt und startet synchron |
| DOUBLE | **Loop × 2** der ausgewählten Spur (der Loop wird verdoppelt, z. B. um über die zweite Hälfte eine Variation zu overdubben). **SHIFT** + DOUBLE = Loop halbieren |
| OCTAVE UP / DOWN | Tonhöhe der ausgewählten Spur +12 / −12 Halbtöne |
| MASTER | Pegelanzeige ein / aus (Display-Zeile 3) |
| SELECT + Pad | Spur auswählen, ohne etwas anderes auszulösen |
| SHIFT + Reihe 1 / Reihe 2 | Undo / Redo dieser Spur |
| AUTOMATION | Auto-Aufnahme ein / aus |
| NEW | Speichermodus für Szenen |
| QUANTIZE | nächster Quantisierungswert |

### Touchstrip

| Geste | Funktion |
|---|---|
| Strip berühren, nach unten ziehen | **Tape-Stopp**: Die Loops werden langsamer wie ein anhaltendes Tonband (oben = normale Geschwindigkeit, unten = Stillstand). **Loslassen**: Das Band läuft wieder an und landet genau dort, wo die Musik jetzt wäre – die Zeitachse hält nie an |
| **SHIFT** + berühren, ziehen | **Performance-Filter** auf den Loops: untere Hälfte = Tiefpass schließt, Mitte = aus, obere Hälfte = Hochpass öffnet. Loslassen schaltet ihn aus |

Beides wirkt auf die Loop-Summe (Spuren und Spur-FX); Eingangs-Monitor und Metronom bleiben unberührt. Beides sind Host-Parameter (*Tape-Stopp*, *Performance-Filter*), ein Expression-Pedal oder ein Ribbon kann sie also auch spielen.

### Fußschalter

Push 1 hat hinten zwei Fußschalter-Buchsen (Taster).

| Fußschalter | Funktion |
|---|---|
| **2** (CC 69) | **antippen** = REC / PLAY / DUB der ausgewählten Spur · **doppelt antippen** = STOP · **1 s halten** = Spur löschen |
| **1** (CC 64) | nächste Spur (wählt die Spuren 1, 2, 3 … der Reihe nach) |

Wie bei einem Looper-Pedal wirkt schon der erste Tipp eines Doppeltipps (REC / PLAY / DUB), der zweite stoppt. Arbeitet ein Fußschalter verkehrt herum, steck ihn gedrückt ein oder nutze seinen Polaritätsschalter.

### FX-PAD-Modus (Taste NOTE)

Die 8 × 8 Pads werden ein **XY-Pad** für einen Effekt: X (links nach rechts) = Parameter 1, Y (unten nach oben) = Parameter 2, **Pad-Druck** = mehr Effekt (vom eigenen Mix des Effekts bis 100 % Effektanteil). Berühren schaltet den Effekt ein. Beim Loslassen **springt er zurück** auf die vorherigen Einstellungen. Halte beim Loslassen **SHIFT**, um den Effekt so zu behalten. Die oberen Tasten 1 – 4 wählen Eingangs-FX A – D, die Tasten 5 – 8 Spur-FX A – D. **SESSION** führt zurück in den LOOP-Modus.

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

Zeile 1: was die Encoder steuern · Zeile 2: ihre Werte · Zeile 3: Status der Spuren (`>` = ausgewählte Spur) · Zeile 4: Tempo, letzte Szene, Auto-Aufnahme, XY-Effekt. Kurze Meldungen (Loop-Länge, Kopie, Oktave …) erscheinen kurz in Zeile 4 (auf Deutsch, wenn livelooper auf Deutsch eingestellt ist).

**Pegelanzeige** (Taste MASTER): Zeile 3 zeigt unter jeder Spur einen Pegelbalken aus den eigenen Balkenzeichen des Displays (wie Lives Push-Skript), 0 bis −48 dB, 25-mal pro Sekunde aktualisiert. Vor jedem Balken: `>` für die ausgewählte Spur und ein Statusbuchstabe:

| Buchstabe | Status |
|---|---|
| `P` | spielt |
| `R` | Aufnahme |
| `D` | Overdub |
| `S` | gestoppt |
| `-` | leer |
| `r` / `p` | wartet auf den Takt für Aufnahme / Wiedergabe |
| `s` | stoppt (Fade / Loop-Ende) |
| `M` | stumm (MUTE, oder eine andere Spur ist solo) |

Ein voller Block am Ende eines Balkens = die Spur hat 0 dBFS erreicht. Bei bis zu 6 Spuren steht der Master-Pegel in der Master-Sektion (*M*), bei 8 Spuren am Anfang von Zeile 4 (*Out*).

### MIDI und Host-Parameter

Jede Push-Geste nutzt Funktionen, die auch Host-Parameter sind – jeder MIDI-Controller kann sie also spielen (siehe [MIDI](MIDI.md)): *Slice 1 – 6* (pro Spur; gehalten = Note an, losgelassen = Note aus), *Zurück in Sync*, *Stumm*, *Solo*, *Loop verdoppeln*, *Loop halbieren*, *Oktave hoch / runter*, *In leere Spur kopieren*, *Aufnahmelänge*, *Tape-Stopp* und *Performance-Filter*.
