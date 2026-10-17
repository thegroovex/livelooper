# livelooper

[English](README.md) | **Deutsch**

<p align="center">
  <a href="https://github.com/thegroovex/livelooper/releases/latest"><img src="https://img.shields.io/github/v/release/thegroovex/livelooper?style=for-the-badge&label=%E2%AC%87%20Neueste%20Version%20herunterladen&color=FF2D6F&labelColor=B14BFF" alt="⬇ Neueste Version herunterladen" height="40"></a>
</p>

<p align="center"><b><a href="https://github.com/thegroovex/livelooper/releases/latest">⬇ Neueste Version herunterladen</a></b> – Installer für <b>Windows</b> (<code>livelooper-Windows-Setup.exe</code>) und <b>macOS</b> (<code>livelooper-macOS.pkg</code>), dazu ZIPs · <a href="docs/de/INSTALLATION.md">Installation</a></p>

**livelooper ist ein Instrument, kein Rekorder.** Eine Loop-Station mit fünf oder acht Spuren für deine DAW – ein VST3- und Audio-Unit-Plug-in für alle kompatiblen Hosts (Ableton Live, Bitwig Studio, Cubase, Studio One, Reaper, FL Studio, Logic Pro, GarageBand …), zusätzlich als Standalone-App –, gebaut zum *Spielen*: eine Idee erforschen, schichten, verbiegen und live aufführen – mit den Händen auf einem **Ableton Push**, einem MIDI-Controller oder der Computertastatur und dem Blick beim Publikum statt auf dem Bildschirm.

![livelooper](docs/images/livelooper-de.png)

## Die Idee

- **Live-Performance zuerst.** Jede Funktion reagiert sofort, landet auf dem Schlag und hält die Musik nie an. Beim Spielen braucht nichts ein Menü.
- **Spaß am Spielen.** Beat-FX, Beat Repeat und Stutter, Color-FX und ein Groove, der sich wie ein Drummer anfühlt, machen aus einem Loop eine Performance – drücken, halten, loslassen, und du bist wieder auf dem Schlag.
- **Erforschen und spielen.** Halte fest, was du gerade improvisiert hast, leg mit dem XY-Pad einen Effekt darauf, verstimm es, dreh es um, lass es ausklingen wie ein Band – und behalte, was funktioniert.
- **Hände an die Hardware.** Schließ einen Ableton Push an und livelooper übernimmt ihn – ohne Mapping. Jeder andere Controller funktioniert per MIDI Learn, alle 501 Funktionen sind zuweisbar.
- **Deine Loops gehören in deinen Song.** Zieh einen Loop direkt aus livelooper auf eine Spur in deiner DAW, exportiere Stems oder behalte alles im Projekt gespeichert.

## Spielen mit Ableton Push

![livelooper auf Push 1](docs/images/livelooper-push1.png)

Push (ab Version 1) anschließen, **User** drücken – und livelooper macht daraus eine eigene Looper-Oberfläche – im Plug-in in jeder DAW und in der Standalone-App, denn livelooper spricht Push direkt über dessen User-Port an:

- **Eine Pad-Spalte pro Spur**: REC / PLAY / DUB und STOP, die Loop-Position läuft als Licht die Spalte hoch, Farben zeigen den Status, Blinken heißt „wartet auf den Takt“
- **Encoder** für Pegel, Panorama, Tonhöhe, Feedback oder Effekte, eine **Master-Sektion** (Gesamtpegel, Tempo) und Swing auf dem Swing-Encoder
- **Szenen** auf den Szenen-Tasten, **Capture**, Undo, Metronom, Tap-Tempo, **DELETE + Pad** löscht eine Spur
- **Loop-Cutting**: Positions-Pad antippen und der Loop springt zur nächsten 16tel dorthin, halten lässt ihn stottern, loslassen – und er ist wieder synchron
- **Performance-Moves**: MUTE / SOLO + Pad halten für Drops, DOUBLE = Loop × 2 (SHIFT = halbieren), OCTAVE ±12, DUPLICATE + Pads kopiert einen Loop, REC-Pad + Positions-Pad = 1–32 Takte
- **Touchstrip** = Tape-Stopp, der synchron zurückspringt (SHIFT = Filter-Sweep), **Fußschalter** = REC/PLAY, doppelt STOP, halten LÖSCHEN
- **FX-XY-Pad**: Das 8×8-Raster wird zum Performance-Pad – anfassen, schieben, fester drücken für mehr, loslassen, und es springt zurück
- **Performance-FX (1.5)**: ADD EFFECT = Beat-FX an / aus, ADD TRACK = Beat-FX solange gehalten, LEFT / RIGHT = Schlag, BROWSE = Encoder für Typ, Schlag, Pegel, Kanal, Color-FX, Repeat- / Stutter-Timing; REPEAT / ACCENT = Beat Repeat / Stutter solange gehalten (SHIFT = Latch), dabei über den Touchstrip streichen für 1/2 … 1/32; FIXED LENGTH = Capture 1–32 Takte; SHIFT + METRONOME = Ableton Link
- **Display** mit Namen, Werten, Spurstatus, Tempo und Pegelanzeige

Einrichtung und komplette Belegung: **[PUSH.md](docs/de/PUSH.md)**.

## Funktionen

**Performance**
- **5 oder 8 Loop-Spuren**: REC → DUB → PLAY auf einem Pad, STOP (halten = löschen), Undo/Redo, Reverse, One-Shot, Pegel, Panorama
- **Szenen**: 8 Szenen-Knöpfe starten und stoppen Spurgruppen im Takt
- **Capture**: Die zuletzt gespielten 1–32 Takte werden nachträglich zum Loop – ganz ohne Aufnahme-Taste
- **Auto-Aufnahme**: Scharf geschaltete Spuren starten mit dem ersten Ton über einer Schwelle – die Hände bleiben am Instrument
- **Quantisierung** auf Takt oder Schlag; „zu spät“ gedrückt wird bis 15 % nach dem Taktstrich korrigiert; feste Längen 1–16 Takte oder frei
- **Start-/Stopp-Modi**: sofort, Fade-In/-Out, Stopp am Loop-Ende; Overdub oder Replace
- **Geister-Abspielmarke**: Gestoppte Spuren zeigen, wo sie gerade wären – so holst du sie punktgenau zurück
- **Loop-Cutting und Stottern**, **Stumm / Solo solange gehalten**, **Loop × 2 / halbieren**, **in eine andere Spur kopieren**, **Tape-Stopp** und ein **Performance-Filter** – alles musikalisch quantisiert, nichts hält je die Musik an
- **Ableton Push**, **MIDI Learn** für jeden Controller (Rechtsklick auf ein Bedienelement) und **Computertastatur** in der Standalone-App

<!-- v1.6 features -->
**Neu in 1.6 (in Entwicklung)**
- **Willkommensbildschirm, Demo-Set und geführte Tour**: Beim ersten Start wählst du *Demo-Set* (ein Groove und drei Loops – ALLE START / STOP drücken und nach Sekunden läuft Musik) oder *leeres Set*; eine 60-Sekunden-Tour zeigt Aufnehmen, Overdub, Groove, Beat-FX und Szenen
- **Loop-Tricks**: halbe und doppelte Geschwindigkeit, Loop-Audio umdrehen – quantisiert, rückgängig machbar, per MIDI zuweisbar, auf Push 1 mit SHIFT
- **Performance-Recorder**: die gesamte livelooper-Ausgabe als WAV mit einer runden Aufnahmetaste (oben rechts, blinkt im Takt), mit Zeitanzeige
- **Eigene Samples auf den BEATS-Zeilen**: beliebige Audiodatei auf eine Zeile ziehen (oder *Sample laden …*), mit einem Klick zurück zum Kit-Sample – wird mit dem Set gespeichert
- **Clip-Export 9:16**: die letzten 20 Sekunden (oder eine Aufnahme) als gebrandetes Hochformat-Video für Reels, TikTok und Shorts
- **Sets teilen**: eine `.livelooper`-Datei mit allem Audio, per Drag & Drop geöffnet – das Demo-Set liegt in [`presets`](presets/)
- **Controller-Vorlagen** für Launchpad Mini MK3 / X, APC mini mk2 und die Pads von Launchkey MK3 / MK4 (automatisch erkannt, mit LED-Rückmeldung) – *experimentell – nicht auf Hardware getestet, Feedback willkommen*

*Drums und BEATS*

- **12 Drum-Kits**: Studio, 808, 909 Style, Lo-Fi plus House, Techno, Boom Bap, Trap, Breaks, Rock, Disco und Afro Latin – echte CC0-Aufnahmen (akustische Sets, Handpercussion, gesampelte Drumcomputer), jedes mit passenden Grooves (insgesamt 39) und eigenem Fill-Stil
- **Lebendige Drums**: mehr Takes, Velocity-Layer, Round Robin über Layer hinweg, feine Tonhöhen- / Ausklang-Variation und Akzent-Kicks (VARIATION) – kein Maschinengewehr
- **BEATS-Step-Sequencer**: 8 Instrumente × 16 oder 32 Schritte, 1 – 4 Takte, Dynamik / Akzent pro Schritt, stumm / solo / Pegel / Tonhöhe pro Zeile, Kopieren / Einfügen / leichte Variation, 16 User-Patterns pro Set plus Pattern-Bibliothek; Pattern-Wechsel auf den nächsten Schlag oder Takt
- **FILL auf Knopfdruck**: FILL halten für einen Fill, loslassen und der Groove landet auf der nächsten Eins
- **Drums durch Beat FX und Beat Repeat / Stutter** – ein Schalter pro Effektgruppe
- **Sound-Packs**: Kits und Patterns importieren (Ordner oder Zip, offenes Format) – siehe [SOUND-PACKS.md](docs/de/SOUND-PACKS.md)
- **Push-1-STEP-Modus**: die Pads als Step-Sequencer, FILL auf IN
- **Punch-FX und Send-FX**: 12 schnelle Effekte zum Halten (Bandstopp, Beat-Repeat, Filter, Granular …), Druck oder Anschlagstärke = Stärke, dazu ein Send-Effekt auf den Loops
- **Push-1-Ergänzungen**: Pad-Druck zerhackt einen Loop (1/4 – 1/32), alle FX auf dem XY-Pad, Beat-Blinker und Set-Aufnahme-Pad, Encoder doppelt berühren, Spurfarben
- **EP-133 K.O. II (experimentell – bitte testen)**: anschließen und loslegen als Controller (4 Pad-Seiten) oder aufnehmen, livelooper als Clock-Master, Pattern resamplen; Controller-Oberfläche für Push 1 und K.O. II
- **Beat-Bibliothek**: SPEICHERN legt einen Beat unter seinem Namen ab (Schritte, Dynamik, Länge, Swing, Zeilen-Mixer), LADEN listet deine gespeicherten Beats oder öffnet eine Datei; mit den Host-Parametern *Nächster / Vorheriger Bibliotheks-Beat* blätterst du durch
<!-- end v1.6 features -->

**Live-Performance-FX (neu in 1.5)**
- **Beat-FX** wie an einem DJ-Mixer: 16 taktsynchrone Effekte (Delay, Echo, Ping Pong, Spiral, Hall, Trans, Filter, Flanger, Phaser, Pitch, Roll, Slip Roll, Reverse Roll, Vinyl Brake, Helix, Noise) mit Schlagwert 1/32 … 16 Schläge, Pegel / Tiefe, ON oder HOLD, auf Master, Loops, Eingang oder einer Spur
- **Beat Repeat und Stutter**: Halten (oder einrasten) wiederholt 1/2 … 1/32 Noten ab der nächsten Rasterlinie; die Musik läuft darunter weiter, Loslassen ist also immer im Takt
- **Color-FX**: ein Regler pro Spur (COLOR) – Filter, Dub Echo, Space, Crush, Noise, Sweep; Mitte = aus
- **Metronom-Block**: Klick an / aus, Betonung, Pegel und Klang direkt auf der Oberfläche
- **Ableton Link**: Tempo und Schlag mit anderen Apps und Geräten im selben Netzwerk (Standalone-App oder in der DAW mit HOST-SYNC aus)

**Klang formen**
- **Input-FX und Track-FX**: je vier Slots mit 12 Effekten – Tiefpass, Hochpass, Bandpass, Phaser, Flanger, Delay (tempo-synchron), Hall, Slicer, Tremolo, Auto-Pan, Lo-Fi, Overdrive
- **Pro Spur**: Feedback (Ausklingen wie ein Band), Sidechain-Ducking, Tonhöhe ±12 Halbtöne, Tempo folgen (Time-Stretch, Signalsmith Stretch), Resample der anderen Spuren
- **Rhythmus** mit aufgenommenen Drum-Samples (CC0 / gemeinfrei): 4 Kits (Studio, 808, 909 Style, Lo-Fi), 21 Patterns von Rock bis Trap, Dembow, UK Garage und Afro House, 6 Groove-Feels (gerade, natürlich, laid back, treibend, drunk, drive), automatische Fills alle 4 oder 8 Takte, Swing und Feel; Metronom mit 4 Klängen

**Arbeiten mit deiner DAW**
- **Synchron zur DAW**: Host-Tempo, Taktart und Transport (ein DAW-Stopp stoppt die Loops – abschaltbar) – oder interne Clock mit Tap-Tempo / eingetipptem Tempo
- **Latenz messen und ausgleichen** (1.7): ein Klick misst die Roundtrip-Latenz deines Interfaces (Loopback-Ping), gespeichert pro Audiogerät; jede Aufnahme, jeder Overdub und jedes Capture landet auf dem Beat
- **Loop-Länge an jeder Spur** (1.7): AUTO oder 1 – 16 Takte direkt in der Spur und in der Live-Ansicht (Klick, Scrollen, MIDI-Learn); am Push 1 FIXED LENGTH halten + Pad oder Encoder
- **Live-Ansicht, Farbschemas, Bühnenmodus** (1.7): eine (auch bildschirmfüllende) Bühnenansicht mit großen Spur-Karten und Performance-FX-Leiste, helles und dunkles Farbschema, kontrastreicher Bühnenmodus, Oberflächengröße 75 – 200 %, große Wellenformen mit Overdub-Schicht
- **Loops in die DAW ziehen**: Zieh die Wellenform einer Spur auf eine Audiospur oder in den Browser der DAW – eine WAV mit Tempo-Infos (ACID-Metadaten), die die meisten DAWs direkt ins Raster legen
- **Stem-Export** mit Tempo-Metadaten, WAV-Export pro Spur, Audiodateien per Drag & Drop auf eine Spur importieren
- **Direkte Spurausgänge** (Plug-in Multi-Out – jede Spur als eigener Ausgang in deiner DAW – oder Interface-Ausgänge in der Standalone-App), **Cue-Ausgang** für das Metronom
- **Mit dem Projekt gespeichert**: Loops verlustfrei (FLAC, 24 Bit) im DAW-Projekt; komplette **Looper-Sets** als `.llset`-Datei
- **Pegel und Wellenformen**: Stereo-Pegel pro Spur, Master-VU in dB im Display (grün / gelb / rot), Wellenform mit Abspielposition, Clip-LED
- **Komfort**: Hilfe in der App (durchsuchbare Bedienungsanleitung), Info-Fenster mit Update-Prüfung, Start mit den zuletzt verwendeten oder den Standard-Einstellungen, Liste der zuletzt verwendeten Sets
- **Alle 501 Funktionen** sind Host-Parameter für MIDI-Mapping und Automation deiner DAW
- **Deutsche und englische** Oberfläche (*SETUP → Language / Sprache*)

## Download

Die neueste Version gibt es auf der **[Release-Seite](https://github.com/thegroovex/livelooper/releases)**:

| System | Empfohlen | Alternative (unverändert) |
|---|---|---|
| macOS (Apple Silicon und Intel) | `livelooper-macOS.pkg` – Installer für App, VST3 und AU | `livelooper-macOS.zip` – von Hand kopieren oder `install.command` doppelklicken |
| Windows (64 Bit) | `livelooper-Windows-Setup.exe` – Installer für App (Startmenü) und VST3 | `livelooper-Windows.zip` – von Hand kopieren |

livelooper ist noch nicht mit einer Apple-Developer-ID oder einem Windows-Zertifikat signiert, deshalb warnen macOS (Gatekeeper) und Windows (SmartScreen) einmal – [INSTALLATION.md](docs/de/INSTALLATION.md) zeigt Schritt für Schritt, wie du es trotzdem öffnest. Blockiert deine DAW das Plug-in weiterhin, starte `fix-gatekeeper.command` (im ZIP, nach dem Installer unter `/Library/Application Support/livelooper`).

## Welche DAWs?

livelooper läuft in jedem Host, der **VST3**-Plug-ins (Windows und macOS) oder **Audio Units** (macOS) lädt – zum Beispiel Ableton Live, Bitwig Studio, Cubase, Studio One, Reaper, FL Studio (VST3), Logic Pro und GarageBand (AU) – und als **Standalone-App** ganz ohne DAW. Es ist ein Audio-Effekt mit MIDI-Eingang: auf eine Audiospur setzen, und es folgt Tempo und Transport des Hosts.

## Warum ein natives Plug-in?

livelooper ist ein JUCE-Plug-in (C++). Als natives Plug-in läuft es in jedem VST3-/AU-Host, statt an eine DAW oder eine bestimmte Edition gebunden zu sein, die Audio-Engine läuft sample-genau und echtzeitsicher in nativem Code, und es ist automatisiert testbar.

## Schnellstart

1. Plug-in installieren – siehe [Installation](docs/de/INSTALLATION.md)
2. In deiner DAW eine Audiospur mit deinem Mikro/Instrument als Eingang anlegen, das Eingangs-Monitoring einschalten und **livelooper** (VST3 oder AU) auf die Spur setzen – in Ableton Live zum Beispiel: Monitor auf **In**, dann livelooper aus *Plug-ins* auf die Spur ziehen. Oder einfach die Standalone-App starten.
3. Das große REC/PLAY-Pad von Spur 1 klicken: Aufnahme startet auf dem nächsten Takt, nochmal klicken: Loop läuft

Alles Weitere steht in der **[Bedienungsanleitung](docs/de/BEDIENUNGSANLEITUNG.md)**, die MIDI-Steuerung in **[MIDI](docs/de/MIDI.md)**.

## Dokumentation

| Dokument | Inhalt |
|---|---|
| [INSTALLATION.md](docs/de/INSTALLATION.md) | Download, Installer und manuelle Installation auf macOS und Windows, Gatekeeper / SmartScreen, Mikrofon-Freigabe, Einrichtung in deiner DAW |
| [DAW-EINRICHTUNG.md](docs/de/DAW-EINRICHTUNG.md) | Das Plug-in in deiner DAW: Looper-Spur, Monitoring, Mikro/Gitarre und Software-Instrumente loopen, live zwischen Quellen wechseln, Audio herausholen, MIDI-Routing (Live, Bitwig, Cubase, Logic, Reaper, Studio One) |
| [BEDIENUNGSANLEITUNG.md](docs/de/BEDIENUNGSANLEITUNG.md) | Oberfläche, alle Funktionen, Beat-FX, Beat Repeat / Stutter, Color-FX, Rhythmus, Link, Setup, Sprache |
| [MIDI.md](docs/de/MIDI.md) | MIDI-Learn, MIDI-Mapping deiner DAW, Standard-Belegung, Parameterliste |
| [PUSH.md](docs/de/PUSH.md) | Steuerung mit Ableton Push (in jeder DAW und standalone): Einrichtung, Pad-Belegung, Encoder, FX-XY-Pad, Performance-FX, STEP-Modus |
| [KO2.md](docs/de/KO2.md) | EP-133 K.O. II steuern (experimentell – bitte testen): Seiten, Rollen, Clock, Aufnahme, Testliste |
| [SOUND-PACKS.md](docs/de/SOUND-PACKS.md) | Sound-Packs: Installation, Ordner-Aufbau, Pack- und Pattern-Format (zum Bauen und Verkaufen von Packs) |
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

livelooper ist freie Software: Sie dürfen sie unter den Bedingungen der **GNU Affero General Public License Version 3** (AGPLv3), veröffentlicht von der Free Software Foundation, weitergeben und/oder verändern – siehe [LICENSE](LICENSE). Sie wird in der Hoffnung verbreitet, dass sie nützlich ist, aber OHNE JEDE GEWÄHRLEISTUNG.

Warum AGPLv3: livelooper baut auf JUCE 8 auf (genutzt unter dessen AGPLv3-Option) und enthält Ableton Link (GPLv2 oder neuer); das Gesamtwerk steht daher unter AGPLv3. Wer livelooper weitergibt (Programmdateien oder veränderte Versionen), muss auch den zugehörigen Quellcode unter derselben Lizenz anbieten.

Fremdkomponenten und ihre Lizenzen – JUCE (AGPLv3), Ableton Link und asio-standalone, Signalsmith Stretch / Linear (MIT) und die Drum-Samples – stehen in [THIRD-PARTY.md](THIRD-PARTY.md).

Ableton, Push, Link und alle anderen genannten Produktnamen sind Marken ihrer jeweiligen Inhaber; livelooper steht in keiner Verbindung zu ihnen und wird nicht von ihnen unterstützt.
