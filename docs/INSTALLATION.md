# Installation

## Download

Fertige Plug-ins gibt es auf der GitHub-Seite unter **Releases** (`livelooper-macOS.zip`, `livelooper-Windows.zip`). Ein Release entsteht automatisch, wenn ein Versions-Tag (`v1.0.0` …) gepusht wird. Ohne Release: unter **Actions → CI → Run workflow** einen Build starten und das Artefakt herunterladen – oder selbst bauen ([ENTWICKLUNG.md](ENTWICKLUNG.md)).

## macOS (Ableton Live 11/12)

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
4. In Live: *Einstellungen → Plug-ins* → „Audio Units" bzw. „VST3-Plug-in-Systemordner" einschalten und **Neu scannen**.

Universal Binary: läuft nativ auf Apple Silicon und Intel.

## Windows

1. ZIP entpacken.
2. `livelooper.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
3. In Live: *Optionen → Einstellungen → Plug-ins* → „VST3-Plug-in-Systemordner verwenden" einschalten und **Neu scannen**.

## Einrichtung in Live

**Audio-Eingang**

1. Audiospur anlegen, *Audio From* = dein Interface-Eingang (Mono oder Stereo).
2. *Monitor* auf **In** (dauerhaft mithören, auch wenn die Spur nicht scharf ist).
3. livelooper auf die Spur ziehen. Das Eingangssignal läuft durch die Input-FX in die Loops und – mit dem Regler **INPUT** – direkt auf den Ausgang. Wer das Direktsignal schon über das Interface hört, dreht **INPUT** auf 0.

**MIDI-Controller / Fußschalter** – zwei Wege, beide gleichzeitig nutzbar:

- *Eigenes MIDI-Learn von livelooper*: eine MIDI-Spur anlegen, *MIDI From* = dein Controller, *Monitor* = **In**, *MIDI To* = die Audiospur mit livelooper → „livelooper". Dann im Plug-in Rechtsklick auf ein Bedienelement → **MIDI Learn**.
- *Live-MIDI-Map*: im Gerät auf **Konfigurieren** klicken, die gewünschten Regler im Plug-in bewegen, dann wie gewohnt mit *MIDI* (Cmd/Strg+M) zuweisen.

Mehr dazu in [MIDI.md](MIDI.md).

**Tempo**: Mit **HOST SYNC** (Standard) folgt livelooper dem Tempo, der Taktart und der Position von Live. Läuft Lives Transport nicht, zählt eine interne Clock im Live-Tempo weiter.

## Systemanforderungen

- macOS 11 oder neuer, Windows 10/11 (64 Bit)
- Ableton Live 10.1 oder neuer (VST3), jede Edition
- Arbeitsspeicher: pro Spur werden `2 × Max. Loop-Länge` Stereo-Audio reserviert (für Undo). Bei 48 kHz und 60 s (Standard) sind das rund 230 MB für alle fünf Spuren; einstellbar unter **SETUP → Maximum loop length**.
