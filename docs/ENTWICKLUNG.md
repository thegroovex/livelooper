# Entwicklung

## Voraussetzungen

| Plattform | Werkzeuge |
|---|---|
| macOS | Xcode 14+ (Command Line Tools), CMake ≥ 3.22 |
| Windows | Visual Studio 2022 (C++-Desktop), CMake ≥ 3.22 |
| Linux (Entwicklung/Tests) | GCC 11+ oder Clang 14+, CMake, Ninja, `libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev`, für Tests ohne Bildschirm `xvfb` |

JUCE 8.0.15 wird von CMake automatisch geladen. Eine lokale JUCE-Kopie geht mit `-DLIVELOOPER_JUCE_PATH=/pfad/zu/JUCE`.

## Bauen

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Ergebnisse in `build/livelooper_artefacts/Release/`:

- `VST3/livelooper.vst3` (macOS, Windows, Linux)
- `AU/livelooper.component` (nur macOS)
- `Standalone/livelooper(.app/.exe)`

Mit `-DLIVELOOPER_COPY_PLUGIN=ON` kopiert der Build die Plug-ins direkt in die Systemordner. Universal Binary auf dem Mac: `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

## Tests

```bash
ctest --test-dir build -C Release --output-on-failure      # Linux ohne Bildschirm: xvfb-run -a ctest ...
```

| Test | Was geprüft wird |
|---|---|
| `unit_tests` (`livelooper_tests`) | Engine: freie und quantisierte Aufnahme (Takt, Schlag), rückwirkendes Beenden, feste Taktanzahl, Abbrechen, große Host-Blöcke, Eingangs-Routing, Overdub, Replace, Undo/Redo (inkl. erster Aufnahme), Clear, Reverse, One-Shot, Stop-Modi, Pegel/Pan, All Start/Stop, Track-FX-Routing, Loop-Sync nach Transportsprung, quantisierter Start in Phase, Follow Transport, interne Clock, Rhythmus. Effekte: alle 12 Typen stabil, Bypass bit-genau, Filterwirkung, Delay-Zeit, Slicer-Raster. MIDI: Learn, Taster/Schalter/Auswahl/Regler, CC-Flanken, Eindeutigkeit, Speichern. Processor: 136 eindeutige, MIDI-fähige Parameter, Taster-Parameter, MIDI über processBlock, Learn, Projekt-Speichern inkl. Loop-Audio, Samplerate-Wechsel, WAV-Export/-Import, Editor |
| `vst3_host_test` (`livelooper_hosttest`) | lädt das fertige `livelooper.vst3` über JUCEs VST3-Hosting wie eine DAW: Name, Effekt-Kategorie, MIDI-Eingang, Parameteranzahl, Aufnahme und Wiedergabe über Parameter, Stop, Zustand speichern/laden in zweiter Instanz, Editor |

Einzelne Gruppe: `livelooper_tests engine` bzw. `midi`, `plugin`.

In der CI läuft zusätzlich [pluginval](https://github.com/Tracktion/pluginval) mit Strenge 5 auf Linux, macOS und Windows.

Screenshots für die Doku: `livelooper_screenshot docs/images` (unter Linux mit `xvfb-run -a`).

## Architektur

```
src/
  engine/                 UI-freier DSP-Kern (nur juce_audio_basics)
    Types.h               Einstellungen, Enums, TimeInfo
    LoopTrack.*           eine Spur: Aufnahme, Overdub, Wiedergabe, Undo/Redo, Fades
    LooperEngine.*        5 Spuren, Zeitachse/Quantisierung, Befehle, FX-Busse, Rhythmus, Status
    Effects.*             FxUnit (12 Effekte) und FxChain (4 Slots)
    RhythmGenerator.*     synthetischer Drumcomputer, 12 Pattern
    MidiMap.*             MIDI-Learn-Tabelle (lock-frei)
  plugin/
    Parameters.*          alle 136 Parameter (IDs, Bereiche, Gruppen)
    PluginProcessor.*     AudioProcessor: Parameter → Engine, Taster, MIDI, Zustand, Import/Export
    PluginEditor.*        Oberfläche, MIDI-Learn-Modus, Menüs, Drag & Drop
    Widgets.*             REC/PLAY-Pad, STOP-Pad, Display, FX-Slot, Spur, Spur-Setup
    LooperLookAndFeel.*   Hardware-Optik
tests/                    Unit-Tests und VST3-Host-Test
tools/Screenshot.cpp      rendert die Oberfläche als PNG
```

### Echtzeit-Regeln

- Der Audio-Thread alloziert nicht und blockiert nicht. Speicher wird in `prepareToPlay` reserviert.
- **Undo ohne Kopieren**: jede Spur hat zwei Ebenen. Der Loop ist in Blöcke zu 512 Samples geteilt, für jeden Block ist vermerkt, welche Ebene gilt. Berührt ein Overdub einen Block zum ersten Mal, wird nur dieser Block in die andere Ebene kopiert. Undo/Redo schaltet die Zuordnung der berührten Blöcke um – O(Blöcke), keine großen Kopien.
- Befehle (Taster, MIDI) werden in eine Warteschlange gelegt und am Blockanfang ausgeführt; quantisierte Aktionen werden sample-genau innerhalb des Blocks ausgelöst.
- Import und Laden schreiben unter einer Spur-Sperre; der Audio-Thread nimmt sie nur mit `tryEnter` (ist sie belegt, bleibt die Spur für einen Block stumm).
- Speichern und Export lesen ohne Sperre, damit die Wiedergabe beim Speichern nicht aussetzt.

### Zeitachse

Die Position in Viertelnoten (ppq) kommt vom Host, solange dessen Transport läuft. Sonst führt die Engine eine interne ppq-Zählung, die beim Start der ersten Aufnahme bei 0 beginnt bzw. nahtlos an der letzten Host-Position weiterläuft. Jede Spur merkt sich den ppq-Anker ihrer Aufnahme; mit Loop-Sync wird die Wiedergabeposition nach Transportsprüngen daraus neu berechnet.

### Taster-Parameter

Momentfunktionen sind Bool-Parameter. Der Processor reagiert auf die steigende Flanke und setzt den Parameter über einen Timer auf dem Message-Thread wieder auf 0. So ist jede Funktion automatisierbar und über Lives MIDI-Map zuweisbar.

## CI / Releases

`.github/workflows/ci.yml`

- jeder Push / Pull Request: Linux-Build, alle Tests, pluginval
- Tag `v*` oder manueller Start: zusätzlich macOS (Universal, VST3 + AU + App) und Windows (VST3 + App), Tests, pluginval, ZIP-Artefakte
- Tag `v*`: GitHub-Release mit den ZIPs

Release erstellen:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## Bekannte Einschränkungen

- Kein Time-Stretching: Tempoänderungen nach der Aufnahme ändern die Loop-Geschwindigkeit nicht.
- Ein gemeinsamer Stereo-Ausgang (keine Einzelausgänge pro Spur).
- Ein Undo-Schritt pro Spur (der letzte Overdub-Durchgang).
- Wechselt die Samplerate oder die maximale Loop-Länge, werden die Loops umgerechnet; laufende Aufnahmen gehen dabei verloren.
- Die macOS-Builds sind nicht signiert/notarisiert (siehe INSTALLATION.md).
