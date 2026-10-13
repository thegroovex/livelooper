# Bedienungsanleitung

[English](../en/USER-GUIDE.md) | **Deutsch**

livelooper ist als **Instrument zum Erforschen und Live-Spielen** gebaut: Jedes Bedienelement reagiert sofort und landet auf dem Schlag, und alles lässt sich mit einem [Ableton Push](PUSH.md), jedem MIDI-Controller oder der Computertastatur spielen. Tipps fürs Live-Spielen stehen in Abschnitt 23.

![Oberfläche](../images/livelooper-de.png)

Die Oberfläche ist wie ein Hardware-Looper aufgebaut. Die Bezeichnungen in dieser Anleitung entsprechen der deutschen Oberfläche; bei englischer Oberfläche steht die englische Bezeichnung in Klammern (siehe auch [Kapitel 7](#7-sprache) und die [Übersicht am Ende](#16-bezeichnungen-deutsch--englisch)).

| Bereich | Inhalt |
|---|---|
| Kopfzeile | Display (Tempo, Takt/Schlag, Quantisierung, Meldungen), **MIDI LEARN**, **MIDI MAP**, **SETUP** |
| EINGANGS-FX (links) | vier Effekt-Slots A–D für das Eingangssignal |
| … BEARBEITEN (Mitte) | Effekttyp und Regler des gerade gewählten Slots |
| SPUR-FX (rechts) | vier Effekt-Slots A–D für die Spuren mit eingeschaltetem **FX** |
| SPUR 1–5 (oder 1–8) | die Loop-Spuren (SETUP → Spuren) |
| RHYTHMUS | Drumcomputer, Tempo, Tap |
| MASTER | Quantisierung, Host-Sync, Pegel, Fade-Zeit, ALLE START / STOP, CAPTURE, ALLE LÖSCHEN |
| Fußleiste | 8 Szenen-Knöpfe, SPEICHERN |

Das Fenster lässt sich an der rechten unteren Ecke größer und kleiner ziehen; die Oberfläche skaliert mit. Fährt man mit der Maus über ein Element, erscheint ein Hinweis.

---

## 1. Eine Spur

![Spur-Setup](../images/livelooper-track-setup-de.png)

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

- kurzer Druck: Spur stoppt (je nach **Stop**-Modus sofort, mit Ausblenden oder am Loop-Ende)
- Druck während der Aufnahme: die Aufnahme wird beendet und die Spur gestoppt
- **1,5 s halten: Spur löschen** (der rote Balken zeigt den Fortschritt)

### Weitere Bedienelemente der Spur

| Element | Funktion |
|---|---|
| Display | Zustand (LEER, REC, DUB, PLAY, STOP, … WARTET), Länge in Takten, Pegel (PEG), Fortschrittsbalken |
| **RÜCKW.** (REV) | Loop rückwärts abspielen (auch beim Overdub) |
| **1SHOT** | Loop einmal abspielen, dann stoppen |
| **FX** | Spur durch die Spur-FX schicken |
| Fader | Pegel 0–200 % (Doppelklick = 100 %), daneben die Pegelanzeige |
| **PAN** | Panorama L50–R50 (Doppelklick = Mitte) |
| **ZURÜCK / VOR** (UNDO / REDO) | letzten Overdub zurücknehmen / wiederherstellen. ZURÜCK direkt nach der ersten Aufnahme leert die Spur, VOR holt sie zurück |
| **SETUP** | Spur-Einstellungen (Bild oben) |

### Spur-Einstellungen (SETUP)

| Einstellung | Werte | Bedeutung |
|---|---|---|
| Eingang (Input) | Stereo · Links · Rechts | welcher Eingangskanal aufgenommen wird (Links/Rechts = mono auf beide Seiten) |
| Start | Sofort · Einblenden | Wiedergabestart sofort oder mit Einblendung (Fade-Zeit im MASTER) |
| Stop | Sofort · Ausblenden · Loop-Ende | Stopp sofort, mit Ausblendung oder am Ende des Loops |
| Dub-Modus (Dub mode) | Overdub · Ersetzen | beim Overdub hinzufügen oder ersetzen |
| Takte (Measures) | Auto · 1 · 2 · 4 · 8 · 16 | Auto = Länge durch zweiten Druck; feste Zahl = Aufnahme endet automatisch nach so vielen Takten |
| LOOP-SYNC | an/aus | Loop bleibt fest am Taktraster: nach dem Spulen in der DAW oder beim Neustart läuft er an der passenden Stelle weiter |
| LÖSCHEN (CLEAR) | | Spur löschen |
| WAV EXPORT | | Loop als 24-Bit-WAV speichern |
| IMPORT | | Audiodatei (WAV, AIFF, FLAC, OGG, MP3) in die Spur laden. Schneller: Datei per Drag & Drop auf die Spur ziehen |

---

## 2. Quantisierung, Tempo und Sync

**Quantisierung** (Auswahlfeld im MASTER, im Display „QUANT."):

- **Takt** (Measure, Standard): Aufnahme-Start, Aufnahme-Ende und Wiedergabestart passieren auf dem nächsten Taktstrich. Loops sind damit immer ganze Takte lang.
- **Schlag** (Beat): dasselbe auf Viertel-Schlägen.
- **Aus** (Off): alles sofort, freie Loop-Länge (auf das Sample genau zwischen den beiden Drücken).

**Rückwirkendes Beenden**: Wer das Ende einer Aufnahme bis zu 15 % eines Rasterschritts *nach* dem Taktstrich drückt, bekommt den Loop trotzdem exakt am Taktstrich beendet – das Gespielte nach dem Taktstrich wird an den Anfang des Loops (in den Overdub) übernommen. Wer zu früh drückt, wartet einfach auf den Taktstrich.

**HOST-SYNC** (Standard an): Tempo, Taktart und Position kommen von deiner DAW. Läuft der Host-Transport nicht, läuft eine interne Clock im aktuellen Host-Tempo weiter, sobald eine Spur aufnimmt oder spielt. Ohne Host-Sync gilt das interne Tempo (Regler **TEMPO**, **TAP** mehrfach im Takt antippen, oder auf das Tempo im Display klicken und eintippen – siehe Abschnitt 11).

**Host-Transport folgen** (SETUP-Menü): Loops starten mit der Play-Taste deiner DAW und stoppen mit ihrer Stop-Taste.

Hinweis: Ändert sich das Tempo, nachdem ein Loop aufgenommen wurde, wird der Loop nicht zeitgedehnt – er spielt in seiner Originalgeschwindigkeit weiter.

---

## 3. Effekte

Es gibt zwei Effektbänke mit je vier Slots:

- **EINGANGS-FX** (INPUT FX) bearbeiten das Eingangssignal – was du hörst, wird so auch aufgenommen.
- **SPUR-FX** (TRACK FX) bearbeiten alle Spuren, bei denen **FX** eingeschaltet ist (gemeinsamer Effektweg), ohne die Aufnahme zu verändern.

Der Knopf **A/B/C/D** schaltet den Slot ein und aus und wählt ihn gleichzeitig zum Bearbeiten aus; ein Klick auf den Effektnamen darunter wählt ihn nur aus. Im Feld **… BEARBEITEN** stellst du Typ, zwei Parameter und Mix ein. Die Slots einer Bank sind in Reihe geschaltet (A → B → C → D).

| Effekt (englisch) | P1 | P2 | Mix |
|---|---|---|---|
| Tiefpass / Hochpass / Bandpass (LPF / HPF / BPF) | Cutoff | Reso(nanz) | Dry/Wet |
| Phaser | Tiefe | Rate (0,05–5 Hz) | Dry/Wet |
| Flanger | Feedback | Rate | Dry/Wet |
| Delay | Feedback | Zeit: 1/16 · 1/8 · 1/8. · 1/4 · 1/4. · 1/2 (tempo-synchron) | Pegel (Echo) |
| Hall (Reverb) | Größe | Dämpfung | Pegel (Hall) |
| Slicer | Tiefe | Muster (8 Rhythmen in 16teln) | Dry/Wet |
| Tremolo | Tiefe | Rate: 1 Takt … 1/32 (tempo-synchron) | Dry/Wet |
| Auto-Pan | Tiefe | Rate (tempo-synchron) | Dry/Wet |
| Lo-Fi | Bits (16 → 3 Bit) | Rate (Sample-Rate-Reduktion) | Dry/Wet |
| Overdrive | Drive | Klang | Dry/Wet |

Ein- und Ausschalten wird ohne Knackser überblendet.

---

## 4. Rhythmus

**RHYTHMUS** schaltet den Drumcomputer ein, daneben wird das Muster gewählt: Metronom, Rock, Pop, Funk, Hip Hop, House, Techno, Breakbeat, Reggae, Bossa, Shuffle, Half Time, Trap, Lo-Fi, Drum & Bass, Afrobeat, Dembow, UK Garage, Neo Soul, Disco, Afro House. Der Rhythmus ist fest mit dem Taktraster verbunden (Host oder interne Clock) und wird nicht mit aufgenommen. Beim Metronom ist die Eins betont.

| Regler | Funktion |
|---|---|
| **Kit** | **Studio** (straff, modern), **808** (tiefe Boom-Kick, knackige Hi-Hats), **909** (druckvoll für House / Techno), **Lo-Fi** (warm, staubig, weiche Höhen) |
| **PEGEL** | Lautstärke des Rhythmus |
| **SWING** | verzögert jede zweite Sechzehntel. 50 % = gerade, 54–58 % = leichter Groove, 66 % = Triolen-Shuffle, 75 % = maximal. Tipp: Hip Hop / Lo-Fi 56–60 %, UK Garage 60–64 %, Neo Soul 60–66 % |
| **FEEL** | Humanisierung: kleine Schwankungen im Timing (ein paar ms später, nie früher) und in der Anschlagstärke. 0 % = maschinengenau |
| **TEMPO / TAP** | internes Tempo |

Jedes Muster hat Akzente und Ghost-Notes, Hi-Hat-Rolls (Trap) und Percussion (Rimshot, Shaker, Tom). Snare, Clap und Rimshot klingen in einem kurzen Raum. Das Metronom wird nie geswingt.

---

## 5. MASTER

| Element | Funktion |
|---|---|
| Quantisierung | Aus · Schlag · Takt |
| HOST-SYNC | Tempo/Transport vom Host übernehmen |
| MASTER | Gesamtpegel 0–200 % |
| EINGANG (INPUT) | Pegel des direkt durchgeschleiften Eingangs 0–200 % (die Aufnahme ist davon unabhängig) |
| FADE | Fade-Zeit für Ein-/Ausblenden (10 ms – 8 s) |
| ALLE START / STOP | läuft irgendeine Spur: alle stoppen; sonst alle Spuren mit Loop starten (quantisiert) |
| ALLE LÖSCHEN | alle Spuren löschen (mit Rückfrage) |

---

## 6. SETUP-Menü

| Eintrag | Bedeutung |
|---|---|
| Aufnahme-Aktion (Record action) | „Rec > Dub > Play" (Standard) oder „Rec > Play > Dub". Die Beschriftung der REC-Taste („DUB / PLAY" bzw. „PLAY / DUB") und die unterste Displayzeile zeigen den aktiven Modus. |
| Host-Transport folgen | Loops starten/stoppen mit dem Host-Transport |
| Maximale Loop-Länge | 30 · 60 · 120 · 240 · 480 Sekunden pro Spur (bestimmt den Speicherbedarf; vorhandene Loops bleiben erhalten) |
| Loops im Projekt speichern | Loops im DAW-Projekt speichern (Standard an). Aus = kleinere Projekte, die Loops sind nach dem Laden weg |
| Steuerung mit der Computertastatur | nur Standalone-App: Tasten an/aus (Abschnitt 10) |
| Tastaturbelegung … | nur Standalone-App: eigene Tastenbelegung (Abschnitt 10) |
| Tempo eintippen (BPM) … | internes Tempo eintippen (Abschnitt 11) |
| Language / Sprache | Automatisch (Systemsprache) · English · Deutsch |

---

## 7. Sprache

Die gesamte Oberfläche – Knöpfe, Displays, Menüs, Hinweise, Dialoge und Parameternamen – gibt es auf Deutsch und Englisch.

- **SETUP → Language / Sprache → Automatisch** (Standard): Deutsch, wenn das Betriebssystem auf Deutsch eingestellt ist, sonst Englisch.
- **English** oder **Deutsch** legt die Sprache fest. Die Wahl wird auf diesem Rechner gespeichert und gilt für alle livelooper-Instanzen, in deiner DAW und in der Standalone-App.
- Die Oberfläche wechselt sofort. Die Parameternamen, die deine DAW anzeigt (Gerät, MIDI-Mapping, Automation), liest sie beim Laden des Plug-ins – sie wechseln nach dem Neuladen (z. B. Projekt neu öffnen). Bestehende MIDI-Zuweisungen und Automationen bleiben erhalten.

![Englische Oberfläche](../images/livelooper.png)

---

## 8. Speichern und Laden

Mit dem DAW-Projekt werden gespeichert: alle Parameter, die MIDI-Zuweisungen und – wenn eingeschaltet – die Loops (verlustfrei als FLAC, 24 Bit). Beim Laden sind die Spuren gestoppt und starten mit REC/PLAY oder ALLE START. Wechselt die Samplerate des Projekts, werden die Loops automatisch umgerechnet.

## 9. Display

- große Zahl: aktuelles Tempo – anklicken, um die BPM einzutippen
- Zeile 1: Tempoquelle (HOST, INTERN, interne Clock)
- Zeile 2: Takt . Schlag und Quantisierung
- Zeile 3: Meldungen (Export, Import, MIDI Learn) oder der Speicher pro Spur
- Punkt oben rechts: leuchtet gelb bei eingehenden MIDI-Daten

---

## 10. Computertastatur (Standalone-App)

In der Standalone-App spielt die Computertastatur livelooper wie ein Hardware-Gerät – ideal am MacBook ohne Controller. In einer DAW gehört die Tastatur der DAW, deshalb ist das eine Standalone-Funktion (dort gibt es MIDI und die eigenen Tastaturbefehle bzw. Tastaturzuweisung der DAW).

Standardbelegung – eine Tastaturspalte pro Spur:

| Taste | Funktion |
|---|---|
| **1 2 3 4 5** (6 7 8) | REC/PLAY Spur 1–5 (6–8) |
| **Q W E R T** (Y U I) | STOP Spur 1–5 (6–8) |
| **Shift + Q … T** | Spur 1–5 LÖSCHEN – Taste 1,5 s halten, wie die STOP-Taste (kurzes Antippen löscht nicht) |
| **A S D F G** (H J L) | ZURÜCK (Undo) Spur 1–5 (6–8) |
| **Shift + A … G** | VOR (Redo) Spur 1–5 |
| **Leertaste** | ALLE START / STOP |
| **Shift + Rücktaste** | ALLE LÖSCHEN (mit Rückfrage) |
| **Eingabe (Return)** | TAP-Tempo |
| **B** | Tempo eintippen (BPM) |
| **Pfeil hoch / runter** | Tempo +1 / −1 BPM (mit Shift: ±0,1) |
| **M** | RHYTHMUS an/aus |
| **N** | nächstes Rhythmus-Pattern |
| **K** | nächste Quantisierung |

Das Display zeigt kurz, welche Taste was ausgelöst hat (z. B. `TASTE Q > S1 STOP`). Gehaltene Tasten wiederholen nicht – außer Tempo hoch/runter.

**Eigene Belegung:** *SETUP → Tastaturbelegung …* listet alle Funktionen (alle Spurfunktionen inkl. Rückwärts, One-Shot, Loop-Sync und FX, alle globalen Funktionen, FX-Slots A–D, Tempo). Funktion anklicken, neue Taste drücken – Shift, Strg, Option und Cmd zählen mit. Eine bereits belegte Taste wandert zur neuen Funktion. Esc bricht ab, Rechtsklick entfernt eine Taste, **STANDARD** stellt die Tabelle oben wieder her, **ALLE ENTFERNEN** leert sie, **TASTATURSTEUERUNG** schaltet die Tastatursteuerung an/aus (auch *SETUP → Steuerung mit der Computertastatur*). Die Belegung wird mit dem Zustand der App gespeichert.

![Tastaturbelegung](../images/livelooper-keyboard-de.png)

## 11. Tempo eintippen

Auf die große Tempo-Zahl im Display klicken (oder **B** drücken, oder Rechtsklick auf den TEMPO-Regler → *Tempo eintippen (BPM) …*, oder *SETUP → Tempo eintippen (BPM) …*), BPM eintippen, Eingabe drücken. Nachkommastellen gehen mit Punkt oder Komma (`97.5`, `97,5`); Bereich 40–300 BPM, Esc bricht ab. Der Wert setzt das interne Tempo – solange HOST-SYNC einem laufenden Host folgt, bestimmt weiter das Host-Tempo.

![Tempo eintippen](../images/livelooper-tempo-entry-de.png)

---

## 12. Pegelanzeigen, Wellenform und Clip-LED

- **Spur-Pegel** (Stereo, neben dem Fader): Während eine Spur aufnimmt oder overdubbt, zeigt er den **Eingangspegel** (orange, Beschriftung **IN**), sonst den **Ausgangspegel** der Spur (grün, **OUT**). Gelb ab -12 dB, rot ab -1 dB; der dünne Strich hält die Spitze etwa eine Sekunde.
- **Master-Pegel**: Die oberste Zeile im Display (**MASTER**) zeigt den Pegel des Hauptausgangs, links und rechts.
- **Wellenform**: Das Spur-Display zeigt statt eines Fortschrittsbalkens eine Übersicht des Loops. Der schon gespielte Teil ist hell, ein weißer Strich markiert die Abspielposition. Bei der Aufnahme wächst die Wellenform von links nach rechts; Overdubs und Undo sind sofort sichtbar. Bei REVERSE wird sie gespiegelt.
- **Clip-LED** (oben rechts an jeder Spur): aus = leer, grün = Spur enthält einen Loop, rot = erste Aufnahme läuft, rot blinkend = wartet auf den nächsten Takt, um aufzunehmen.

## 13. Looper-Sets

Ein Set speichert den kompletten Looper in einer Datei (`.llset`): alle Loops, Pegel, Pan, Spur-Einstellungen, Effekte, Rhythmus, Tempo und Quantisierung.

- **SETUP → Set speichern ...** schreibt ein Set (ein Set enthält immer seine Loops, auch wenn *Loops mit dem Projekt speichern* aus ist).
- **SETUP → Set laden ...** lädt eins; alternativ eine `.llset`-Datei auf das Fenster ziehen.
- MIDI-Zuweisungen, Tastatur-Belegung und Cue-Routing gehören zur App und bleiben beim Laden eines Sets erhalten.

## 14. Cue-Ausgang (Standalone-App)

Die Standalone-App hat einen zweiten Ausgangsbus für **Rhythmus und Metronom**: Du hörst den Click im Kopfhörer, das Publikum nicht.

1. Unter **Options → Audio/MIDI Settings** die zusätzlichen Ausgänge des Audio-Interfaces aktivieren (Ausgang 3 und höher).
2. **SETUP → Cue-Ausgang (Click / Rhythmus)**: Ausgangspaar wählen (z. B. *Ausgänge 3 / 4*) und festlegen, wo der Rhythmus spielt: **Hauptausgang**, **Nur Cue-Ausgang** oder **Haupt- und Cue-Ausgang**.

Ohne Cue-Paar (und im Plug-in) spielt der Rhythmus immer auf dem Hauptausgang – der Click geht also nie verloren. Die Einstellung wird mit der App gespeichert.

## 15. Metronomklang

**SETUP → Metronomklang**: **Piep** (klassisch), **Holzblock** (kurzer, hölzerner Schlag), **Klick** (trockener Tick ohne Tonhöhe) oder **Rimshot**. Die Drum-Sounds sind synthetisiert, siehe Abschnitt 4.

## 16. Bezeichnungen Deutsch / Englisch

| Deutsch | English |
|---|---|
| SPUR | TRACK |
| EINGANGS-FX / SPUR-FX | INPUT FX / TRACK FX |
| BEARBEITEN | EDIT |
| RHYTHMUS · Muster | RHYTHM · Pattern |
| ALLE START / STOP · ALLE LÖSCHEN | ALL START / STOP · CLEAR ALL |
| HOST-SYNC | HOST SYNC |
| RÜCKW. | REV |
| ZURÜCK / VOR | UNDO / REDO |
| LÖSCHEN · WAV EXPORT | CLEAR · EXPORT WAV |
| Pegel · Eingang | Level · Input |
| Quantisierung: Aus · Schlag · Takt | Quantize: Off · Beat · Measure |
| Sofort · Einblenden · Ausblenden · Loop-Ende | Immediate · Fade In · Fade Out · Loop End |
| Dub-Modus: Overdub · Ersetzen | Dub mode: Overdub · Replace |
| Takte | Measures |
| LEER · STOPPT · WARTET · ENDE · GELÖSCHT | EMPTY · STOPPING · WAIT · END · CLEARED |

## 16. Capture (rückwirkende Aufnahme)

Du hast etwas Gutes gespielt, ohne Aufnahme zu drücken? Drück **CAPTURE** (MASTER-Bereich). Die zuletzt gespielten Takte werden ein Loop auf der ersten leeren Spur und laufen sofort im Takt weiter. Die Länge stellst du unter **SETUP → Capture-Länge** ein (1, 2, 4 oder 8 Takte). Wurde weniger gespielt, nimmt livelooper die größte Länge, die passt. Im SETUP jeder Spur gibt es außerdem einen **CAPTURE**-Knopf für genau diese Spur. **UNDO** entfernt einen Capture-Loop wieder.

## 17. Auto-Aufnahme

**SETUP → Auto-Aufnahme**: Scharf geschaltete Spuren (REC gedrückt, Anzeige **AUTO REC**) warten, bis das Eingangssignal die Schwelle überschreitet (-50 bis -12 dB), und starten mit dem ersten Ton. So bleiben die Hände am Instrument. Erneutes Drücken von REC bricht ab.

## 18. Spur-Einstellungen: Feedback, Ducking, Tonhöhe, Tempo folgen, Ausgang

![Spur-Setup](../images/livelooper-track-setup-de.png)

| Einstellung | Bedeutung |
|---|---|
| **FEEDBACK** | wie viel vom Loop bei jedem Overdub-Durchlauf erhalten bleibt. 100 % = alles (klassisches Overdub), kleinere Werte lassen ältere Schichten wie bei einem Bandecho ausklingen |
| **DUCKING** | senkt die Spur ab, während die Ducking-Quelle spielt (**SETUP → Ducking-Quelle**: Kick des Rhythmus oder Spur 1). Ergibt pumpende Sidechain-Grooves |
| **TONHÖHE** | -12 bis +12 Halbtöne, die Länge bleibt gleich |
| **TEMPO FOLGEN** | der Loop folgt Tempoänderungen (Host oder intern), ohne seine Tonhöhe zu ändern (Time-Stretch) |
| **AUSGANG** | Haupt · Direkt · Haupt + Direkt: wo die Spur zu hören ist (Abschnitt 20) |
| **Eingang → Resample** | die Spur nimmt die Summe der anderen Spuren auf (Bounce): Loops auf einer Spur zusammenfassen und die anderen frei machen |

## 19. Szenen

In der Fußleiste gibt es **8 Szenen-Knöpfe**. Ein leerer Szenen-Knopf speichert beim ersten Drücken, welche Spuren gerade spielen. Eine gespeicherte Szene startet ihre Spuren und stoppt die anderen, quantisiert auf den nächsten Takt oder Schlag. **SPEICHERN** + Szenen-Knopf überschreibt eine Szene. Eine schwache LED zeigt eine gespeicherte Szene, eine helle die zuletzt aufgerufene. Szenen werden mit dem Projekt und in Sets gespeichert und lassen sich per MIDI steuern.

## 20. Direkte Spurausgänge

- **Plug-in**: livelooper hat pro Spur einen optionalen Stereo-Ausgang („Track 1“ … „Track 8“). Aktiviere ihn in deiner DAW (jede Spur bekommt dort dann einen eigenen Ausgang), in Ableton Live zum Beispiel über eine zweite Audiospur mit *Audio von: livelooper → Track 1*.
- **Standalone-App**: **SETUP → Direkte Spurausgänge** weist jeder Spur ein Ausgangspaar des Audio-Interfaces zu (ab Ausgang 3/4, nicht das Cue-Paar).
- Im Spur-SETUP legt **AUSGANG** fest: *Haupt*, *Direkt* (nur der direkte Ausgang) oder *Haupt + Direkt*. Ohne aktiven direkten Ausgang spielt die Spur immer auf dem Hauptausgang.

## 21. 8 Spuren, Geister-Abspielmarke, Stem-Export

- **SETUP → Spuren → 8 Spuren** zeigt drei weitere Spuren und macht das Fenster breiter. Ausgeblendete Spuren belegen keinen Speicher. Beim Zurückschalten auf 5 Spuren werden die Spuren 6 bis 8 gelöscht.
- **Geister-Abspielmarke**: Eine gestoppte Spur zeigt mit einer gepunkteten Linie, wo sie gerade spielen würde. So holst du sie punktgenau wieder rein.
- **SETUP → Stems exportieren** schreibt jede Spur mit Loop als 24-Bit-WAV (`livelooper-track1-120bpm.wav`) mit Tempo- und Taktangaben (ACID). Die meisten DAWs legen sie dadurch direkt im Raster ab.

![8 Spuren](../images/livelooper-8-tracks-de.png)

## 22. Loops in die DAW ziehen

Pack die **Wellenform** im Display einer Spur und zieh sie aus livelooper heraus:

- auf eine **Audiospur** in deiner DAW – der Loop wird ein Clip bzw. eine Region (in Ableton Live in der Arrangement- oder Session-Ansicht),
- in den **Browser** deiner DAW oder auf den Schreibtisch / Finder / Explorer.

livelooper schreibt dabei eine neue 24-Bit-WAV mit Tempo- und Taktangaben (ACID-Metadaten, `livelooper-track1-120bpm-<Datum>.wav`). Die meisten DAWs legen sie dadurch in der richtigen Länge ins Raster (Ableton Live etwa warpt sie automatisch). Die Dateien bleiben in *Musik → livelooper → Dragged Loops*, weil das DAW-Projekt sie dort weiter verwendet – mit der Funktion deiner DAW zum Sammeln/Konsolidieren von Dateien kopierst du sie ins Projekt (in Live: *Datei → Alles sammeln und sichern*). Zieht man eine solche Datei zurück auf livelooper, passiert nichts. Über einer ziehbaren Wellenform wird der Mauszeiger zur Hand; leere Spuren oder Spuren, die gerade aufnehmen, lassen sich nicht ziehen.

## 23. Live spielen

livelooper ist zum Spielen gedacht, wie ein Instrument. Ein paar Gewohnheiten, die auf der Bühne helfen:

- **Hände an die Hardware**: Leg die Bedienung auf einen [Ableton Push](PUSH.md) oder einen MIDI-Controller ([MIDI](MIDI.md)) – dann brauchst du nie die Maus.
- **Im Fluss bleiben**: Quantisierung *Takt*, feste Loop-Längen und Auto-Aufnahme – jeder Druck sitzt, auch wenn du gerade spielst.
- **Erst improvisieren, dann aufnehmen**: Spiel einfach – wenn etwas Gutes passiert, drück **CAPTURE**.
- **Einen Bogen bauen**: Strophe, Refrain und Breakdown als **Szenen** speichern und im Takt wechseln.
- **Schichten formen statt stapeln**: Feedback unter 100 % lässt alte Schichten ausklingen, Tonhöhe und Reverse machen aus einer Phrase etwas Neues, das FX-XY-Pad auf dem Push ist für Drops und Steigerungen gemacht.
- **Gute Momente behalten**: Zieh einen Loop auf eine Spur in deiner DAW oder exportiere nach dem Auftritt die Stems.

## 24. Performance-Moves (seit 1.4)

Neue Funktionen fürs Live-Spielen. Sie sind Host-Parameter (MIDI Learn, MIDI-Mapping deiner DAW, Tastatur) und Gesten am [Ableton Push](PUSH.md):

- **Loop-Cutting** (*Slice 1 – 6*): springt zur nächsten 16tel-Note an eine von sechs Positionen eines spielenden Loops (mit Quantisierung *Aus* sofort). Kurz gedrückt bleibt er dort, länger als ¼ s gehalten wiederholt er das Stück (eine ¼-, ⅛- oder 1/16-Note) und springt beim Loslassen dorthin zurück, wo der Loop jetzt wäre – synchron. *Zurück in Sync* beendet einen Schnitt. Sprünge werden übergeblendet und knacken nie.
- **Stumm** / **Solo**: Eine Spur (oder alle anderen) stummschalten, ohne sie zu stoppen – sie läuft im Takt weiter und kommt genau an der richtigen Stelle zurück.
- **Loop verdoppeln**: Der Loop wird verdoppelt (doppelt so lang) – overdubbe eine Variation über die zweite Hälfte. **Loop halbieren** behält die erste Hälfte. Beides geht bei spielenden oder gestoppten Spuren und lässt sich mit UNDO zurücknehmen.
- **Oktave hoch / runter**: Tonhöhe +12 / −12 Halbtöne.
- **In leere Spur kopieren**: kopiert den Loop in die nächste leere Spur (gestoppt; gestartet läuft sie synchron mit dem Original).
- **Aufnahmelänge**: eine feste Länge (1 – 32 Takte) für die nächste oder die laufende erste Aufnahme, ohne die Takte-Einstellung der Spur zu ändern.
- **Tape-Stopp**: Die Loops werden langsamer wie ein anhaltendes Tonband; zurück auf 0 % laufen sie wieder an und spielen synchron weiter – die Zeitachse hält nie an.
- **Performance-Filter**: ein DJ-Filter auf den Loops (links Tiefpass, rechts Hochpass, in der Mitte aus).
