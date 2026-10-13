# livelooper

[English](README.md) | **Deutsch**

**livelooper** ist eine Loop-Station mit fünf oder acht Spuren als Plug-in für Ableton Live (VST3 und Audio Unit, zusätzlich als Standalone-App). Aufnehmen, Overdubben, Effekte, Rhythmus und alles frei per MIDI steuerbar – gebaut für das Live-Looping auf der Bühne und im Studio.

![livelooper](docs/images/livelooper-de.png)

## Funktionen

- **5 Loop-Spuren** mit REC → DUB → PLAY, STOP (halten = löschen), Undo/Redo des letzten Overdubs, Level, Pan, Reverse, One-Shot, Loop-Sync
- **Quantisierung** auf Takt oder Schlag, synchron zu Tempo, Taktart und Transport von Live (oder interne Clock mit Tap-Tempo)
- **Feste Loop-Längen** (1, 2, 4, 8, 16 Takte) oder freie Länge; „zu spät gedrückt" wird bis 15 % nach dem Taktstrich rückwirkend korrigiert
- **Input-FX und Track-FX**: je vier Slots (A–D) mit 12 Effekten – LPF, HPF, BPF, Phaser, Flanger, Delay (tempo-synchron), Reverb, Slicer, Tremolo, Auto Pan, Lo-Fi, Overdrive
- **Rhythmus**: eingebauter Drumcomputer mit Metronom und 11 Pattern, immer tempo-synchron
- **Start-/Stop-Modi**: sofort, Fade-In/Fade-Out, Stopp am Loop-Ende; Dub-Modus Overdub oder Replace
- **Volle MIDI-Steuerung**: alle 247 Funktionen sind Host-Parameter (Live-MIDI-Map) und haben zusätzlich ein eigenes MIDI-Learn per Rechtsklick
- **Follow Transport**: Loops starten und stoppen mit Live
- **Projekt-Speicherung**: die Loops werden verlustfrei (FLAC, 24 Bit) im Live-Set gespeichert
- **Import/Export**: Loops als WAV exportieren, Audiodateien per Drag & Drop auf eine Spur laden
- **Steuerung per Computertastatur** in der Standalone-App: logische Standardbelegung (1–5 REC/PLAY, Q–T STOP, A–G ZURÜCK, Leertaste, Eingabe = TAP) und eigene Belegung unter *SETUP → Tastaturbelegung …*
- **Tempo eintippen**: auf die BPM im Display klicken (oder B drücken) und eingeben
- **Looper-Sets**: den kompletten Looper (Loops, Einstellungen, Effekte) als `.llset`-Datei speichern und laden
- **Pegel und Wellenform**: Stereo-Pegel pro Spur (Eingang bei Aufnahme, Ausgang bei Wiedergabe), Master-Pegel im Display, Wellenform mit Abspielposition, Clip-LED pro Spur
- **Cue-Ausgang** in der Standalone-App: Rhythmus/Metronom auf ein eigenes Ausgangspaar (z. B. Kopfhörer) statt oder zusätzlich zum Hauptausgang
- **Zwei Metronomklänge** (Piep, Holzblock) und moderne synthetisierte Drum-Sounds
- **App-Icon** für die Standalone-App (macOS-Dock, Windows)
- **Oberfläche auf Deutsch und Englisch**: umschaltbar unter *SETUP → Language / Sprache* (Automatisch folgt der Systemsprache)

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
