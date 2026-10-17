# Entwicklung

[English](../en/DEVELOPMENT.md) | **Deutsch**

## Voraussetzungen

| Plattform | Werkzeuge |
|---|---|
| macOS | Xcode 14+ (Command Line Tools), CMake ≥ 3.22 |
| Windows | Visual Studio 2022 (C++-Desktop), CMake ≥ 3.22 |
| Linux (Entwicklung/Tests) | GCC 11+ oder Clang 14+, CMake, Ninja, `libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev`, für Tests ohne Bildschirm `xvfb` |

JUCE 8.0.15 wird von CMake automatisch geladen. Eine lokale JUCE-Kopie geht mit `-DLIVELOOPER_JUCE_PATH=/pfad/zu/JUCE`.

## Bauen

Ableton Link ist ein Git-Submodul – mit Submodulen klonen (oder sie später holen):

```bash
git clone --recursive https://github.com/thegroovex/livelooper.git
# bestehender Klon: git submodule update --init --recursive
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
| `unit_tests` (`livelooper_tests`) | Engine: freie und quantisierte Aufnahme (Takt, Schlag), rückwirkendes Beenden, feste Taktanzahl, Abbrechen, große Host-Blöcke, Eingangs-Routing, Overdub, Replace, Undo/Redo (inkl. erster Aufnahme), Clear, Reverse, One-Shot, Stop-Modi, Pegel/Pan, All Start/Stop, Track-FX-Routing, Loop-Sync nach Transportsprung, quantisierter Start in Phase, Follow Transport, interne Clock, Rhythmus. Effekte: alle 12 Typen stabil, Bypass bit-genau, Filterwirkung, Delay-Zeit, Slicer-Raster. MIDI: Learn, Taster/Schalter/Auswahl/Regler, CC-Flanken, Eindeutigkeit, Speichern. Processor: 137 eindeutige, MIDI-fähige Parameter, Taster-Parameter, MIDI über processBlock, Learn, Projekt-Speichern inkl. Loop-Audio, Samplerate-Wechsel, WAV-Export/-Import, Editor. Features 1.2: Cue-Routing (Haupt / Cue / beide, ohne Cue → Haupt), beide Metronomklänge, alle Drum-Patterns im Pegelbereich, Spur-Pegel Eingang/Ausgang, Master-Pegel, Wellenform inkl. Overdub/Undo, Standalone-Cue-Bus-Kanalpaare, Sets speichern/laden mit Erhalt der Zuweisungen, STOP-Knopf beim Loslassen. Lokalisierung: deutsche/englische Texte, Übersetzung für jede Auswahl und jeden Effekt, Parameternamen, Umschalten im laufenden Betrieb. Features 1.4: Loop-Cutting im 16tel-Raster (übergeblendet, Antippen bleibt, Halten stottert, zurück in Sync), Stumm/Solo, Verdoppeln/Halbieren mit Undo, Kopieren, Aufnahmelänge, Tape-Stopp, Performance-Filter, Slice-Parameter als gehaltene MIDI-Funktion; Push: Cutting-Gesten, REC + Positions-Pad mit Fortschrittsbalken, Fußschalter, Touchstrip, Pad-Druck, DOUBLE/OCTAVE/DUPLICATE, MUTE/SOLO halten, Pegelanzeige mit Display-Zeichen (begrenzte Rate). Features 1.5: Beat-FX (alle 16 Typen stabil, Beat-Sync, Fahnen, Kanal-Routing), Color-FX (Mitte bit-genau), Beat Repeat / Stutter im Raster und zurück in Sync, Metronom-Block, Sample-Kits / Grooves / Fills, Capture 16 / 32 Takte, Master-VU, Hilfe-Parser und Suche; Push 1.5: Beat-FX-Tasten und BROWSE-Encoder, REPEAT / ACCENT gehalten + Latch, Touchstrip-Timing 1/2 … 1/32, FIXED LENGTH Capture-Länge, SHIFT + METRONOME Link, SHIFT + TAP Metronom; Link an / aus (ohne Netzwerk) und Link als Taktgeber, App-Einstellungen in einem temporären Ordner (Startmodus, letzte Einstellungen, zuletzt verwendete Sets, Update-Optionen), `compareVersions` (Vorabversionen, v-Präfix) |
| `vst3_host_test` (`livelooper_hosttest`) | lädt das fertige `livelooper.vst3` über JUCEs VST3-Hosting wie eine DAW: Name, Effekt-Kategorie, MIDI-Eingang, Parameteranzahl, Aufnahme und Wiedergabe über Parameter, Stop, Zustand speichern/laden in zweiter Instanz, Editor |

Einzelne Gruppe: `livelooper_tests engine` bzw. `midi`, `plugin`. `livelooper_tests --list` listet alle Testgruppen, `livelooper_tests --shard 2/3` führt jede dritte Gruppe ab der zweiten aus (die CI-Teile).

In der CI läuft zusätzlich [pluginval](https://github.com/Tracktion/pluginval) mit Strenge 5 auf Linux, macOS und Windows.

Screenshots für die Doku: `livelooper_screenshot docs/images` (unter Linux mit `xvfb-run -a`). Es schreibt jedes Bild auf Englisch (`livelooper*.png`) und Deutsch (`livelooper*-de.png`): Hauptansicht, Live-Performance (Beat-FX, Color-FX, Repeat-Latch, Metronom und Link an), MIDI Learn, Tempo-Eingabe, Tastatur-Belegung, Spur-Setup, 8 Spuren, Hilfe- und Info-Fenster.

## Architektur

```
src/
  engine/                 UI-freier DSP-Kern (nur juce_audio_basics)
    Types.h               Einstellungen, Enums, TimeInfo
    LoopTrack.*           eine Spur: Aufnahme, Overdub, Wiedergabe, Undo/Redo, Fades
    LooperEngine.*        8 Spuren (5 oder 8 sichtbar), Zeitachse/Quantisierung, Befehle, FX-Busse, Rhythmus, Status
    Effects.*             FxUnit (12 Effekte) und FxChain (4 Slots)
    Performance.*         Tape-Stopp und Performance-Filter auf der Loop-Summe (1.4)
    RhythmGenerator.*     Drumcomputer: 21 Pattern, Sample-Kits (1.5), Grooves und Fills
    DrumSamples.inc       Sample-Slots der eingebauten Kits
    BeatFx.*              Beat-FX (16 Typen), Color-FX, Beat Repeat / Stutter (1.5)
    MidiMap.*             MIDI-Learn-Tabelle (lock-frei)
  plugin/
    Localisation.*        deutsche/englische Texte, Spracheinstellung
    Parameters.*          alle 400 Parameter (IDs nur angehängt, Bereiche, Gruppen, englische und lokalisierte Namen)
    LinkSync.*            Ableton Link: Tempo / Schlag / Start-Stopp, latenzkompensiert (1.5)
    AppSettings.*         Einstellungen von Benutzer / Computer: Startmodus, letzte Einstellungen, zuletzt verwendete Sets, Updates (1.5)
    Updates.*, InfoWindows.*  GitHub-Release-Prüfung, Download + Prüfung, Hilfe-Fenster, Info-Fenster (1.5)
    DrumSampleLoader.*    dekodiert die eingebetteten FLAC-Drum-Samples
    push/                 Ableton Push (1) über dessen User-Port: Protokoll, Gerät, Oberfläche
    PluginProcessor.*     AudioProcessor: Parameter → Engine, Taster, MIDI, Zustand, Import/Export
    PluginEditor.*        Oberfläche, MIDI-Learn-Modus, Menüs, Drag & Drop
    Widgets.*             REC/PLAY-Pad, STOP-Pad, Display, FX-Slot, Spur, Spur-Setup
    LooperLookAndFeel.*   Hardware-Optik
  standalone/
    StandaloneApp.cpp     eigenes Standalone-Fenster (Knopf Audio-/MIDI-Einstellungen, Start-Einstellungen)
    MacMicPermission.mm   fragt einmal nach der Mikrofon-Freigabe (macOS)
tests/                    Unit-Tests und VST3-Host-Test
installer/macos/          .pkg: distribution.xml, Willkommen / Readme (EN / DE), postinstall, install.command
installer/windows/        livelooper.iss (Inno Setup: App + VST3, EN / DE, AGPLv3-Seite, Deinstallation)
resources/macos/          livelooper.entitlements (Audio-Eingang)
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

Momentfunktionen sind Bool-Parameter. Der Processor reagiert auf die steigende Flanke und setzt den Parameter über einen Timer auf dem Message-Thread wieder auf 0. So ist jede Funktion automatisierbar und über das MIDI-Mapping des Hosts (z. B. Lives MIDI-Map) zuweisbar.

### Übersetzungen

- Alle Texte im Code sind Englisch. Alles, was der Nutzer sieht, läuft durch `i18n::tr()` (`src/plugin/Localisation.cpp`), das den englischen Text in der deutschen Tabelle nachschlägt. Texte mit Zahlen oder Namen nutzen Platzhalter: `tr ("TRACK %1", "3")` → „SPUR 3".
- Knopftexte und Hinweise stehen englisch im Code und werden beim Zeichnen übersetzt (`LooperLookAndFeel`); Auswahlfelder (`LLCombo::setEnglishItems`) und der Editor wechseln sofort, wenn die Sprache umgestellt wird.
- Parameternamen werden aus der Parameter-ID gebildet (`params::englishName`, `params::localisedName`) und beim Erzeugen des Plug-ins festgelegt. Die IDs ändern sich nie – Projekte und MIDI-Maps funktionieren in beiden Sprachen.
- Die Spracheinstellung liegt in der Datei `livelooper.settings` (macOS `~/Library/Application Support/livelooper/`, Windows `%APPDATA%\livelooper\`, Linux `~/.config/livelooper/`). Die Umgebungsvariable `LIVELOOPER_LANGUAGE=en|de` hat Vorrang (nutzt der Host-Test).
- Neuer Text: englisch mit `tr()` in den Code schreiben und die deutsche Übersetzung in die Tabelle in `Localisation.cpp` eintragen; der Test „Every choice, effect and effect parameter has a German text" findet fehlende Auswahlwerte und Effekte.

## Rhythmus 1.6: Samples, Texte, Packs

- **Drum-Samples**: `tools/samples/build_drum_samples.py <SRC> .` baut `resources/drums` und `src/engine/DrumSamples.inc` neu. Die Stil-Kits nutzen nur echte CC0-Aufnahmen: die sfzinstruments-Kits (Virtuosity Drums, Big Rusty Drums, Swirly Drums, Frankensnare, Body Percussion) lädt das Skript bei Bedarf nach `<SRC>/sfz/`; Sonic-Pi-Samples liegen in `<SRC>/sonicpi`, das TR-808-Set in `<SRC>/808` (alle URLs in `SOURCES.md`). Die Kit-Indizes sind die Werte des Kit-Parameters – nur anhängen, nie umsortieren.
- **Deutsche Texte der Rhythmus-Funktionen** stehen in einer eigenen Tabelle: `tools/i18n/rhythm16_de.py` bearbeiten und ausführen; es schreibt `src/plugin/Localisation_Rhythm16.inc` (eingebunden von `Localisation.cpp`).
- **BEATS**: `BeatPattern` (reine Daten) und `BeatPatternBank` (16 Plätze aus Atomics, bearbeitet im Message-Thread, gelesen vom Audio-Thread) in `RhythmGenerator.h`; der Prozessor speichert die Plätze im Element `BEATS` des States. `src/plugin/BeatsPage.*` ist die Editor-Seite, `src/plugin/SoundPacks.*` liest Packs und die Pattern-Bibliothek und baut die Drum-Bank (Bänke werden zur Laufzeit nie freigegeben, der Audio-Thread darf also einen Rohzeiger halten).
- **Tests**: `tests/Rhythm16Tests.cpp` (Kits, Variation, Sequencer-Timing, quantisierter Wechsel, Fill auf Knopfdruck, Zeilen-Mixer, Drum-FX-Routing, Pack-Format / Zip-Import / Plätze, BEATS-State) und die STEP-Modus-Tests in `tests/Push15Tests.cpp`.

## CI / Releases

`.github/workflows/ci.yml`

- jeder Push (`main`, `v*-wip`) / Pull Request: Linux wird einmal gebaut; danach laufen die Unit-Tests in 3 parallelen Teilen (`livelooper_tests --shard i/3`) und VST3-Host-Test + pluginval in einem parallelen Job, alle auf dem Build-Artefakt
- reine Doku-Änderungen (`docs/**`, `*.md`, inkl. `docs/marketing`) überspringen die Builds – außer den Hilfedateien, die ins Plug-in kompiliert werden (USER-GUIDE / BEDIENUNGSANLEITUNG, PUSH, MIDI, INSTALLATION, `docs/brand`, THIRD-PARTY.md). Vollen Lauf erzwingen: `[ci full]` in der Commit-Nachricht, Label `force-ci` am Pull Request oder ein manueller Start
- ein neuer Push bricht die laufende CI desselben Branches / Pull Requests ab (Tag-Läufe nie); der Job **CI result** ist der eine Status, auf den man schaut
- Tempo: ccache auf Linux, macOS und Windows (Schlüssel: OS + Compiler + Branch, Rückfall auf den neuesten Cache des Branches, dann `v1.7-wip` / `main`), der JUCE-Download inkl. `juceaide` wird gecacht; macOS und Windows bauen mit Ninja nur Plug-ins, App und Tests (ohne Screenshot-Werkzeug)
- mit warmem Cache kommt fast jede Objektdatei aus ccache; die restliche Bauzeit ist vor allem die Link-Time-Optimierung (LTO) der Plug-in- und App-Binaries, die kein Cache überspringen kann
- Linux-Test-Builds bei Push und PR konfigurieren mit `-DLIVELOOPER_LTO=OFF` und sparen sich diesen Link-Schritt; manuelle (Dispatch-)Builds und Tags behalten LTO, getestet werden also die ausgelieferten Binaries. Lokal ist LTO standardmäßig an
- Checkout mit Submodulen (rekursiv) für Ableton Link
- Tag `v*` oder manueller Start: zusätzlich macOS (Universal, VST3 + AU + App) und Windows (VST3 + App), Tests, pluginval, und (Installer bei Tags, bei manuellen Starts, solange die Eingabe **build_installers** nicht ausgeschaltet ist, und bei Pull Requests mit dem Label `installer`; Label `plugins` = nur macOS- + Windows-ZIPs):
  - **macOS**: Ad-hoc-Codesignatur mit fester Kennung (`codesign --sign - --identifier com.groovex.livelooper --entitlements resources/macos/livelooper.entitlements`, Hardened Runtime aus), ZIP (mit `install.command`, erstellt mit `ditto`) und **`livelooper-macOS.pkg`** (`pkgbuild` pro Komponente + `productbuild` mit `installer/macos/distribution.xml`)
  - **Windows**: ZIP und **`livelooper-Windows-Setup.exe`** (Inno Setup, per Chocolatey installiert, wenn `iscc` fehlt)
- Tag `v*`: GitHub-Release mit den ZIPs (weiterhin der primäre / manuelle Weg) plus `livelooper-macOS.pkg` und `livelooper-Windows-Setup.exe`

Signatur: Es gibt noch keine Apple-Developer-ID / Notarisierung und kein Windows-Zertifikat, daher warnen Gatekeeper und SmartScreen einmal (INSTALLATION.md). Mit einer Developer ID ersetzt man den Ad-hoc-`codesign`-Schritt (Hardened Runtime + Entitlements), signiert das pkg mit `productsign` und notarisiert mit `notarytool`.

Release erstellen:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## Bekannte Einschränkungen

- Ohne *Tempo folgen* (Spur-SETUP) ändern Tempoänderungen nach der Aufnahme die Loop-Geschwindigkeit nicht.
- Ein Undo-Schritt pro Spur (der letzte Overdub-Durchgang).
- Wechselt die Samplerate oder die maximale Loop-Länge, werden die Loops umgerechnet; laufende Aufnahmen gehen dabei verloren.
- Die Builds sind nur ad-hoc signiert (macOS) bzw. unsigniert (Windows): Gatekeeper und SmartScreen warnen einmal (siehe INSTALLATION.md).
- Parameternamen im Host wechseln die Sprache erst nach dem Neuladen des Plug-ins.
