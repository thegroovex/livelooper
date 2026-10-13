# livelooper

[English](README.md) | **Deutsch**

**livelooper ist ein Instrument, kein Rekorder.** Eine Loop-Station mit fünf oder acht Spuren für Ableton Live (VST3 und Audio Unit, zusätzlich als Standalone-App), gebaut zum *Spielen*: eine Idee erforschen, schichten, verbiegen und live aufführen – mit den Händen auf einem **Ableton Push**, einem MIDI-Controller oder der Computertastatur und dem Blick beim Publikum statt auf dem Bildschirm.

![livelooper](docs/images/livelooper-de.png)

## Die Idee

- **Live-Performance zuerst.** Jede Funktion reagiert sofort, landet auf dem Schlag und hält die Musik nie an. Beim Spielen braucht nichts ein Menü.
- **Erforschen und spielen.** Halte fest, was du gerade improvisiert hast, leg mit dem XY-Pad einen Effekt darauf, verstimm es, dreh es um, lass es ausklingen wie ein Band – und behalte, was funktioniert.
- **Hände an die Hardware.** Schließ einen Ableton Push an und livelooper übernimmt ihn – ohne Mapping. Jeder andere Controller funktioniert per MIDI Learn, alle 372 Funktionen sind zuweisbar.
- **Deine Loops gehören in deinen Song.** Zieh einen Loop direkt aus livelooper auf eine Spur in Live, exportiere Stems oder behalte alles im Live-Set.

## Spielen mit Ableton Push

![livelooper auf Ableton Push](docs/images/livelooper-live-push.jpg)

Push (ab Version 1) anschließen, **User** drücken – und livelooper macht daraus eine eigene Looper-Oberfläche, im Plug-in und in der Standalone-App:

- **Eine Pad-Spalte pro Spur**: REC / PLAY / DUB und STOP, die Loop-Position läuft als Licht die Spalte hoch, Farben zeigen den Status, Blinken heißt „wartet auf den Takt“
- **Encoder** für Pegel, Panorama, Tonhöhe, Feedback oder Effekte, eine **Master-Sektion** (Gesamtpegel, Tempo) und Swing auf dem Swing-Encoder
- **Szenen** auf den Szenen-Tasten, **Capture**, Undo, Metronom, Tap-Tempo, **DELETE + Pad** löscht eine Spur
- **Loop-Cutting**: Positions-Pad antippen und der Loop springt zur nächsten 16tel dorthin, halten lässt ihn stottern, loslassen – und er ist wieder synchron
- **Performance-Moves**: MUTE / SOLO + Pad halten für Drops, DOUBLE = Loop × 2 (SHIFT = halbieren), OCTAVE ±12, DUPLICATE + Pads kopiert einen Loop, REC-Pad + Positions-Pad = 1–32 Takte
- **Touchstrip** = Tape-Stopp, der synchron zurückspringt (SHIFT = Filter-Sweep), **Fußschalter** = REC/PLAY, doppelt STOP, halten LÖSCHEN
- **FX-XY-Pad**: Das 8×8-Raster wird zum Performance-Pad – anfassen, schieben, fester drücken für mehr, loslassen, und es springt zurück
- **Display** mit Namen, Werten, Spurstatus, Tempo und Pegelanzeige

Einrichtung und komplette Belegung: **[PUSH.md](docs/de/PUSH.md)**.

## Funktionen

**Performance**
- **5 oder 8 Loop-Spuren**: REC → DUB → PLAY auf einem Pad, STOP (halten = löschen), Undo/Redo, Reverse, One-Shot, Pegel, Panorama
- **Szenen**: 8 Szenen-Knöpfe starten und stoppen Spurgruppen im Takt
- **Capture**: Die zuletzt gespielten 1–8 Takte werden nachträglich zum Loop – ganz ohne Aufnahme-Taste
- **Auto-Aufnahme**: Scharf geschaltete Spuren starten mit dem ersten Ton über einer Schwelle – die Hände bleiben am Instrument
- **Quantisierung** auf Takt oder Schlag; „zu spät“ gedrückt wird bis 15 % nach dem Taktstrich korrigiert; feste Längen 1–16 Takte oder frei
- **Start-/Stopp-Modi**: sofort, Fade-In/-Out, Stopp am Loop-Ende; Overdub oder Replace
- **Geister-Abspielmarke**: Gestoppte Spuren zeigen, wo sie gerade wären – so holst du sie punktgenau zurück
- **Loop-Cutting und Stottern**, **Stumm / Solo solange gehalten**, **Loop × 2 / halbieren**, **in eine andere Spur kopieren**, **Tape-Stopp** und ein **Performance-Filter** – alles musikalisch quantisiert, nichts hält je die Musik an
- **Ableton Push**, **MIDI Learn** für jeden Controller (Rechtsklick auf ein Bedienelement) und **Computertastatur** in der Standalone-App

**Klang formen**
- **Input-FX und Track-FX**: je vier Slots mit 12 Effekten – Tiefpass, Hochpass, Bandpass, Phaser, Flanger, Delay (tempo-synchron), Hall, Slicer, Tremolo, Auto-Pan, Lo-Fi, Overdrive
- **Pro Spur**: Feedback (Ausklingen wie ein Band), Sidechain-Ducking, Tonhöhe ±12 Halbtöne, Tempo folgen (Time-Stretch, Signalsmith Stretch), Resample der anderen Spuren
- **Rhythmus**: 4 Kits (Studio, 808, 909, Lo-Fi), 21 Grooves von Rock bis Trap, Dembow, UK Garage und Afro House, Swing und Feel; Metronom mit 4 Klängen

**Arbeiten mit Live**
- **Synchron zu Live**: Tempo, Taktart und Transport – oder interne Clock mit Tap-Tempo / eingetipptem Tempo
- **Loops nach Live ziehen**: Zieh die Wellenform einer Spur auf eine Audiospur oder in den Browser – eine WAV mit Tempo-Infos landet im Raster von Live
- **Stem-Export** mit Tempo-Metadaten, WAV-Export pro Spur, Audiodateien per Drag & Drop auf eine Spur importieren
- **Direkte Spurausgänge** (Plug-in Multi-Out, Interface-Ausgänge in der Standalone-App), **Cue-Ausgang** für das Metronom
- **Mit dem Projekt gespeichert**: Loops verlustfrei (FLAC, 24 Bit) im Live-Set; komplette **Looper-Sets** als `.llset`-Datei
- **Pegel und Wellenformen**: Stereo-Pegel pro Spur, Master-Pegel, Wellenform mit Abspielposition, Clip-LED
- **Alle 372 Funktionen** sind Host-Parameter für Lives MIDI-Map
- **Deutsche und englische** Oberfläche (*SETUP → Language / Sprache*)

## Warum VST3/AU?

livelooper ist ein JUCE-Plug-in (C++). Gegenüber einem Max-for-Live-Gerät hat das drei Vorteile: es läuft in jeder Live-Edition (nicht nur in Suite), die Audio-Engine läuft sample-genau und echtzeitsicher in nativem Code, und es ist automatisiert testbar. Dasselbe Plug-in funktioniert auch in anderen DAWs und als eigenständige App.

## Schnellstart

1. Plug-in installieren – siehe [Installation](docs/de/INSTALLATION.md)
2. In Live eine Audiospur mit deinem Mikro/Instrument als Eingang anlegen, Monitor auf **In** stellen und **livelooper** aus *Plug-ins → Audio Units / VST3* auf die Spur ziehen
3. Das große REC/PLAY-Pad von Spur 1 klicken: Aufnahme startet auf dem nächsten Takt, nochmal klicken: Loop läuft

Alles Weitere steht in der **[Bedienungsanleitung](docs/de/BEDIENUNGSANLEITUNG.md)**, die MIDI-Steuerung in **[MIDI](docs/de/MIDI.md)**.

## Dokumentation

| Dokument | Inhalt |
|---|---|
| [INSTALLATION.md](docs/de/INSTALLATION.md) | Download, Installation auf macOS und Windows, Einrichtung in Live |
| [BEDIENUNGSANLEITUNG.md](docs/de/BEDIENUNGSANLEITUNG.md) | Oberfläche, alle Funktionen, Effekte, Rhythmus, Setup, Sprache |
| [MIDI.md](docs/de/MIDI.md) | MIDI-Learn, Live-MIDI-Map, Standard-Belegung, Parameterliste |
| [PUSH.md](docs/de/PUSH.md) | Steuerung mit Ableton Push: Einrichtung, Pad-Belegung, Encoder, FX-XY-Pad |
| [ENTWICKLUNG.md](docs/de/ENTWICKLUNG.md) | Bauen aus dem Quellcode, Architektur, Tests, CI, Übersetzungen |

Englische Dokumentation: [docs/en](docs/en/).

## Selbst bauen

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

JUCE 8 wird beim Konfigurieren automatisch geladen. Details in [ENTWICKLUNG.md](docs/de/ENTWICKLUNG.md).

## Lizenz

Für den livelooper-Quellcode ist noch keine Lizenz festgelegt. livelooper basiert auf dem JUCE-Framework, das unter AGPLv3 oder einer kommerziellen JUCE-Lizenz steht – bei Weitergabe von Binärdateien gelten deren Bedingungen.

Fremdcode in `third_party/`: [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) und Signalsmith Linear (Time-Stretch / Tonhöhe), beide unter MIT-Lizenz – siehe deren `LICENSE.txt`.
