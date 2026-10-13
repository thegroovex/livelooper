# Installation

[English](../en/INSTALLATION.md) | **Deutsch**

## Download

Fertige Plug-ins gibt es auf der GitHub-Seite unter **Releases** (`livelooper-macOS.zip`, `livelooper-Windows.zip`). Ein Release entsteht automatisch, wenn ein Versions-Tag (`v1.0.0` …) gepusht wird. Ohne Release: unter **Actions → CI → Run workflow** einen Build starten und das Artefakt herunterladen – oder selbst bauen ([ENTWICKLUNG.md](ENTWICKLUNG.md)).

## macOS

1. ZIP entpacken.
2. Kopieren:
   - `livelooper.component` → `~/Library/Audio/Plug-Ins/Components/`
   - `livelooper.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - optional `livelooper.app` → `Programme`
3. Die Builds sind nicht von Apple notarisiert. Einmal im Terminal freigeben:
   ```bash
   xattr -cr ~/Library/Audio/Plug-Ins/Components/livelooper.component
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/livelooper.vst3
   ```
4. Die DAW ihre Plug-ins neu scannen lassen. Logic Pro und GarageBand verwenden die Audio Unit (`.component`), die meisten anderen Hosts das VST3. In Ableton Live zum Beispiel: *Einstellungen → Plug-ins* → „Audio Units" bzw. „VST3-Plug-in-Systemordner" einschalten und **Neu scannen**.

Universal Binary: läuft nativ auf Apple Silicon und Intel.

## Windows

1. ZIP entpacken.
2. `livelooper.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
3. Die DAW ihre VST3-Plug-ins neu scannen lassen (das ist der Standard-VST3-Ordner, den jeder Host durchsucht). In Ableton Live zum Beispiel: *Optionen → Einstellungen → Plug-ins* → „VST3-Plug-in-Systemordner verwenden" einschalten und **Neu scannen**.

## Einrichtung in deiner DAW

livelooper funktioniert in jedem VST3-/AU-Host gleich. Unten die Kurzfassung; Schritt-für-Schritt-Anleitungen für jede DAW – Software-Instrumente loopen, live zwischen Quellen wechseln, Monitoring ohne doppeltes Signal, direkte Ausgänge und MIDI-Routing – stehen in **[DAW-EINRICHTUNG.md](DAW-EINRICHTUNG.md)**.

1. Eine **Audiospur** anlegen, deren Eingang dein Mikro/Instrument ist (Mono oder Stereo).
2. **Eingangs-Monitoring einschalten**, damit du den Eingang hörst (und er das Plug-in erreicht), auch wenn die Spur nicht aufnahmebereit ist – wie das geht, hängt von deiner DAW ab.
3. **livelooper** als Effekt auf diese Spur setzen. Das Eingangssignal läuft durch die Input-FX in die Loops und – mit dem Regler **EINGANG** – direkt auf den Ausgang. Wer das Direktsignal schon über das Interface hört, dreht **EINGANG** auf 0.
4. Für die **MIDI-Steuerung** das MIDI deines Controllers an livelooper schicken – je nach DAW über den MIDI-Eingang der Spur oder eine MIDI-Spur, die auf das Plug-in geroutet ist – oder die Host-Parameter von livelooper mit dem MIDI-Mapping deiner DAW zuweisen. Ein Ableton Push braucht nichts davon: livelooper spricht ihn direkt an ([PUSH.md](PUSH.md)).
5. Mit **HOST-SYNC** (Standard) folgt livelooper Tempo, Taktart und Transport des Hosts.

### Beispiel: Ableton Live

**Audio-Eingang**

1. Audiospur anlegen, *Audio From* = dein Interface-Eingang (Mono oder Stereo).
2. *Monitor* auf **In** (dauerhaft mithören, auch wenn die Spur nicht scharf ist).
3. livelooper auf die Spur ziehen. Das Eingangssignal läuft durch die Input-FX in die Loops und – mit dem Regler **EINGANG** – direkt auf den Ausgang. Wer das Direktsignal schon über das Interface hört, dreht **EINGANG** auf 0.

**MIDI-Controller / Fußschalter** – zwei Wege, beide gleichzeitig nutzbar:

- *Eigenes MIDI-Learn von livelooper*: eine MIDI-Spur anlegen, *MIDI From* = dein Controller, *Monitor* = **In**, *MIDI To* = die Audiospur mit livelooper → „livelooper". Dann im Plug-in Rechtsklick auf ein Bedienelement → **MIDI Learn**.
- *Live-MIDI-Map* (das MIDI-Mapping der DAW): im Gerät auf **Konfigurieren** klicken, die gewünschten Regler im Plug-in bewegen, dann wie gewohnt mit *MIDI* (Cmd/Strg+M) zuweisen.

Mehr dazu in [MIDI.md](MIDI.md).

**Tempo**: Mit **HOST-SYNC** (Standard) folgt livelooper dem Tempo, der Taktart und der Position von Live. Läuft der Transport nicht, zählt eine interne Clock im Host-Tempo weiter – in Live wie in jeder anderen DAW.

## Standalone-App

Du brauchst gar keine DAW: Die **Standalone-App** (im selben Download) ist livelooper für sich allein – Audio-Interface unter **Options → Audio/MIDI Settings** wählen, Push anschließen und spielen. Sie bietet zusätzlich einen Cue-Ausgang fürs Metronom und direkte Spurausgänge auf den Ausgängen deines Interfaces (Bedienungsanleitung, Abschnitte 14 und 20). Das Plug-in nimmst du, wenn du die Instrumente, Effekte und Aufnahme deiner DAW drumherum willst ([DAW-EINRICHTUNG.md](DAW-EINRICHTUNG.md)).

## Standalone-App: kein Eingang / Aufnahmen bleiben stumm

Die Standalone-App schaltet den Audio-Eingang standardmäßig stumm, um Rückkopplungen zu vermeiden. Öffne **Options → Audio/MIDI Settings**, wähle dein Eingangsgerät (z. B. das eingebaute Mikrofon), aktiviere dessen Eingangskanäle und entferne den Haken bei **Mute audio input**. Nimm mit dem eingebauten Mikrofon am besten mit Kopfhörern auf. Unter macOS zusätzlich das Mikrofon erlauben: **Systemeinstellungen → Datenschutz & Sicherheit → Mikrofon → livelooper**.

## Standalone-App: Computertastatur

Die Standalone-App lässt sich mit der Computertastatur spielen (1–5 REC/PLAY, Q–T STOP, Leertaste ALLE START/STOP, Eingabe TAP …). Das livelooper-Fenster muss dafür das aktive Fenster sein. Die Belegung lässt sich unter **SETUP → Tastaturbelegung …** ändern – siehe Bedienungsanleitung, Abschnitt 10.

## Sprache

livelooper spricht Deutsch und Englisch. Standardmäßig folgt es der Sprache des Betriebssystems; umstellen unter **SETUP → Language / Sprache**. Die Wahl gilt für alle livelooper-Instanzen auf diesem Rechner. Die Parameternamen, die deine DAW anzeigt, wechseln nach dem Neuladen des Plug-ins (z. B. Projekt neu öffnen).

## Systemanforderungen

- macOS 11 oder neuer, Windows 10/11 (64 Bit)
- Ein VST3-Host (Windows, macOS) oder Audio-Unit-Host (macOS), z. B. Ableton Live (10.1 oder neuer, jede Edition), Bitwig Studio, Cubase, Studio One, Reaper, FL Studio, Logic Pro oder GarageBand – oder ganz ohne DAW mit der Standalone-App
- Arbeitsspeicher: pro Spur werden `2 × Max. Loop-Länge` Stereo-Audio reserviert (für Undo). Bei 48 kHz und 60 s (Standard) sind das rund 230 MB für fünf Spuren (acht Spuren: rund 370 MB); einstellbar unter **SETUP → Maximale Loop-Länge**.
