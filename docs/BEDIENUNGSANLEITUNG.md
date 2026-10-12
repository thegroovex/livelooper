# Bedienungsanleitung

![Oberfläche](images/livelooper.png)

Die Oberfläche ist wie ein Hardware-Looper aufgebaut:

| Bereich | Inhalt |
|---|---|
| Kopfzeile | Display (Tempo, Takt/Schlag, Quantisierung, Meldungen), **MIDI LEARN**, **MIDI MAP**, **SETUP** |
| INPUT FX (links) | vier Effekt-Slots A–D für das Eingangssignal |
| FX EDIT (Mitte) | Effekttyp und Regler des gerade gewählten Slots |
| TRACK FX (rechts) | vier Effekt-Slots A–D für die Spuren mit eingeschaltetem **FX** |
| TRACK 1–5 | die fünf Loop-Spuren |
| RHYTHM | Drumcomputer, Tempo, Tap |
| MASTER | Quantisierung, Host-Sync, Pegel, Fade-Zeit, ALL START/STOP, CLEAR ALL |

Das Fenster lässt sich an der rechten unteren Ecke größer und kleiner ziehen; die Oberfläche skaliert mit.

---

## 1. Eine Spur

![Track-Setup](images/livelooper-track-setup.png)

### REC/PLAY-Pad (der große runde Knopf)

| Zustand der Spur | Druck auf REC/PLAY |
|---|---|
| leer | **Aufnahme** startet (mit Quantisierung auf dem nächsten Takt/Schlag) |
| Aufnahme | Aufnahme endet → **Overdub** (Standard „Rec > Dub") bzw. **Play** („Rec > Play") |
| Overdub | → **Play** |
| Play | → **Overdub** |
| gestoppt | → **Play** (quantisiert) |
| wartet (blinkt) | erneuter Druck bricht das Warten ab |

Der LED-Ring zeigt den Zustand und die Position im Loop:

- **rot** Aufnahme · **orange** Overdub · **grün** Play · **grün gedimmt** gestoppt, Loop vorhanden · **aus** leer
- **blinkend**: wartet auf den nächsten Takt (Aufnahme, Start oder Aufnahme-Ende)
- die hellen Segmente laufen im Kreis und zeigen die Wiedergabeposition

### STOP

- kurzer Druck: Spur stoppt (je nach **Stop**-Modus sofort, mit Fade-Out oder am Loop-Ende)
- Druck während der Aufnahme: die Aufnahme wird beendet und die Spur gestoppt
- **1,5 s halten: Spur löschen** (der rote Balken zeigt den Fortschritt)

### Weitere Bedienelemente der Spur

| Element | Funktion |
|---|---|
| Display | Zustand, Länge in Takten, Pegel, Fortschrittsbalken |
| **REV** | Loop rückwärts abspielen (auch beim Overdub) |
| **1SHOT** | Loop einmal abspielen, dann stoppen |
| **FX** | Spur durch die Track-FX schicken |
| Fader | Pegel 0–200 % (Doppelklick = 100 %), daneben die Pegelanzeige |
| **PAN** | Panorama L50–R50 (Doppelklick = Mitte) |
| **UNDO / REDO** | letzten Overdub zurücknehmen / wiederherstellen. Undo direkt nach der ersten Aufnahme leert die Spur, Redo holt sie zurück |
| **SETUP** | Spur-Einstellungen (Bild oben) |

### Spur-Einstellungen (SETUP)

| Einstellung | Werte | Bedeutung |
|---|---|---|
| Input | Stereo · Left · Right | welcher Eingangskanal aufgenommen wird (Left/Right = mono auf beide Seiten) |
| Start | Immediate · Fade In | Wiedergabestart sofort oder mit Einblendung (Fade-Zeit im MASTER) |
| Stop | Immediate · Fade Out · Loop End | Stopp sofort, mit Ausblendung oder am Ende des Loops |
| Dub mode | Overdub · Replace | beim Overdub hinzufügen oder ersetzen |
| Measures | Auto · 1 · 2 · 4 · 8 · 16 | Auto = Länge durch zweiten Druck; feste Zahl = Aufnahme endet automatisch nach so vielen Takten |
| LOOP SYNC | an/aus | Loop bleibt fest am Taktraster: nach dem Spulen in Live oder beim Neustart läuft er an der passenden Stelle weiter |
| CLEAR | | Spur löschen |
| EXPORT WAV | | Loop als 24-Bit-WAV speichern |
| IMPORT | | Audiodatei (WAV, AIFF, FLAC, OGG, MP3) in die Spur laden. Schneller: Datei per Drag & Drop auf die Spur ziehen |

---

## 2. Quantisierung, Tempo und Sync

**QUANTIZE** (MASTER):

- **Measure** (Standard): Aufnahme-Start, Aufnahme-Ende und Wiedergabestart passieren auf dem nächsten Taktstrich. Loops sind damit immer ganze Takte lang.
- **Beat**: dasselbe auf Viertel-Schlägen.
- **Off**: alles sofort, freie Loop-Länge (auf das Sample genau zwischen den beiden Drücken).

**Rückwirkendes Beenden**: Wer das Ende einer Aufnahme bis zu 15 % eines Rasterschritts *nach* dem Taktstrich drückt, bekommt den Loop trotzdem exakt am Taktstrich beendet – das Gespielte nach dem Taktstrich wird an den Anfang des Loops (in den Overdub) übernommen. Wer zu früh drückt, wartet einfach auf den Taktstrich.

**HOST SYNC** (Standard an): Tempo, Taktart und Position kommen von Live. Läuft Lives Transport nicht, läuft eine interne Clock im aktuellen Live-Tempo weiter, sobald eine Spur aufnimmt oder spielt. Ohne Host-Sync gilt das interne Tempo (Regler **TEMPO**, oder **TAP** mehrfach im Takt antippen).

**Follow host transport** (SETUP-Menü): Loops starten mit Lives Play-Taste und stoppen mit Lives Stop-Taste.

Hinweis: Ändert sich das Tempo, nachdem ein Loop aufgenommen wurde, wird der Loop nicht zeitgedehnt – er spielt in seiner Originalgeschwindigkeit weiter.

---

## 3. Effekte

Es gibt zwei Effektbänke mit je vier Slots:

- **INPUT FX** bearbeiten das Eingangssignal – was du hörst, wird so auch aufgenommen.
- **TRACK FX** bearbeiten alle Spuren, bei denen **FX** eingeschaltet ist (gemeinsamer Effektweg), ohne die Aufnahme zu verändern.

Der Knopf **A/B/C/D** schaltet den Slot ein und aus und wählt ihn gleichzeitig zum Bearbeiten aus; ein Klick auf den Effektnamen darunter wählt ihn nur aus. Im Feld **FX EDIT** stellst du Typ, zwei Parameter und Mix ein. Die Slots einer Bank sind in Reihe geschaltet (A → B → C → D).

| Effekt | P1 | P2 | Mix |
|---|---|---|---|
| LPF / HPF / BPF | Cutoff | Resonanz | Dry/Wet |
| Phaser | Tiefe | Rate (0,05–5 Hz) | Dry/Wet |
| Flanger | Feedback | Rate | Dry/Wet |
| Delay | Feedback | Zeit: 1/16 · 1/8 · 1/8. · 1/4 · 1/4. · 1/2 (tempo-synchron) | Echo-Pegel |
| Reverb | Raumgröße | Dämpfung | Hall-Pegel |
| Slicer | Tiefe | Pattern (8 Rhythmen in 16teln) | Dry/Wet |
| Tremolo | Tiefe | Rate: 1 Takt … 1/32 (tempo-synchron) | Dry/Wet |
| Auto Pan | Tiefe | Rate (tempo-synchron) | Dry/Wet |
| Lo-Fi | Bit-Tiefe (16 → 3 Bit) | Sample-Rate-Reduktion | Dry/Wet |
| Overdrive | Drive | Ton | Dry/Wet |

Ein- und Ausschalten wird ohne Knackser überblendet.

---

## 4. Rhythmus

**RHYTHM** schaltet den Drumcomputer ein, daneben wird das Pattern gewählt: Metronome, Rock, Pop, Funk, Hip Hop, House, Techno, Breakbeat, Reggae, Bossa, Shuffle, Half Time. **LEVEL** regelt die Lautstärke. Der Rhythmus ist fest mit dem Taktraster verbunden (Host oder interne Clock) und wird nicht mit aufgenommen. Beim Metronom ist die Eins betont.

---

## 5. MASTER

| Element | Funktion |
|---|---|
| QUANTIZE | Off · Beat · Measure |
| HOST SYNC | Tempo/Transport von Live übernehmen |
| MASTER | Gesamtpegel 0–200 % |
| INPUT | Pegel des direkt durchgeschleiften Eingangs 0–200 % (die Aufnahme ist davon unabhängig) |
| FADE | Fade-Zeit für Fade-In/Fade-Out (10 ms – 8 s) |
| ALL START / STOP | läuft irgendeine Spur: alle stoppen; sonst alle Spuren mit Loop starten (quantisiert) |
| CLEAR ALL | alle Spuren löschen (mit Rückfrage) |

---

## 6. SETUP-Menü

| Eintrag | Bedeutung |
|---|---|
| Record action | „Rec > Dub > Play" (Standard) oder „Rec > Play > Dub" |
| Follow host transport | Loops starten/stoppen mit Live |
| Maximum loop length | 30 · 60 · 120 · 240 · 480 Sekunden pro Spur (bestimmt den Speicherbedarf; vorhandene Loops bleiben erhalten) |
| Save loops with the project | Loops im Live-Set speichern (Standard an). Aus = kleinere Sets, die Loops sind nach dem Laden weg |

---

## 7. Speichern und Laden

Mit dem Live-Set werden gespeichert: alle Parameter, die MIDI-Zuweisungen und – wenn eingeschaltet – die Loops (verlustfrei als FLAC, 24 Bit). Beim Laden sind die Spuren gestoppt und starten mit REC/PLAY oder ALL START. Wechselt die Samplerate des Projekts, werden die Loops automatisch umgerechnet.

## 8. Display

- große Zahl: aktuelles Tempo
- Zeile 1: Tempoquelle (HOST, interne Clock)
- Zeile 2: Takt . Schlag und Quantisierung
- Zeile 3: Meldungen (Export, Import, MIDI-Learn) oder der Speicher pro Spur
- Punkt oben rechts: leuchtet gelb bei eingehenden MIDI-Daten
