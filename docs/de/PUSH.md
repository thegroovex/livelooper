# Steuerung mit Ableton Push

livelooper spricht **direkt** mit einem Ableton Push (1) über dessen **User-Port**, sowohl im Plug-in (VST3 / AU, in jeder DAW) als auch in der Standalone-App. Weil livelooper Push selbst ansteuert, muss deine DAW Push nicht unterstützen. Die Pads leuchten mit Status und Loop-Position jeder Spur, das Display zeigt Namen, Werte und Tempo.

Mit einem Push wird livelooper zu einem eigenständigen Instrument: keine Maus, kein Bildschirm, nur Pads und Encoder – so, wie es live gespielt werden soll.

![livelooper auf Ableton Push](../images/livelooper-live-push.jpg)

Push 2 und Push 3 senden dieselben Pad- und Tasten-Nachrichten, die Bedienung sollte dort also auch funktionieren (noch nicht getestet). Die LED-Farben können dort abweichen, und ihr Display wird nicht genutzt. Touchstrip, Fußschalter und Display-Funktionen sind für Push 1 gemacht.

## Einrichtung

**In jeder DAW:** Push anschließen, livelooper laden und **USER** auf Push drücken – Push wechselt zu livelooper. **SETUP → Ableton Push** zeigt „Verbunden“. Unter Windows kann einen MIDI-Port immer nur ein Programm benutzen: Hat deine DAW den User-Port von Push („MIDIIN2 / MIDIOUT2 (Ableton Push)“) eingeschaltet, schalte ihn in den MIDI-Einstellungen der DAW aus, damit livelooper ihn öffnen kann.

**In Ableton Live**
1. Push wie gewohnt eingerichtet lassen (Bedienoberfläche „Push“ in Lives MIDI-Einstellungen).
2. livelooper laden. **SETUP → Ableton Push** zeigt „Verbunden“.
3. **USER** auf Push drücken: Push wechselt zu livelooper. Erneut **USER** drücken, und du bist zurück in Live.

**Windows (Live):** Lass in Lives MIDI-Einstellungen den Ein- und Ausgang **„MIDIIN2 (Ableton Push)“ / „MIDIOUT2 (Ableton Push)“** ausgeschaltet (Spur, Sync und Fern aus). Sonst kann livelooper den Port nicht öffnen. Unter macOS ist das nicht nötig.

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
| METRONOME | Rhythmus ein / aus · **SHIFT** + METRONOME = **Ableton Link** ein / aus |
| TAP TEMPO | Tap-Tempo · **SHIFT** + TAP TEMPO = Metronom-Klick ein / aus |
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
| FIXED LENGTH | **Capture-Länge**: 1 → 2 → 4 → 8 → 16 → 32 Takte und wieder von vorn (**SHIFT** = kürzer); RECORD macht den Capture |
| ADD EFFECT · ADD TRACK · LEFT / RIGHT · BROWSE · REPEAT · ACCENT | Performance-FX, siehe [unten](#performance-fx-seit-15) |

### Touchstrip

| Geste | Funktion |
|---|---|
| Strip berühren, nach unten ziehen | **Tape-Stopp**: Die Loops werden langsamer wie ein anhaltendes Tonband (oben = normale Geschwindigkeit, unten = Stillstand). **Loslassen**: Das Band läuft wieder an und landet genau dort, wo die Musik jetzt wäre – die Zeitachse hält nie an |
| **SHIFT** + berühren, ziehen | **Performance-Filter** auf den Loops: untere Hälfte = Tiefpass schließt, Mitte = aus, obere Hälfte = Hochpass öffnet. Loslassen schaltet ihn aus |
| **REPEAT** halten + berühren, schieben | **Beat Repeat** mit dem Timing unter deinem Finger: unten = 1/2, dann 1/4, 1/8, 1/16, oben = 1/32. Es wiederholt, solange der Finger auf dem Strip liegt (oder REPEAT gehalten wird) |
| **ACCENT** halten + berühren, schieben | dasselbe für **Stutter** |

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
| BROWSE | Performance-FX: 1 Beat-FX-Typ · 2 Schlag · 3 Pegel / Tiefe · 4 Kanal · 5 Color-FX-Typ · 6 Repeat-Timing · 7 Stutter-Timing · 8 Metronom-Pegel (Zeile 3 zeigt ON / off) |

Master-Encoder = Gesamtpegel · Tempo-Encoder = Tempo (1 BPM, mit **SHIFT** 0,1 BPM). Mit **SHIFT** wird fein eingestellt.

**Master-Sektion**: Bei bis zu 6 sichtbaren Spuren sind die beiden Encoder ganz rechts eine Master-Sektion: **Encoder 7 = Gesamtpegel**, **Encoder 8 = Tempo** (BPM, mit **SHIFT** 0,1 BPM); das Display zeigt darüber *Master* und *Tempo*. Bei 8 Spuren (und im DEVICE- und BROWSE-Modus) steuern alle acht Encoder Spuren bzw. Effekte; dann gelten der Master-Encoder (oben rechts) und der Tempo-Encoder (oben links), und Zeile 4 zeigt den Gesamtpegel. Mit **HOST SYNC** bestimmt der Host (deine DAW) das Tempo (Display: *Host*); das interne Tempo gilt nur ohne Host-Sync.

Swing-Encoder = **Swing** des Rhythmus, **SHIFT** + Swing-Encoder = Rhythmus-Pegel.

### Display

Zeile 1: was die Encoder steuern · Zeile 2: ihre Werte · Zeile 3: Status der Spuren (`>` = ausgewählte Spur) · Zeile 4: Tempo, letzte Szene, Auto-Aufnahme, Link (mit Anzahl der Teilnehmer), XY-Effekt. Kurze Meldungen (Loop-Länge, Kopie, Oktave …) erscheinen kurz in Zeile 4 (auf Deutsch, wenn livelooper auf Deutsch eingestellt ist).

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

### Performance-FX (seit 1.5)

Beat-FX, Beat Repeat / Stutter, Color-FX und Metronom der Performance-Reihe (Bedienungsanleitung, Abschnitte 25–28) liegen ohne Mapping auf dem Push:

| Taste | Funktion |
|---|---|
| ADD EFFECT | **Beat-FX** ein / aus |
| ADD TRACK | Beat-FX **solange gehalten** (ein kurzer Einwurf) |
| LEFT / RIGHT (Pfeile) | Schlagwert kürzer / länger (1/32 … 16 Schläge) |
| BROWSE | Encoder für die Performance-FX (siehe *Encoder*) |
| REPEAT | **Beat Repeat solange gehalten** mit dem Repeat-Timing · **SHIFT** + REPEAT = Latch ein / aus |
| ACCENT | **Stutter solange gehalten** mit dem Stutter-Timing · **SHIFT** + ACCENT = Latch ein / aus |
| REPEAT / ACCENT gehalten + Touchstrip | Timing mit dem Finger wählen: 1/2 (unten) … 1/32 (oben) |
| FIXED LENGTH | Capture-Länge 1 – 32 Takte (**SHIFT** = kürzer) |
| SHIFT + METRONOME | Ableton Link ein / aus (Zeile 4 zeigt *Link* und die Teilnehmer) |
| SHIFT + TAP TEMPO | Metronom-Klick ein / aus |

Die Tasten leuchten, solange ihre Funktion an ist. Alles beginnt auf dem Raster des gewählten Timings und kehrt beim Loslassen synchron zurück.

### MIDI und Host-Parameter

Jede Push-Geste nutzt Funktionen, die auch Host-Parameter sind – jeder MIDI-Controller kann sie also spielen (siehe [MIDI](MIDI.md)): *Slice 1 – 6* (pro Spur; gehalten = Note an, losgelassen = Note aus), *Zurück in Sync*, *Stumm*, *Solo*, *Loop verdoppeln*, *Loop halbieren*, *Oktave hoch / runter*, *In leere Spur kopieren*, *Aufnahmelänge*, *Tape-Stopp* und *Performance-Filter* – und seit 1.5 *Beat-FX an / Halten / Typ / Schlag / Pegel/Tiefe / Kanal*, *Color-FX-Typ*, *Color 1 – 8*, *Beat Repeat Halten / Latch / Timing*, *Stutter Halten / Latch / Timing*, *Metronom an / Pegel / Betonung*, *Link*, *Capture-Takte*.


### STEP-Modus und FILL (1.6)

**SCALES** schaltet in den **STEP-Modus**: Die Pads werden zum Step-Sequencer für die Drums (nochmal SCALES, SESSION oder NOTE beendet ihn).

| | STEP-Modus |
|---|---|
| Pad-Reihen | die acht Instrumente des Kits, **unterste Reihe = Kick**, dann Snare, Clap, Hi-Hat zu, Hi-Hat offen, Rim, Shaker, **oberste Reihe = Tom** (Afro Latin / Packs: ihre Instrumente) |
| Pad-Spalten | 8 Schritte; die Anschlagstärke des Pads wird zur Dynamik des Schritts |
| Pad | Schritt an / aus · **SHIFT** + Pad = Akzent an / aus |
| LEFT / RIGHT | vorherige / nächste 8 Schritte (16 Schritte pro Takt = 2 Seiten, bis 4 Takte × 32 Schritte) |
| UP / DOWN | nächstes / vorheriges Pattern (eingebaute Grooves und User 1 … 16; Wechsel wie unter PATTERN-WECHSEL eingestellt) |
| DELETE + Pad | ganze Zeile löschen |
| MUTE + Pad, obere Tasten 1 – 8 | Zeile stumm an / aus |
| SOLO + Pad, untere Tasten 1 – 8 | Zeile solo an / aus |
| Encoder 1 – 8 | Pegel der Zeile · **SHIFT**: Tonhöhe der Zeile (±12 Halbtöne) |

Pad-Farben: grau = leerer Schritt auf dem 1. / 3. Schlag (Orientierung), gelb = Ghost, orange = normal, rot = Akzent, grün / weiß = der Schritt, der gerade spielt, gelb gedimmt = stumme Zeile. Läuft ein eingebauter Groove, kopiert der erste Pad-Druck ihn in einen freien User-Platz (User 1 … 16), der dann spielt – die eingebauten Grooves bleiben unverändert. Display: Zeile 1 Instrument-Namen, Zeile 2 Pegel (SHIFT: Tonhöhe) oder MUTE / SOLO, Zeile 3 Pattern und gezeigte Schritte, Zeile 4 Kit, FILL, DRUMS-Routing und Meldungen.

In **jedem Modus**:

| Taste | Funktion |
|---|---|
| IN | **FILL** solange gehalten – nach dem Loslassen setzt der Groove auf der nächsten Eins wieder ein |
| OUT | Drums durch die **Beat FX** an / aus |
| SHIFT + OUT | Drums durch **Beat Repeat / Stutter** an / aus |

IN und OUT leuchten, solange der Fill spielt / ein DRUMS-Routing an ist. Neue Host-Parameter für andere Controller: *Fill (halten)*, *Drums durch Beat FX*, *Drums durch Repeat/Stutter*, *Pattern-Wechsel*, *Drum-Variation*, *Beat Zeile 1 … 8 stumm / solo / Pegel / Tonhöhe*.
