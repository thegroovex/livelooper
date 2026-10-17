# Bedienungsanleitung

[English](../en/USER-GUIDE.md) | **Deutsch**

livelooper ist als **Instrument zum Erforschen und Live-Spielen** gebaut: Jedes Bedienelement reagiert sofort und landet auf dem Schlag, und alles lässt sich mit einem [Ableton Push](PUSH.md), jedem MIDI-Controller oder der Computertastatur spielen. Tipps fürs Live-Spielen stehen in Abschnitt 23. Wie du das Plug-in in deine DAW einbindest (Looper-Spur, Monitoring, Software-Instrumente loopen, Quellen wechseln), steht in [DAW-EINRICHTUNG.md](DAW-EINRICHTUNG.md).

![Oberfläche](../images/livelooper-de.png)

Die Oberfläche ist wie ein Hardware-Looper aufgebaut. Die Bezeichnungen in dieser Anleitung entsprechen der deutschen Oberfläche; bei englischer Oberfläche steht die englische Bezeichnung in Klammern (siehe auch [Kapitel 7](#7-sprache) und die [Übersicht nach Abschnitt 15](#bezeichnungen-deutsch--englisch)).

Die Oberfläche hat die klassische Anordnung: alle Effekte oben in zwei Reihen (alles gleichzeitig sichtbar, keine Reiter), darunter die Loop-Spuren – der größte Teil, jede mit eigener Farbe, großem leuchtendem Pad und eigener Pegelanzeige – und rechts in einer schmalen Spalte Rhythmus und Master:

| Bereich | Inhalt |
|---|---|
| Kopfzeile | **LIVE** neben dem Logo (Live-Ansicht, Abschnitt 43), Display (Tempo, Takt/Schlag, Quantisierung, Link, Master-VU in dB, Meldungen), **MIDI LEARN**, **MIDI MAP**, **SETUP**, **?** (Hilfe), der runde **REC**-Knopf (Performance-Recorder) |
| FX-Reihe 1 | **EINGANGS-FX** A–D, **EINGANGS-FX … BEARBEITEN** (Typ, zwei Parameter, Mix des gewählten Slots), **SPUR-FX** A–D, **PUNCH-FX** (Abschnitte 3, 39) |
| FX-Reihe 2 | **BEAT-FX**, **REPEAT / STUTTER**, **COLOR-FX**, **SEND-FX**, **METRONOM** (Abschnitte 25–28, 39) |
| SPUR 1–5 (oder 1–8) | die Loop-Spuren (SETUP → Looper → Spuren), von oben nach unten: Titelzeile mit der **Loop-Länge**-Pille (AUTO / 1–16 Takte, Abschnitt 45), Display mit Status und Wellenform, RÜCKW. / 1SHOT / FX, Pegel-Fader mit IN/OUT-Anzeige, PAN / COLOR, ZURÜCK / VOR / SETUP, das große REC/PLAY-Pad, STOP |
| RHYTHMUS | Drumcomputer, Tempo, Tap, FILL, BEATS |
| MASTER | Quantisierung, Host-Sync, Pegel, Fade-Zeit, ALLE START / STOP, CAPTURE, ALLE LÖSCHEN |
| Fußleiste | 8 Szenen-Knöpfe, SPEICHERN |

Das Fenster lässt sich an der rechten unteren Ecke größer und kleiner ziehen; die Oberfläche skaliert mit. Feste Größen (75 – 200 %), helles Farbschema, Bühnenmodus und große Wellenformen: *SETUP → Anzeige* (Abschnitt 44). Fährt man mit der Maus über ein Element, erscheint ein Hinweis.

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
| **Loop-Länge** (Pille neben *SPUR n*, neu in 1.7) | **AUTO** oder **1 · 2 · 4 · 8 · 16 TAKTE**: die erste Aufnahme endet nach so vielen Takten von selbst. Klick = Menü, Mausrad / nach oben oder unten ziehen = ändern, Rechtsklick = MIDI-Learn. Abgedunkelt, solange die Spur einen Loop enthält (gilt dann für die nächste neue Aufnahme). Siehe [Abschnitt 45](#45-loop-länge-neu-in-17) |
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
| Loop-Länge (Loop length) | Auto · 1 · 2 · 4 · 8 · 16 | Auto = Länge durch zweiten Druck; feste Zahl = Aufnahme endet automatisch nach so vielen Takten. Dieselbe Einstellung wie die Loop-Länge-Pille in der Spur (vor 1.7 *Takte*) |
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

**Host-Transport folgen** (SETUP → Looper, **seit 1.7 standardmäßig an**): Loops starten mit der Play-Taste deiner DAW und **stoppen mit ihrer Stop-Taste** – stoppt die DAW, stoppt livelooper. Schalte es aus, wenn livelooper bei stehender DAW mit seiner internen Clock weiterloopen soll. Die Standalone-App hat keinen Host-Transport und ist nicht betroffen; mit einer älteren Version gespeicherte Projekte behalten ihre gespeicherte Einstellung.

Hinweis: Ändert sich das Tempo, nachdem ein Loop aufgenommen wurde, wird der Loop nicht zeitgedehnt – er spielt in seiner Originalgeschwindigkeit weiter.

---

## 3. Effekte

Es gibt zwei Effektbänke mit je vier Slots:

- **EINGANGS-FX** (INPUT FX) bearbeiten das Eingangssignal – was du hörst, wird so auch aufgenommen.
- **SPUR-FX** (TRACK FX) bearbeiten alle Spuren, bei denen **FX** eingeschaltet ist (gemeinsamer Effektweg), ohne die Aufnahme zu verändern.

Beide Bänke liegen oben in der ersten FX-Reihe (**EINGANGS-FX** links, **SPUR-FX** rechts). Das Pad **A/B/C/D** schaltet den Slot ein und aus und wählt ihn gleichzeitig zum Bearbeiten aus; ein Klick auf den Effektnamen darunter wählt ihn nur aus. Direkt daneben (**… BEARBEITEN**) stellst du Typ, zwei Parameter und Mix ein. Die Slots einer Bank sind in Reihe geschaltet (A → B → C → D).

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
| **Kit** | **Studio** (straffes, modernes akustisches Kit), **808** (tiefe Boom-Kick, knackige Hi-Hats), **909 Style** (druckvoll für House / Techno), **Lo-Fi** (warm, staubig, weiche Höhen) – alle vier spielen aufgenommene Drum-Samples (CC0 / gemeinfrei, siehe [THIRD-PARTY.md](../../THIRD-PARTY.md)) mit mehreren Anschlagstufen und Round Robin |
| **PEGEL** | Lautstärke des Rhythmus |
| **SWING** | verzögert jede zweite Sechzehntel. 50 % = gerade, 54–58 % = leichter Groove, 66 % = Triolen-Shuffle, 75 % = maximal. Tipp: Hip Hop / Lo-Fi 56–60 %, UK Garage 60–64 %, Neo Soul 60–66 % |
| **FEEL** | Humanisierung: kleine Schwankungen im Timing (ein paar ms später, nie früher) und in der Anschlagstärke. 0 % = maschinengenau |
| **TEMPO / TAP** | internes Tempo |

Jedes Muster hat Akzente und Ghost-Notes, Hi-Hat-Rolls (Trap) und Percussion (Rimshot, Shaker, Tom). Snare, Clap und Rimshot klingen in einem kurzen Raum. Das Metronom wird nie geswingt.

Die Kit-Namen beschreiben einen Klangstil; „909 Style“ und „808“ sind nicht die Original-Geräte und bedeuten keine Unterstützung durch einen Hersteller.

**Groove** (*SETUP → Rhythmus-Groove*, seit 1.5): das Gefühl eines Drummers – kleine, feste Timing-Verschiebungen pro Instrument und eine Akzentkurve, zusätzlich zu SWING und FEEL:

| Groove | Gefühl |
|---|---|
| Gerade (Straight) | maschinengenau |
| Natürlich (Natural, Standard) | ein paar ms menschliche Lockerheit, beim Backbeat leicht dahinter |
| Laid Back | Snare und Hi-Hats deutlich hinter dem Schlag – Hip Hop, Neo Soul |
| Treibend (Pushed) | alles leicht vor dem Schlag – drängend, nach vorn |
| Drunk | locker, wackelig, hinter dem Schlag (wie späte Beat-Tapes) |
| Drive | Kick und Hi-Hats auf dem Schlag – Rock, Techno |

**Fills** (*SETUP → Rhythmus-Fills*): *Aus*, *Alle 4 Takte* oder *Alle 8 Takte* – der Drummer spielt im letzten Schlag vor der nächsten Phrase einen kurzen Fill (Snare- / Tom-Lauf).

**Seit 1.6:** 8 weitere Stil-Kits mit passenden Grooves und Fills, lebendigere Kicks (VARIATION), der Step-Sequencer **BEATS** für eigene Patterns, **FILL** auf Knopfdruck, auf Wunsch die Drums durch Beat FX / Repeat und Sound-Packs – siehe [Drums, BEATS und Sound-Packs](#drums-beats-und-sound-packs-16).

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
| CAPTURE | die letzten 1–32 Takte werden ein Loop (Abschnitt 16) |

---

## 6. SETUP-Menü

Seit 1.6 öffnet **SETUP** ein Einstellungsfenster im Stil der macOS-Systemeinstellungen: links die Seiten (**Sets**, **Looper**, **Rhythmus**, **Aufnahme**, **Audio**, **Controller**, **Anzeige**, **Sync**, **App**; seit 1.7 gibt es die Seite **Audio** immer – sie enthält die Latenzmessung – und **Anzeige** enthält Farbschema, Oberflächengröße, Bühnenmodus, Wellenformen und die Live-Ansicht), rechts gruppierte Zeilen – Schalter, Aufklappmenüs und Aktionen, darunter kurze Hinweise. Jeder Eintrag unten steht auf einer dieser Seiten.

![SETUP](../images/livelooper-setup-de.png)

| Eintrag | Bedeutung |
|---|---|
| Aufnahme-Aktion (Record action) | „Rec > Dub > Play" (Standard) oder „Rec > Play > Dub". Die Beschriftung der REC-Taste („DUB / PLAY" bzw. „PLAY / DUB") und die unterste Displayzeile zeigen den aktiven Modus. |
| Host-Transport folgen | Loops starten/stoppen mit dem Host-Transport (seit 1.7 standardmäßig an) |
| Maximale Loop-Länge | 30 · 60 · 120 · 240 · 480 Sekunden pro Spur (bestimmt den Speicherbedarf; vorhandene Loops bleiben erhalten) |
| Loops im Projekt speichern | Loops im DAW-Projekt speichern (Standard an). Aus = kleinere Projekte, die Loops sind nach dem Laden weg |
| Steuerung mit der Computertastatur | nur Standalone-App: Tasten an/aus (Abschnitt 10) |
| Tastaturbelegung … | nur Standalone-App: eigene Tastenbelegung (Abschnitt 10) |
| Tempo eintippen (BPM) … | internes Tempo eintippen (Abschnitt 11) |
| Language / Sprache | Automatisch (Systemsprache) · English · Deutsch |
| Zuletzt verwendete Sets | die letzten 10 gespeicherten oder geladenen Sets, ein Klick lädt (Abschnitt 13) |
| Capture-Länge | 1 · 2 · 4 · 8 · 16 · 32 Takte (Abschnitt 16) |
| Audio → Latenz | Latenz messen, Latenzausgleich (an), manueller Versatz (Abschnitt 41) |
| Controller → Anschlagkurven / Push-1-Display / MIDI-Feedback | Anschlagkurven pro Controller, Loop-Fortschrittsbalken im Push-1-Display, LED-Feedback für gelernte Bedienelemente (Abschnitt 42) |
| Anzeige | Farbschema, Oberflächengröße 75 – 200 %, Bühnenmodus, Wellenformen in den Loop-Spuren, Live-Ansicht (Abschnitte 43, 44) |
| Rhythmus-Groove / Rhythmus-Fills | Gefühl und Fills des Drumcomputers (Abschnitt 4) |
| Ableton Link | Link an / aus, Start / Stopp synchronisieren, Anzahl Teilnehmer (Abschnitt 29) |
| Einstellungen beim Start | mit den zuletzt verwendeten oder den Standard-Einstellungen starten, jetzt zurücksetzen (Abschnitt 31) |
| Updates | jetzt prüfen, automatisch prüfen, automatisch installieren (Abschnitt 32) |
| Hilfe … / Über livelooper … | Hilfe in der App und Info-Fenster (Abschnitte 30 und 32) |

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
- oberste Reihe: **Master-VU** des Hauptausgangs in dBFS, links und rechts, mit Skala von -60 bis +6 dB und dem Spitzenwert als Zahl – grün bis -12 dB, gelb bis -3 dB, orange bis 0 dB, rot darüber (Abschnitt 12)
- Zeile 1: Tempoquelle (HOST, INTERN, interne Clock) und **Link**-Status: *LINK 2 TEILNEHMER*, solange Link das Tempo vorgibt, *LINK: HOST-SYNC ZUERST*, wenn in einer DAW HOST-SYNC Vorrang hat
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

- **Spur-Pegel** (Stereo, neben dem Fader; standardmäßig an – ausblenden mit **SETUP → Looper → Pegelanzeigen pro Spur**): Während eine Spur aufnimmt oder overdubbt, zeigt er den **Eingangspegel** (orange, Beschriftung **IN**), sonst den **Ausgangspegel** der Spur (grün, **OUT**). Gelb ab -12 dB, rot ab -1 dB; der dünne Strich hält die Spitze etwa eine Sekunde.
- **Master-VU** (seit 1.5): Die oberste Zeile im Display (**MASTER**) zeigt den Pegel des Hauptausgangs in dBFS, links und rechts, auf einer Skala von -60 bis +6 dB, rechts den Spitzenwert als Zahl. Grün bis -12 dB, gelb bis -3 dB, orange bis 0 dB, rot darüber – laute Stellen am besten im Gelben halten.
- **Wellenform**: Das Spur-Display zeigt statt eines Fortschrittsbalkens eine Übersicht des Loops. Der schon gespielte Teil ist hell, ein weißer Strich markiert die Abspielposition. Bei der Aufnahme wächst die Wellenform von links nach rechts; Overdubs und Undo sind sofort sichtbar. Bei REVERSE wird sie gespiegelt.
- **Clip-LED** (oben rechts an jeder Spur): aus = leer, grün = Spur enthält einen Loop, rot = erste Aufnahme läuft, rot blinkend = wartet auf den nächsten Takt, um aufzunehmen.

## 13. Looper-Sets

Ein Set speichert den kompletten Looper in einer Datei (`.llset`): alle Loops, Pegel, Pan, Spur-Einstellungen, Effekte, Rhythmus, Tempo und Quantisierung.

- **SETUP → Set speichern ...** schreibt ein Set (ein Set enthält immer seine Loops, auch wenn *Loops mit dem Projekt speichern* aus ist).
- **SETUP → Set laden ...** lädt eins; alternativ eine `.llset`-Datei auf das Fenster ziehen.
- MIDI-Zuweisungen, Tastatur-Belegung und Cue-Routing gehören zur App und bleiben beim Laden eines Sets erhalten.
- **SETUP → Zuletzt verwendete Sets** listet die letzten 10 gespeicherten oder geladenen Sets (nicht mehr vorhandene Dateien fallen weg); *Liste leeren* leert sie.

## 14. Cue-Ausgang (Standalone-App)

Die Standalone-App hat einen zweiten Ausgangsbus für **Rhythmus und Metronom**: Du hörst den Click im Kopfhörer, das Publikum nicht.

1. Unter **Audio-/MIDI-Einstellungen** (oben links im Fenster) die zusätzlichen Ausgänge des Audio-Interfaces aktivieren (Ausgang 3 und höher).
2. **SETUP → Cue-Ausgang (Click / Rhythmus)**: Ausgangspaar wählen (z. B. *Ausgänge 3 / 4*) und festlegen, wo der Rhythmus spielt: **Hauptausgang**, **Nur Cue-Ausgang** oder **Haupt- und Cue-Ausgang**.

Ohne Cue-Paar (und im Plug-in) spielt der Rhythmus immer auf dem Hauptausgang – der Click geht also nie verloren. Die Einstellung wird mit der App gespeichert.

## 15. Metronomklang

**SETUP → Metronomklang** (oder das Klang-Menü im **METRONOM**-Block, Abschnitt 28): **Piep** (klassisch), **Holzblock** (kurzer, hölzerner Schlag), **Klick** (trockener Tick ohne Tonhöhe) oder **Rimshot**.

## Bezeichnungen Deutsch / Englisch

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
| Loop-Länge (vor 1.7 Takte) | Loop length (before 1.7 Measures) |
| LEER · STOPPT · WARTET · ENDE · GELÖSCHT | EMPTY · STOPPING · WAIT · END · CLEARED |
| BEAT-FX · Kanal · PEGEL | BEAT FX · Channel · LEVEL |
| COLOR-FX | COLOR FX |
| METRONOM · BETONUNG | METRONOME · ACCENT |
| Einstellungen beim Start | Settings on start |
| Loop-Länge einmalig (vor 1.7 Aufnahmelänge) | Loop Length Once (before 1.7 Rec Length) |
| Live-Ansicht · LIVE BEENDEN | Live view · EXIT LIVE |
| Anzeige · Farbschema: Dunkel · Hell | Display · Colour theme: Dark · Light |
| Oberflächengröße · Bühnenmodus · Wellenformen zeigen | Interface size · Stage mode · Show waveforms |
| Latenz messen · Latenzausgleich · Manueller Versatz | Measure latency · Latency compensation · Manual offset |
| Anschlagkurven: Weich · Normal · Hart · Fest | Velocity curves: Soft · Normal · Hard · Fixed |
| Host-Transport folgen | Follow host transport |
| Zuletzt verwendete Sets | Last used sets |

## 16. Capture (rückwirkende Aufnahme)

Du hast etwas Gutes gespielt, ohne Aufnahme zu drücken? Drück **CAPTURE** (MASTER-Bereich). Die zuletzt gespielten Takte werden ein Loop auf der ersten leeren Spur und laufen sofort im Takt weiter. Die Länge stellst du unter **SETUP → Capture-Länge** ein (1, 2, 4, 8, 16 oder 32 Takte; am Push: FIXED LENGTH). Wurde weniger gespielt, nimmt livelooper die größte Länge, die passt. Im SETUP jeder Spur gibt es außerdem einen **CAPTURE**-Knopf für genau diese Spur. **UNDO** entfernt einen Capture-Loop wieder.

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
- Weitere Wege, Audio aus livelooper in die DAW zu holen: [DAW-EINRICHTUNG.md, Abschnitt 5](DAW-EINRICHTUNG.md#5-audio-herausholen).

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
- **Mehrere Instrumente, ein Looper**: alle in die Looper-Spur routen und durch Spielen, Stummschalten oder Scharfschalten wechseln – siehe [DAW-EINRICHTUNG.md, Abschnitt 4](DAW-EINRICHTUNG.md#4-beim-spielen-zwischen-instrumenten-und-quellen-wechseln).

## 24. Performance-Moves (seit 1.4)

Neue Funktionen fürs Live-Spielen. Sie sind Host-Parameter (MIDI Learn, MIDI-Mapping deiner DAW, Tastatur) und Gesten am [Ableton Push](PUSH.md):

- **Loop-Cutting** (*Slice 1 – 6*): springt zur nächsten 16tel-Note an eine von sechs Positionen eines spielenden Loops (mit Quantisierung *Aus* sofort). Kurz gedrückt bleibt er dort, länger als ¼ s gehalten wiederholt er das Stück (eine ¼-, ⅛- oder 1/16-Note) und springt beim Loslassen dorthin zurück, wo der Loop jetzt wäre – synchron. *Zurück in Sync* beendet einen Schnitt. Sprünge werden übergeblendet und knacken nie.
- **Stumm** / **Solo**: Eine Spur (oder alle anderen) stummschalten, ohne sie zu stoppen – sie läuft im Takt weiter und kommt genau an der richtigen Stelle zurück.
- **Loop verdoppeln**: Der Loop wird verdoppelt (doppelt so lang) – overdubbe eine Variation über die zweite Hälfte. **Loop halbieren** behält die erste Hälfte. Beides geht bei spielenden oder gestoppten Spuren und lässt sich mit UNDO zurücknehmen.
- **Oktave hoch / runter**: Tonhöhe +12 / −12 Halbtöne.
- **In leere Spur kopieren**: kopiert den Loop in die nächste leere Spur (gestoppt; gestartet läuft sie synchron mit dem Original).
- **Loop-Länge einmalig** (vor 1.7 *Aufnahmelänge*): eine feste Länge (1 – 32 Takte) nur für die nächste oder die laufende erste Aufnahme, ohne die Loop-Länge-Einstellung der Spur zu ändern (siehe Abschnitt 45).
- **Tape-Stopp**: Die Loops werden langsamer wie ein anhaltendes Tonband; zurück auf 0 % laufen sie wieder an und spielen synchron weiter – die Zeitachse hält nie an.
- **Performance-Filter**: ein DJ-Filter auf den Loops (links Tiefpass, rechts Hochpass, in der Mitte aus).

## 25. Beat-FX (seit 1.5)

![Performance-Reihe](../images/livelooper-performance-de.png)

Der **BEAT-FX**-Block (zweite FX-Reihe links; **REPEAT / STUTTER** und **COLOR-FX** liegen daneben) funktioniert wie die Beat-FX-Sektion eines DJ-Mixers: ein Effekt, fest am Schlag, den du beim Spielen ein- und auswirfst.

| Element | Funktion |
|---|---|
| Typ (oben links) | der Effekt, siehe Tabelle |
| Kanal (darunter) | wo er wirkt: **Master** (alles), **Loops** (alle Spuren), **Eingang** (was du live spielst) oder **Spur 1 … 8** |
| **<  1/32 … 16  >** | Schlagwert: die Länge des Effekts in Schlägen (1/32, 1/16, 1/8, 1/4, 1/2, 3/4, 1, 2, 4, 8, 16 – 16 Schläge = 4 Takte). Die Pfeile schalten weiter |
| **PEGEL** (LEVEL) | Pegel / Tiefe des Effekts (was er tut, hängt vom Typ ab) |
| **ON** | Effekt an / aus (rastet ein) |
| **HOLD** | Effekt an, solange gehalten – für einen kurzen Einwurf |

| Typ | Was er tut |
|---|---|
| Delay | sauberes Echo im Schlagwert |
| Echo | Echo, das natürlich ausklingt, auch nach dem Ausschalten |
| Ping Pong | Echo, das links / rechts springt |
| Spiral | Echo, das ansteigt und in sich selbst zurückläuft |
| Hall (Reverb) | großer Raum, klingt nach dem Ausschalten aus |
| Trans | rhythmische Schnitte (Lautstärke-Gate) im Schlagwert |
| Filter | Filter, das im Schlagwert wandert |
| Flanger / Phaser | Sweep-Zyklus im Schlagwert |
| Pitch | Tonhöhe verschieben (PEGEL = Tonhöhe) |
| Roll | wiederholt den letzten Schlagwert (wie Beat Repeat) |
| Slip Roll | Roll, der beim Loslassen dorthin zurückkehrt, wo die Musik jetzt ist |
| Reverse Roll | der Roll läuft rückwärts |
| Vinyl Brake | der Klang bremst ab wie ein stoppender Plattenspieler |
| Helix | ein Roll, der mit jeder Wiederholung höher wird |
| Noise | gefilterter Rausch-Sweep für Steigerungen |

Umschalten wird überblendet, Echo- und Hallfahnen klingen aus. Jedes Element ist ein Host-Parameter (MIDI Learn, DAW-Mapping) und liegt auf dem [Ableton Push](PUSH.md#performance-fx-seit-15) (ADD EFFECT, ADD TRACK, LEFT / RIGHT, BROWSE).

## 26. Beat Repeat und Stutter

**BEAT REPEAT / STUTTER** fängt ein Stück der Musik ein und wiederholt es:

- **REPEAT** (halten): Ab der nächsten Rasterlinie des gewählten Timings (**1/2, 1/4, 1/8, 1/16, 1/32** Note) wird das Stück wiederholt, solange du den Knopf hältst.
- **STUTTER** (halten): dasselbe mit einem kurzen Gate bei jeder Wiederholung – ein abgehacktes Maschinengewehr-Stottern. Es hat ein eigenes Timing.
- **LATCH**: hält es ohne Festhalten an; nochmal drücken beendet es.

Es wirkt auf den im BEAT-FX-Block gewählten Kanal. Die Musik läuft darunter weiter – beim Loslassen bist du also genau wieder auf dem Schlag. Am Push: **REPEAT** / **ACCENT**, dabei über den Touchstrip streichen für 1/2 … 1/32.

## 27. Color-FX

Ein Ein-Regler-Effekt pro Spur, wie die Color-FX eines DJ-Mixers: Die sechs Knöpfe im **COLOR-FX**-Block wählen den Effekt, der **COLOR**-Regler jeder Spur stellt ihn ein – **Mitte = aus**, nach links oder rechts für mehr.

| Effekt | Links | Rechts |
|---|---|---|
| Filter | Tiefpass | Hochpass |
| Dub Echo | dunkles punktiertes Achtel-Echo | helles Echo |
| Space | Hall | Hall |
| Crush | Bit-Crush, dunkler | Bit-Crush, dünner |
| Noise | dunkles Rauschen | helles Rauschen |
| Sweep | Gate in Sechzehnteln | Bandpass, der nach oben wandert |

In der Mitte bleibt die Spur unberührt (bit-genau). Der Regler ist pro Spur zuweisbar (*Color 1 … 8*).

## 28. Metronom-Block

Der **METRONOM**-Block spielt einen Klick unabhängig vom RHYTHMUS: **METRONOM** = an / aus, **BETONUNG** = lautere Eins, das Menü darunter = Klang (Piep, Holzblock, Klick, Rimshot), **PEGEL** = Lautstärke. In der Standalone-App kann der Klick nur auf den Cue-Ausgang gehen (Abschnitt 14), damit das Publikum ihn nicht hört. Am Push: SHIFT + TAP TEMPO.

## 29. Ableton Link

livelooper kann Tempo und Schlag mit anderen Apps und Geräten im selben Netzwerk teilen (Ableton Live, andere Link-Apps auf Mac, PC, iPhone oder iPad):

1. **SETUP → Ableton Link → Ableton Link (Tempo und Beat mit anderen Apps)** – oder am Push SHIFT + METRONOME.
2. Das Display zeigt *LINK 1 TEILNEHMER*, *LINK 2 TEILNEHMER* … – livelooper folgt Tempo und Schlag der Session; TEMPO oder TAP ändern das Tempo für alle.
3. **Start / Stopp mit Link synchronisieren** (optional): Auch Starten und Stoppen wird geteilt.

Das Beitreten ändert nie das Tempo einer Session – livelooper übernimmt das Tempo der anderen. In der **Standalone-App** gibt Link den Takt von livelooper vor. In einer **DAW** hat HOST-SYNC Vorrang (das Display zeigt *LINK: HOST-SYNC ZUERST*) – schalte HOST-SYNC aus, um Link zu folgen, oder nutze das Link deiner DAW. Link braucht keine Einrichtung am Computer; alle Geräte müssen nur im selben Netzwerk sein.

## 30. Hilfe

Der Knopf **?** (oben rechts) oder **SETUP → App → Hilfe …** öffnet die Hilfe. Sie beginnt mit bebilderten Seiten (seit 1.6): **Willkommen**, **Loops**, **FX**, **BEATS**, **Aufnahme**, **Link**, seit 1.7 **Loop-Länge**, **Live-Ansicht**, **Anzeige**, **Latenz** und **Controller** (Bilder der echten Oberfläche), **Push 1** (der ganze Controller mit nummerierten Tasten und ihrer Funktion – mit den 1.7-Gesten SHIFT + MASTER und gehaltenem FIXED LENGTH), **Push 1: STEP** und **K.O. II**. Danach folgen diese Bedienungsanleitung, die Push-Anleitung, die MIDI-Anleitung und die Installationsanleitung in deiner Sprache, in Abschnitte aufgeteilt, mit Suchfeld (z. B. *Stutter*, *Link*, *Capture*). Fährt man mit der Maus über ein Element, erscheint ein kurzer Hinweis.

![Hilfe](../images/livelooper-help-de.png)

## 31. Einstellungen beim Start

Beim ersten Start fragt livelooper einmal, wie es künftig starten soll:

- **Letzte Einstellungen**: Jeder Start – und jedes neue Plug-in ohne gespeichertes Projekt – nutzt die zuletzt verwendeten Einstellungen: Effekte, Rhythmus, Metronom, MIDI- und Tastatur-Zuordnung, Spuren, Push (keine Loops).
- **Standard-Einstellungen**: immer frisch starten.

Jederzeit änderbar unter **SETUP → Einstellungen beim Start**; *Alle Einstellungen jetzt zurücksetzen (Loops bleiben)* setzt die Bedienelemente zurück, ohne die Loops zu löschen. Ein DAW-Projekt lädt immer seinen eigenen gespeicherten Zustand.

## 32. Info und Updates

**SETUP → Über livelooper …** zeigt die Version (mit Build-Nummer, Git-Hash und Datum), die Lizenz (AGPLv3) und die Hinweise zu Drittkomponenten. **Nach Updates suchen** fragt bei GitHub nach einer neueren Version:

- **Standalone-App**: *Herunterladen und installieren* – die neue Version wird geladen, geprüft und ersetzt die App, wenn du livelooper beendest.
- **Plug-in**: Das Update wird geladen und vorbereitet; installiere es bei geschlossener DAW (Knopf *Plug-ins installieren* oder mit der Standalone-App). Ein Plug-in wird nie ersetzt, solange eine DAW es benutzt.

*Beim Start automatisch prüfen* und *Updates automatisch installieren* sind aus, bis du sie einschaltest (auch unter **SETUP → Updates**).

![Info](../images/livelooper-about-de.png)


<!-- v1.6 features (branch v1.6-wip) -->
## 33. Demo-Set und geführte Tour (neu in 1.6)

Beim **allerersten Start** zeigt livelooper seinen **Willkommensbildschirm** und lässt dich wählen, wie du startest:

- **DEMO-SET LADEN** – ein Drum-Groove (100 BPM) und drei kurze Loops – Bass, Keys und ein Arpeggio – auf den Spuren 1 – 3. Drück **ALLE START / STOP** und du hörst nach Sekunden Musik; probier die Szenen 1 und 2, die Beat-FX und die Loop-Tricks. Die Loops sind eigens für livelooper erzeugt und frei verwendbar (CC0).
- **MIT LEEREM SET STARTEN** – ein leerer Looper für deinen ersten eigenen Loop.

Der Hinweis auf dem Bildschirm sagt es: **Das Demo-Set ersetzt das aktuelle Set** (Loops und Einstellungen), und ein leeres Set kannst du jederzeit mit **SETUP → Leeres Set (alles löschen)** laden. Nach deiner Wahl startet die geführte Tour.

![Willkommensbildschirm](../images/livelooper-welcome-de.png)

Das Demo spielt über deine Lautsprecher, deshalb **startet es mit stummem Eingang** (Regler **EINGANG** im MASTER auf 0) – keine Rückkopplung durch ein offenes Mikrofon. Aufnehmen funktioniert trotzdem; dreh **EINGANG** auf, um zu hören, was du spielst.

- **SETUP → Willkommen / Demo-Set …** zeigt denselben Bildschirm jederzeit (ohne Tour). **SETUP → Demo-Set laden** lädt das Demo direkt.
- Enthält das aktuelle Set Loops, **fragt livelooper nach**, bevor das Demo-Set (oder das leere Set) es ersetzt.
- Ein DAW-Projekt oder ein geladenes Set wird nie ohne deine Wahl ersetzt.

Direkt nach dem ersten Start zeigt eine **geführte Tour** (etwa 60 Sekunden) die fünf Dinge, die du zuerst ausprobieren solltest: **Aufnehmen → Overdub → Groove → Beat-FX → Szene**. Jeder Schritt hebt das Bedienelement hervor und erklärt es; **WEITER**, **ZURÜCK** oder **TOUR ÜBERSPRINGEN**. Du startest sie jederzeit wieder über die Hilfe-Taste **?** → *Geführte Tour (60 Sekunden)* oder **SETUP → Geführte Tour**.

## 34. Loop-Tricks (neu in 1.6)

Drei Tricks schreiben den Loop einer Spur um – musikalisch auf den nächsten Schlag oder Takt (QUANTISIERUNG), und **ZURÜCK** (Undo) stellt den Loop exakt wieder her:

- **Halbe Geschwindigkeit**: Der Loop läuft halb so schnell, eine Oktave tiefer, und wird doppelt so lang (reicht der Speicher nicht, wird die erste Hälfte auf die alte Länge gedehnt).
- **Doppelte Geschwindigkeit**: doppelt so schnell, eine Oktave höher – zweimal hintereinander, Länge und Taktstruktur bleiben.
- **Loop-Audio umdrehen**: Die Aufnahme selbst wird umgedreht. (Der Schalter RÜCKW. spielt den Loop nur rückwärts ab; dieser Trick dreht die Aufnahme um, du kannst also vorwärts über den umgedrehten Klang overdubben.)
- **Loop verdoppeln / halbieren** (seit 1.4) ergänzen das: Loop × 2 (Kopie) und halbe Länge.

Alle Tricks sind Host-Parameter (*S1 Halbe Geschwindigkeit*, *S1 Doppelte Geschwindigkeit*, *S1 Loop-Audio umdrehen* …) für MIDI Learn und das MIDI-Mapping deiner DAW. Auf **Push 1**: SHIFT + OCTAVE DOWN = halbe Geschwindigkeit, SHIFT + OCTAVE UP = doppelte Geschwindigkeit, SHIFT + die Taste einer Spur in der zweiten Reihe über den Pads = diese Spur umdrehen (siehe [PUSH.md](PUSH.md)). Sie wirken auf laufende oder gestoppte Spuren, nicht während eine Spur aufnimmt.

## 35. Performance-Recorder (neu in 1.6)

Die runde rote **Aufnahmetaste** oben rechts, direkt unter MIDI LEARN / MIDI MAP / SETUP / ?, nimmt **alles auf, was livelooper spielt** – den Master-Mix, genau das, was am Hauptausgang zu hören ist – als WAV-Datei (Stereo, 24 Bit, aktuelle Samplerate). Während der Aufnahme **blinkt das rote Licht im Takt** und daneben steht die Zeit (*REC 02:13*); erneut klicken stoppt. Rechtsklick: MIDI Learn (Parameter *Performance-Recorder*). Die Datei heißt nach Datum und Uhrzeit (`livelooper-2026-10-14_21-30-05.wav`).

- Ordner: **Musik/livelooper/Recordings** – ändern unter **SETUP → Performance-Recorder → Ordner für Aufnahmen wählen …**, öffnen mit *Aufnahme-Ordner zeigen*.
- Funktioniert in der Standalone-App und im Plug-in (in einer DAW nimmt er die Ausgabe des Loopers auf seiner Spur auf; die Aufnahme der DAW brauchst du dafür nicht).
- Host-Parameter **Performance-Recorder** (an / aus) für MIDI Learn – z. B. ein Fußschalter – und Automation. Das Laden eines Projekts schaltet ihn nie ein.
- Die Datei wird im Hintergrund geschrieben; der Audio-Thread wartet nie auf die Festplatte.

## 36. Clip-Export 9:16 (neu in 1.6)

**SETUP → Clip-Export 9:16** macht aus den letzten Momenten deiner Performance ein Hochformat-Video für Reels, TikTok und Shorts:

- *Die letzten 20 Sekunden als 9:16-Video exportieren …* – livelooper hält immer die letzten 30 Sekunden seiner Ausgabe im Speicher, du entscheidest also **nach** einem guten Moment.
- *Die letzte Performance-Aufnahme als 9:16-Video exportieren …* – die letzte REC-Aufnahme (bis zu 90 Sekunden ab ihrem Anfang).

Das Video hat 1080 × 1920 Pixel, 30 Bilder pro Sekunde, H.264 + AAC (.mp4), mit animierter Wellenform, Abspielposition, bewegter Pegelanzeige, Zeit, dem livelooper-Titel und der Marke BeatTactics. Es wird im Hintergrund gerendert und im Aufnahme-Ordner gespeichert, dazu ein Titelbild (`…-cover.png`). Der Host-Parameter **9:16-Clip exportieren** (Trigger) startet den Export vom Controller aus.

So wird das Video kodiert: unter **macOS** mit AVFoundation, unter **Windows** mit Media Foundation (beides Teil des Betriebssystems). Unter **Linux** nutzt livelooper ein auf dem System installiertes **ffmpeg**, falls vorhanden. Ohne Video-Encoder schreibt der Export einen Ordner mit `audio.wav`, den PNG-Einzelbildern, einem Titelbild und `make-video.txt` mit dem ffmpeg-Befehl, der daraus ein Video macht.

## 37. Sets teilen (neu in 1.6)

**SETUP → Set zum Teilen speichern (.livelooper, eine Datei mit allem Audio) …** schreibt den ganzen Looper – Loops, Effekte, Rhythmus, Szenen, Tempo – in **eine Datei** mit der Endung `.livelooper`. Schick sie einem Freund, leg sie in die Cloud oder auf GitHub; sie öffnet sich auf Mac, Windows und Linux.

- **Öffnen**: SETUP → *Set laden …* (zeigt `.livelooper`- und `.llset`-Dateien) oder **die Datei auf das livelooper-Fenster ziehen** (Standalone-App und Plug-in).
- Innen ist es ein normales ZIP-Archiv: `set.xml` (Einstellungen), `audio/track1.flac` … (verlustfreie Loops), `info.json` (Tempo, Version) und ein kurzes README. Dein MIDI-Mapping, Tastatur-Mapping und Audio-Routing bleiben auf deinem Rechner, nicht im Set.
- `.llset`-Dateien (eine XML-Datei, seit 1.0) funktionieren weiter.

**Presets auf GitHub**: Der Ordner [`presets`](../../presets/) im livelooper-Repository enthält das Demo-Set (`demo-set.livelooper`). Eigene Sets teilen: als `.livelooper` speichern, prüfen, dass jeder Klang darin von dir ist oder frei geteilt werden darf, und einen Pull Request öffnen, der es in `presets/` legt (oder es an ein Issue anhängen).

## 38. Controller-Vorlagen (neu in 1.6, experimentell)

> **Experimentell – nicht auf Hardware getestet, Feedback willkommen.**


Neben [Push 1](PUSH.md) und MIDI Learn hat livelooper fertige Vorlagen für verbreitete Grid-Controller. Schließ den Controller an, livelooper erkennt ihn am Namen seines MIDI-Ports; **SETUP → Controller-Vorlagen** zeigt den Status und lässt dich selbst eine Vorlage wählen (oder die Vorlagen ausschalten). Unterstützt: **Launchpad Mini MK3, Launchpad X, APC mini mk2** (und das erste APC mini) und die Pads von **Launchkey MK3 / MK4**. Die Vorlagen folgen den öffentlichen Programmer's References der Hersteller (Notennummern, Modus-Umschaltung und LED-Farben daran geprüft) – **an der Hardware sind sie noch nicht getestet**; bitte melde, was nicht funktioniert.

8 × 8-Grid (Launchpad, APC mini) – eine Spalte pro Spur, Reihen von oben:

| Reihe | Pad | Mit SHIFT |
|---|---|---|
| 1 | REC / PLAY (Clip starten, aufnehmen, overdubben) | |
| 2 | STOP | LÖSCHEN |
| 3 | ZURÜCK (Undo) | VOR (Redo) |
| 4 | STUMM | |
| 5 | HALBE GESCHWINDIGKEIT | |
| 6 | DOPPELTE GESCHWINDIGKEIT | |
| 7 | LOOP UMDREHEN | |
| 8 | LOOP × 2 | LOOP / 2 |

- **Szenen-Tasten** (Launchpad rechte Spalte, APC-mini-Szenentasten): Szenen 1 – 8; mit SHIFT wird die Szene gespeichert.
- **Funktionstasten** (Launchpad obere Reihe, APC-mini-Track-Tasten von links): ALLE START / STOP, ALLE STOP, CAPTURE, TAP, RHYTHMUS, METRONOM, Performance-Recorder; die achte Launchpad-Taste (und SHIFT am APC mini) ist SHIFT.
- **APC-mini-Fader**: Pegel der Spuren 1 – 8 (Fader oben = 100 %), der neunte Fader ist der Master-Pegel.
- **Launchkey-Pads** (DAW-Modus): obere Reihe REC / PLAY, untere Reihe STOP für die Spuren 1 – 8; die Szenen-Taste (>) = ALLE START / STOP, die Taste darunter = ALLE STOP. Die Tasten (Keys) bleiben bei deiner DAW.
- **LEDs**: grün = läuft, rot pulsierend = Aufnahme, orange = Overdub, gelb blinkend = wartet auf den Takt, gelb gedimmt = gestoppt, dunkel = leer. Szenen: gedimmt = gespeichert, hell = zuletzt gestartet.

livelooper schaltet den Controller in seinen Programmer-/DAW-Modus, solange er ihn nutzt, und beim Loslassen zurück. Ports: Launchpad = sein *MIDI*-Port (unter Windows der zweite, `MIDIIN2 (…)`), Launchkey = sein *DAW*-Port (unter Windows der zweite), APC mini mk2 = sein erster Port. **Windows**: Ein MIDI-Port kann nur von einem Programm gleichzeitig benutzt werden – schalte den Controller in den MIDI-Einstellungen deiner DAW aus (wie bei Push).
<!-- end v1.6 features -->
---

## 39. Punch-FX und Send-FX (neu in 1.6)

![Punch-FX](../images/livelooper-punch-fx-de.png)

Das Feld **PUNCH-FX** (erste FX-Reihe rechts): zwölf schnelle Effekte, die du beim Spielen **hältst** –
wie die Punch-in-Effekte eines Taschen-Samplers. Mehrere zugleich kombinieren sich (TIEFPASS + HOCHPASS =
Bandpass). Jeder blendet in 5 ms ein und aus, damit nichts knackt.

| # | Punch-FX | STÄRKE bewirkt | X | Y |
|---|---|---|---|---|
| 1 | Tonhöhen-Zufall | wie weit die Tonhöhe fallen darf (0 … −12 Halbtöne) | Rate (1/32 … 1/4) | Wahrscheinlichkeit |
| 2 | Slice-Tausch | wie oft ein Slice getauscht wird | Slice-Größe (1/4 … 2 Schläge) | wie weit zurück |
| 3 | Granular | Korngröße | Dichte | Streuung (Position) |
| 4 | Beat-Repeat | Wiederholungslänge 1/4 → 1/32 (fester = kürzer), im Raster | Gate | Abklingen |
| 5 | Bandstopp | wie stark das Band abbremst | Stoppzeit | Anlaufzeit |
| 6 | Filter-Wobble | LFO-Tempo | Tiefe | Resonanz |
| 7 | Tiefpass | Cutoff nach unten | Resonanz | Drive |
| 8 | Hochpass | Cutoff nach oben | Resonanz | Drive |
| 9 | Send-Wurf | schiebt die Loops in den Send-FX | – | – |
| 10 | Tremolo | Tempo | Tiefe | Form (Sinus ↔ Rechteck) |
| 11 | Oktave runter | gleitet Richtung −12 Halbtöne (X rechts: −24) | Intervall | Gleitzeit |
| 12 | Dezimierer | Bit- und Sampleraten-Reduktion | Bits | Samplerate |

- **Pads 1 – 12**: halten = an, loslassen = aus. **LATCH**: ein Druck schaltet einen Effekt ein, der nächste aus.
- **XY-Feld**: Maus halten spielt den zuletzt angeklickten Punch-FX, bewegen ändert X / Y.
- **STÄRKE**: wie stark. Am Push 1 bestimmt sie der Pad-Druck (XY-Pad, SHIFT + obere Tasten 5 – 8); an einem
  Pad-Controller mit Anschlagstärke die Anschlagstärke. Pads ohne Druck (K.O. II, siehe [KO2.md](KO2.md)): die
  Stärke beginnt beim Anschlag und steigt bis 100 %, solange du das Pad hältst (*Punch-FX Halte-Rampe*: aus /
  1 Schlag / 2 Schläge / 1 Takt); die Titelzeile zeigt die aktuelle Stärke (*HALTEN 83 %*).
- **Ziel**: wo sie wirken – **Loops** (Standard), **Eingang**, **Master** (Loops + Eingangs-Mithören) oder **Drums**.

**SEND-FX** (zweite FX-Reihe neben dem Metronom; den Typ wählst du in seiner Titelzeile): ein Send-Effekt auf den Loops – **Delay** (X Länge, Y Feedback),
**Hall** (Länge, Farbe), **Verzerrung** (Drive, Farbe), **Chorus** (Modulation, Feedback), **Filter**
(bipolarer DJ-Filter: X links schneidet Höhen, rechts Tiefen; Y Resonanz), **Kompressor** (Drive,
Geschwindigkeit). **SEND** = wie viel der Loops hineingeht. Ein Typwechsel startet ihn sauber.

Alles ist Host-Parameter und MIDI-zuweisbar: *Punch-FX 1 … 12 (halten)*, *Punch-FX-Stärke / X / Y / -Ziel /
halten*, *Send-FX-Typ / X / Y / -Anteil*.

## 40. Controller-Ansicht und Controller (neu in 1.6)

![Controller-Ansicht: Push 1](../images/livelooper-controller-push-de.png)

Ist ein unterstützter Controller verbunden (Ableton Push 1, EP-133 K.O. II – experimentell), bietet *SETUP →
Controller → Oberfläche* die Wahl **Controller**: statt der Standard-Oberfläche zeigt livelooper eine
Zeichnung des Controllers mit dem Live-Zustand des Loopers auf Pads, Tasten und Display. Klick auf ein
gezeichnetes Pad oder eine Taste drückt es. *◀ Standard* führt zurück; beim Abziehen springt livelooper
automatisch zur Standard-Oberfläche. Push 1: siehe [PUSH.md](PUSH.md); K.O. II: siehe [KO2.md](KO2.md).

![SETUP > Controller](../images/livelooper-setup-controllers-de.png)

## 41. Latenz messen und ausgleichen (neu in 1.7)

![SETUP > Audio > Latenz](../images/livelooper-setup-audio-de.png)

Jedes Audio-Interface verzögert den Klang zweimal: auf dem Weg hinaus (du hörst den Loop etwas später) und auf dem Weg zurück (was du spielst, kommt etwas später an). Ohne Ausgleich landet jeder Overdub ein paar Millisekunden hinter dem Beat – und das summiert sich Schicht für Schicht. livelooper misst diese **Roundtrip-Latenz** und verschiebt **jede** Aufnahme darum: erste Aufnahmen, Overdubs, Captures (und Resampling) – nicht mehr nur erste Aufnahmen.

**Messen** – *SETUP → Audio → Latenz → Latenz messen …* (oder der Host-Parameter *Latenz messen*, `lat_measure`, z. B. auf einem Fußschalter):

1. Verbinde einen Ausgang deines Interfaces per **Kabel** mit einem Eingang (am besten) oder halte ein **Mikrofon** dicht an den Lautsprecher. Dreh den Eingang auf.
2. Klick *Latenz messen …*. livelooper schaltet alles stumm, spielt sieben kurze Klicks auf dem Hauptausgang und hört auf den Eingang. Das dauert etwa 4 Sekunden.
3. Das Ergebnis erscheint in ms und Samples (*Gemessen: 11,6 ms (557 Samples)*) und wird **für dieses Audiogerät gespeichert** (Gerätetyp, Eingang, Ausgang, Samplerate und Puffergröße) – wechselst du Gerät oder Puffer, nimmt livelooper die Messung dieses Geräts.

Kommt nichts zurück, sagt livelooper das (*kein Ping erkannt – einen Ausgang per Kabel mit einem Eingang verbinden oder ein Mikrofon an die Lautsprecher halten*). Weichen die Durchläufe um mehr als 1 ms ab (lauter Raum, Mikrofon zu weit weg, andere Audio-Apps), wird nichts gespeichert (*unsicher*). Der Wert ist der Median der Durchläufe – ein gestörter Klick ändert nichts.

**Ausgleich** – *Latenzausgleich* (Host-Parameter `lat_comp`, standardmäßig an) nimmt in dieser Reihenfolge: die Messung des aktuellen Geräts; ohne Messung, was das Audiogerät meldet (Eingangs- + Ausgangslatenz + zwei Puffer – meist ein guter erster Wert, selten exakt); im Modus *K.O. II aufnehmen* den Latenz-Ausgleich des K.O. II (siehe [KO2.md](KO2.md)). Dazu kommt der **manuelle Versatz** (*SETUP → Audio → Latenz → Manueller Versatz* oder der Host-Parameter *Latenz-Versatz*, `lat_offset`, −50 … +50 ms, in der DAW als Regler und per MIDI zuweisbar) – positive Werte schieben Aufnahmen nach vorn. *Aktiv* zeigt, was gerade gilt.

So funktioniert es: Eine fertige erste Aufnahme oder ein Capture spielt den Loop um diesen Wert früher gegen den Beat; ein Overdub schreibt das Gespielte um so viele Samples früher in den Loop – genau dorthin, wo du den Loop beim Spielen gehört hast.

**In einer DAW (Plug-in):** Plug-in-Formate geben einem Plug-in keine Information über die Latenz des Audiogeräts, livelooper kann sie also nicht vom Host lesen. Miss stattdessen **in der DAW**: ein Kabel von einem Interface-Ausgang zum Eingang der Looper-Spur, Eingangsmonitor an, sonst nichts abspielen – die Messung enthält dann auch die Puffer der DAW. Gespeichert pro Host. Der Aufnahme-Latenzausgleich deiner DAW gilt nur für Clips, die die DAW selbst aufnimmt, nicht für den Live-Eingang durch ein Plug-in – beides addiert sich also nie.

## 42. Anschlagkurven und MIDI-Feedback (neu in 1.7)

![SETUP > Controller: Anschlagkurven, Push-1-Display, MIDI-Feedback](../images/livelooper-setup-controllers-17-de.png)

**Anschlagkurven** – *SETUP → Controller → Anschlagkurven*, getrennt für **Push 1**, **K.O. II** und **MIDI (gelernte Noten)** (Host-Parameter `vel_push`, `vel_ko2`, `vel_midi`): *Weich* (leichte Schläge kommen lauter an), *Normal*, *Hart* (für vollen Pegel fester schlagen), *Fest* (jeder Schlag mit voller Anschlagstärke). Sie wirken auf alles, was die Anschlagstärke nutzt: Punch-FX-Stärke, Step-Anschlag im STEP-Modus und Parameter, die du auf eine Note gelernt hast (eine Note setzt einen Regler auf ihre Anschlagstärke).

**MIDI-Feedback (experimentell)** – *SETUP → Controller → MIDI-Feedback*: Für jedes per MIDI Learn zugewiesene Bedienelement sendet livelooper den Zustand an einen **MIDI-Ausgang** deiner Wahl zurück – ein Note-on (oder CC) mit demselben Kanal und derselben Nummer. Die meisten Controller lassen ihre Pad-/Tasten-LED je nach Wert leuchten, gedimmt leuchten oder blinken:

| Zustand | gesendeter Wert (Standard) |
|---|---|
| an – Spur spielt, Schalter an, Szene aktiv | *Wert für an* (127; auch 100, 64, 1) |
| aus – leer / gestoppt, Schalter aus | *Wert für aus* (0; auch 1, 7) |
| blinken – Aufnahme, Overdub, wartet auf den Takt, Messung | *Wert für blinken*: livelooper blinkt im Takt (Standard) oder sendet 2 / 3 / 64 |

REC / PLAY auf einem Pad: blinkt bei Aufnahme / Overdub / Warten, leuchtet beim Abspielen. STOP leuchtet bei gestoppter Spur, UNDO / REDO, wenn es etwas rückgängig zu machen / wiederherzustellen gibt, Szenen-Tasten bei der zuletzt abgerufenen Szene. Auf einen CC gelernte Regler und Fader bekommen ihren aktuellen Wert (LED-Kränze, Motorfader). An / aus mit dem Host-Parameter *MIDI-Feedback (experimentell)* (`midi_fb`, standardmäßig aus). Alle paar Sekunden wird alles neu gesendet, damit ein später angeschlossener Controller nachzieht. Unter Windows darf der Ausgang in keinem anderen Programm geöffnet sein.

## 43. Live-Ansicht (neu in 1.7)

![Live-Ansicht](../images/livelooper-live-de.png)

Die **Live-Ansicht** ist die Oberfläche für die Bühne: eine große Karte pro Loop-Spur und sonst nichts, was man aus Versehen trifft. Öffnen mit der **LIVE**-Taste neben dem Logo, dem Host-Parameter *Live-Ansicht* (`live_view`, wie jede Funktion per MIDI zuweisbar), **SHIFT + MASTER** am Push 1 oder *SETUP → Anzeige → Live-Ansicht → Live-Ansicht öffnen (Vollbild)*. **Esc**, **LIVE BEENDEN** oder noch einmal LIVE führen zur vollen Oberfläche zurück.

| Bereich | Inhalt |
|---|---|
| Kopfzeile | Logo mit LIVE-Plakette, **TAP**, **LIVE BEENDEN**, ein großes Display: Tempo (BPM), **Takt . Schlag** und die Schlag-Punkte (der erste Schlag im Takt orange) |
| Spur-Karten (5 oder 8) | Zustand (PLAY, DUB, REC, STOP, LEER, REC WAIT …), Länge in Takten, Loop-Position in %, die **Wellenform** mit Abspielmarke, das große **REC / PLAY / DUB**-Pad mit LED-Ring, **PEGEL** (Balken ziehen; die Pegelanzeige läuft hinter der weißen Pegelmarke), **STOP** (1,5 s halten = löschen) |
| PERFORMANCE-FX | Regler **FILTER** (Performance-Filter), **BEAT-FX** (Beat-FX-Pegel), **PUNCH** (Punch-FX-Stärke), **MASTER**; **ECHO …** (Beat-FX an / aus, zeigt Beat-FX-Typ und Notenwert), **REPEAT** und **STUTTER** (solange gehalten), **FILL** (solange gehalten); der Punch-FX-Balken (der gehaltene Punch-FX mit seiner aktuellen Stärke, sonst der ausgewählte); **ALLE START / STOP**, **CAPTURE** |

Jedes Bedienelement der Live-Ansicht ist dieselbe Funktion wie auf der vollen Oberfläche (Rechtsklick = MIDI Learn). *SETUP → Anzeige → Live-Ansicht* schaltet die **Wellenformen in den Spur-Displays** (standardmäßig an; aus = ein Positionsbalken) und die **Performance-FX-Leiste** (an; aus = höhere Karten). *Live-Ansicht öffnen (Vollbild)* öffnet sie in einem eigenen randlosen Fenster, das den ganzen Bildschirm bedeckt und über anderen Fenstern bleibt (Esc schließt es) – praktisch auf einem zweiten Monitor oder dem Laptop auf der Bühne. Es ist kein gesperrter Kiosk-Modus: die Tastenkürzel des Betriebssystems (App wechseln, Taskleiste / Dock) funktionieren weiter.

Bühnenmodus und helles Farbschema gelten auch für die Live-Ansicht:

![Live-Ansicht im Bühnenmodus](../images/livelooper-live-stage-de.png)

## 44. Anzeige: Farbschema, Größe, Bühnenmodus, Wellenformen (neu in 1.7)

![SETUP > Anzeige](../images/livelooper-setup-display-de.png)

*SETUP → Anzeige*:

- **Farbschema** – *Dunkel* (Standard) oder *Hell*: derselbe Glas-Look auf hellen Flächen mit dunkler Schrift. Displays und VU-Meter bleiben dunkel wie echte LCDs. Host-Parameter *Farbschema* (`ui_theme`).

![Helles Farbschema](../images/livelooper-light-de.png)

- **Oberflächengröße** – 75 %, 100 %, 125 %, 150 % oder 200 %, auch mit **Strg + / Strg −** (Mac: **Cmd + / Cmd −**, Strg / Cmd 0 = 100 %). Am Fenstereck ziehen skaliert weiterhin frei.
- **Bühnenmodus** – hoher Kontrast und größere Schrift für dunkle Bühnen und einen weiter entfernten Bildschirm: fast schwarze Flächen mit klaren Rändern, weiße Schrift, größere Beschriftungen (was nicht passt, behält seine normale Größe). Host-Parameter *Bühnenmodus* (`stage_mode`).

![Bühnenmodus](../images/livelooper-stage-de.png)

- **Wellenformen in den Loop-Spuren zeigen** – standardmäßig aus (die Spur-Displays zeigen den Zustand in großer Schrift mit einer kleinen Wellenform). An: eine kompakte Kopfzeile (Zustand, Takte, Pegel) und eine große Wellenform in der Spurfarbe – der gespielte Teil heller, die letzte **Overdub-Schicht** als hellere Tönung im Loop (bis du sie rückgängig machst; ältere Overdubs gehören zum Loop), eine weiße Abspielmarke (gestrichelt bei einer gestoppten Spur: wo der Loop jetzt wäre, beim Aufnehmen am rechten Rand), *KEIN LOOP* bei einer leeren Spur. Host-Parameter *Wellenformen zeigen* (`waveforms`).

![Wellenformen in den Spur-Displays](../images/livelooper-waveforms-de.png)

Größe, Farbschema, Bühnenmodus und die Wellenform-Einstellung merkt sich livelooper auf diesem Computer (ein neues livelooper startet damit; ein geladenes Projekt oder Set kann eigene Werte für die Parameter mitbringen). Die Wellenformen werden im Oberflächen-Thread aus dem Loop-Speicher gelesen – die Audioverarbeitung macht dafür keine zusätzliche Arbeit und reserviert keinen Speicher.

## 45. Loop-Länge (neu in 1.7)

![Loop-Länge-Pille in den Spuren](../images/livelooper-loop-length-de.png)

Jede Spur hat **eine** Loop-Länge: *wie viele Takte die erste Aufnahme dauert*. Sie sitzt als kleine Pille in der Titelzeile jeder Spur (neben *SPUR n*) und in den Karten der Live-Ansicht, solange die Spur leer ist oder aufnimmt.

- **AUTO** (Standard): du beendest die erste Aufnahme selbst mit dem nächsten Druck (gerundet durch die Quantisierung).
- **1 · 2 · 4 · 8 · 16 TAKTE** (orange): die Aufnahme endet nach so vielen Takten von selbst – REC drücken und spielen, livelooper schließt den Loop auf dem Taktstrich. Während der Aufnahme füllt sich die Pille mit dem Fortschritt.
- **Ändern**: Pille anklicken und wählen oder darüber das Mausrad drehen / nach oben oder unten ziehen. Rechtsklick = MIDI-Learn (wie bei jedem Bedienelement). Es ist dieselbe Einstellung wie *Loop-Länge* im Spur-SETUP und der Host-Parameter **Loop-Länge** (Loop Length, `t1_measure` … `t8_measure`, IDs unverändert; vor 1.7 *Takte*) – DAW-Automation, MIDI-Zuweisung und Controller ändern sie also alle.
- **Solange die Spur einen Loop enthält**, ist die Pille abgedunkelt: die Länge zählt erst für die nächste neue Aufnahme (nach LÖSCHEN). Vorab einstellen geht trotzdem.
- **Nur dieses eine Mal**: *Loop-Länge einmalig* (Host-Parameter `t1_reclen` …, vor 1.7 *Aufnahmelänge*; am Push: REC-Pad halten + Positions-Pad) setzt eine Länge (1 – 32 Takte) nur für die nächste oder laufende Aufnahme. Die Pille zeigt diese Länge dann mit gestricheltem orangefarbenem Rand; die Einstellung selbst bleibt, wie sie war.

**Push 1:** **FIXED LENGTH** halten und ein Pad in der Spalte einer Spur drücken (Reihen 3 – 8 von unten = AUTO, 1, 2, 4, 8, 16 Takte) oder den Encoder dieser Spur drehen. Solange die Taste gehalten wird, zeigt das Display die acht Längen; die FIXED-LENGTH-LED leuchtet, wenn die ausgewählte Spur eine andere Länge als AUTO hat. FIXED LENGTH allein gedrückt und losgelassen stellt weiterhin die Capture-Länge um. Details: [Push](PUSH.md#loop-länge-neu-in-17).

![Push 1: FIXED LENGTH gehalten](../images/livelooper-controller-push-length-de.png)

## Drums, BEATS und Sound-Packs (1.6)

### Kits

| Kit | Klang | Fill-Stil |
|---|---|---|
| **Studio** | modernes akustisches Set, jetzt mit mehr Kick- und Snare-Takes | Toms |
| **808** | tiefe Boom-Kick, knackige Hi-Hats | Toms |
| **909 Style** | druckvoll, House / Techno | Toms |
| **Lo-Fi** | warm, staubig, weiche Höhen | Toms |
| **House** | runde elektronische Kick, echte Handclaps, knackige Hi-Hats | Clap-Wirbel |
| **Techno** | treibende, helle Kick, metallische Hi-Hats, elektronischer Rim | Kick-Wirbel |
| **Boom Bap** | akustisches Set über ein Lo-Fi-Mikrofon, 12-Bit-Sampler-Crunch, Fingerschnipsen | Snare-Wirbel |
| **Trap** | lange Sub-Kick, knackige Snare, kurze Hi-Hats für Rolls, geschichteter Clap | Hi-Hat-Rolls |
| **Breaks** | akustisches Set mit Raummikrofonen für Jungle / Drum & Bass / Big Beat | Breaks (Snare-Chops) |
| **Rock** | großes akustisches Set: offene 24"-Kick, Rimshot-Snare, Cross Stick, Standtom | Toms |
| **Disco** | trockenes, gedämpftes 70er-Set: abgedämpfte Snare, enge Hi-Hats, Tamburin | Snare + offene Hi-Hat |
| **Afro Latin** | echte Handpercussion: Cajón-Kick, Bongo, Claves, Shaker, Tamburin, Kuhglocke, Cabasa, Conga | Conga-Läufe |

Alle Stil-Kits sind echte Aufnahmen unter CC0 (akustische Multisample-Kits des sfzinstruments-Projekts, Samples von freesound.org, ein gesampelter Drumcomputer); nichts ist synthetisiert. Jede Datei und ihre Quelle stehen in `resources/drums/SOURCES.md`. Die Instrument-Namen (z. B. *Conga* statt *Tom* bei Afro Latin) stehen auf der BEATS-Seite und auf Push. Die Kit-Auswahl enthält außerdem **Pack-Kit 1 … 8** für Sound-Packs.

Neue Grooves (Pattern-Auswahl): Deep House, Tech House, Peak Techno, Hypnotic Techno, Boom Bap, Boom Bap Swing, Trap Rolls, Trap Half Time, Jungle, Two Step, Big Beat, Rock Drive, Rock Half Time, Disco Hustle, Funk Groove, Son Clave, Afro Groove, Samba. Jeder bringt seinen eigenen Fill mit; die älteren Grooves nutzen den Fill des Kits.

### Lebendigere Drums: VARIATION

**VARIATION** (BEATS-Seite, Parameter *Drum-Variation*, Standard 50 %) beendet das „Maschinengewehr“: Bei jedem Schlag wählt livelooper einen anderen Take (nie zweimal hintereinander dasselbe Sample), manchmal den benachbarten Velocity-Layer, mischt zwei Takes, variiert Tonhöhe (härtere Schläge etwas höher) und Ausklang leicht und formt die Kick-Akzente (stärkste Eins im ersten Takt einer 4-Takt-Phrase, leisere Auftakte). 0 % = jeder Schlag im selben Layer wie vor 1.6. Am meisten profitieren Kick, Snare und Hi-Hats.

### BEATS – der Step-Sequencer

Klick auf **BEATS** im Rhythmus-Feld. Die Seite liegt über dem Looper, bis du **LOOPER** klickst.

- **Zeilen** = die acht Instrumente des aktuellen Kits (Kick, Snare, Clap, Hi-Hat zu, Hi-Hat offen, Rim, Shaker, Tom).
- **Schritte**: 16 (16tel) oder 32 (32stel) pro Takt, **1 – 4 Takte**; die Tasten **1 – 4** zeigen einen Takt.
- **Maus**: Klick = Schritt an / aus (die Zeile entlang ziehen zum Malen), **SHIFT** + Klick = Akzent, **ALT** + Klick = Ghost Note, **Rechtsklick** = nächste Dynamik (Ghost → normal → Akzent), **Mausrad** über einem Schritt = feine Dynamik. Höhe und Helligkeit des Balkens zeigen die Dynamik, eine weiße Linie markiert Akzente.
- **Eigene Samples pro Zeile**: Zieh eine Audiodatei (WAV, AIFF, FLAC, MP3, Ogg – was dein System liest) auf eine Zeile oder klick auf den Zeilennamen → **Sample laden …**. Die Zeile spielt dann dein Sample statt des Kit-Instruments – in jedem Kit – und zeigt seinen Namen unter dem Instrument. **Zurück zum Kit-Sample** im selben Menü stellt es zurück. Pegel, Tonhöhe, stumm und solo der Zeile wirken weiter; das Sample wird als One-Shot gespielt (höchstens 10 s, mono, normalisiert). Eigene Samples werden im Projekt / Plug-in-Zustand und in Sets gespeichert (eine `.livelooper`-Datei enthält sie als FLAC in `audio/`, zusammen mit dem Originalpfad); ein gespeicherter Beat merkt sich, welche Zeile welche Datei spielte, und lädt sie wieder, wenn die Dateien noch da sind.
- **Pro Zeile**: **M** stumm, **S** solo, **PEGEL** (0 – 150 %), **TONHÖHE** ±12 Halbtöne. Sie wirken auf jedes Pattern (auch die eingebauten Grooves) und sind Host-Parameter (*Beat Zeile 1 … 8 stumm / solo / Pegel / Tonhöhe*), also automatisierbar und per MIDI steuerbar.
- **Platz**: 16 User-Patterns pro Set, in der Pattern-Auswahl als **User 1 … 16** (mit Namen). Den Namen tippst du neben dem Platz ein.
- **PLAY** spielt den Platz. Auch Bearbeiten lässt ihn spielen. Öffnest du BEATS, während ein eingebauter Groove läuft, startet ein leerer Platz als Kopie dieses Grooves.
- **SPEICHERN**: fragt nach einem Namen und speichert den Beat in deiner **Beat-Bibliothek** (`Musik/livelooper/Patterns/<Name>.llpattern`) – in jedem Set und Projekt verfügbar. Ein Beat mit gleichem Namen wird nur nach Rückfrage ersetzt. **Als Datei speichern …** im selben Dialog schreibt die `.llpattern`-Datei an einen beliebigen Ort (zum Weitergeben).
- **LADEN**: Zuerst kommt die Liste deiner **gespeicherten Beats** (Bibliothek), dann **Datei öffnen …** (eine `.llpattern`-Datei von irgendwo), die eingebauten Grooves, Sound-Pack-Patterns und die anderen Plätze dieses Sets. Ein Beat landet im **aktuellen Platz, wenn er leer ist, sonst im nächsten freien Platz** – nichts, was du gebaut hast, wird überschrieben (mit *Diesen Platz ersetzen* im LADEN-Menü lädst du stattdessen in den aktuellen Platz). Der Platz spielt danach.
- Ein gespeicherter Beat behält **Name, Schritte, Dynamik, Schritte pro Takt, Länge (Takte), Swing und den Zeilen-Mixer** (Pegel, Tonhöhe, stumm, solo jeder Zeile); beim Laden werden Swing und Zeilen-Mixer wieder gesetzt. Ältere Pattern-Dateien und Sound-Pack-Patterns haben keinen Swing / Mixer – dort bleibt beides, wie es ist.
- **Nächster / Vorheriger Bibliotheks-Beat** (Host-Parameter, MIDI Learn): blättert auf der Bühne durch deine Bibliothek – die Beats laden in einen Platz (einen freien, wenn der aktuelle Platz deine eigene Bearbeitung enthält) und setzen auf den nächsten Schlag / Takt ein (PATTERN-WECHSEL).
- **LÖSCHEN**, **KOPIEREN** / **EINFÜGEN** (auch zwischen Plätzen), **VARIIEREN**: eine leichte Variation (ein paar Ghost Notes, kleine Dynamikänderungen, evtl. ein Kick-Auftakt) – der Groove bleibt.
- **PATTERN-WECHSEL** (*Sofort*, *Nächster Schlag*, *Nächster Takt* – Standard): wann ein neues Pattern (eingebaut oder User, aus der Auswahl, BEATS, Push oder Automation) einsetzt. Quantisiertes Umschalten hält den Groove im Takt.
- Unter dem Raster: RHYTHMUS an / aus, GROOVE (Mikro-Timing), FILLS (automatisch), SWING, FEEL, VARIATION, PEGEL und die beiden DRUMS-Schalter.

Die User-Patterns werden im Projekt und in Looper-Sets gespeichert. Die Pattern-Auswahl ist ein Host-Parameter, Pattern-Wechsel lassen sich also automatisieren und per MIDI steuern.

### FILL auf Knopfdruck

**FILL** halten (Rhythmus-Feld, BEATS-Seite, Push-Taste **IN** oder Host-Parameter *Fill (halten)* – MIDI Note an / aus): Der Drummer spielt einen Fill im Stil des Grooves / Kits, solange du hältst. Lässt du los, setzt der Groove **auf der nächsten Eins** (Taktanfang) wieder ein, mit akzentuierter Kick und offener Hi-Hat. Kurz antippen spielt einen Fill bis zum Taktende.

### Drums durch Beat FX und Beat Repeat / Stutter

Bis 1.5 blieben die Drums immer trocken. Jetzt gibt es einen Schalter **DRUMS** im Feld BEAT-FX und einen im Feld BEAT REPEAT / STUTTER (auch auf der BEATS-Seite und auf Push **OUT** / **SHIFT + OUT**; Host-Parameter *Drums durch Beat FX*, *Drums durch Repeat/Stutter*):

- **aus** (Standard) – wie bisher: Die Drums bleiben trocken, der Effekt wirkt nur auf den gewählten Kanal.
- **an** – die Drums laufen zusätzlich durch diesen Effekt, egal welcher Kanal gewählt ist (mit demselben Typ, Beat, Tiefe, Timing, Latch …). Jede Gruppe hat ihren eigenen Schalter, so können z. B. die Drums stuttern, während die Beat FX nur die Loops treffen.

Das Metronom bleibt immer trocken.

### Sound-Packs

Ein Sound-Pack (Ordner oder `.zip` mit einer `livelooper-pack.json`, WAV- / AIFF- / FLAC-Samples und optionalen Patterns) bringt Kits und Patterns mit. **BEATS → PACK IMPORT** installiert eines, **PACKS** listet die installierten Packs, liest den Ordner neu ein und öffnet die Pack- / User-Pattern-Ordner. Jedes Pack-Kit belegt einen der Plätze **Pack-Kit 1 … 8** in der Kit-Auswahl. Wie man Packs baut: [SOUND-PACKS.md](SOUND-PACKS.md).

### Push

**SCALES** öffnet auf Push den STEP-Modus (Step-Sequencer auf den Pads), **IN** = FILL solange gehalten, **OUT** = Drums durch die Beat FX (SHIFT: durch Repeat / Stutter) – siehe [PUSH.md](PUSH.md#step-modus-und-fill-16).

