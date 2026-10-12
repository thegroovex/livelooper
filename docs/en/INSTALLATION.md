# Installation

**English** | [Deutsch](../de/INSTALLATION.md)

## Download

Ready-made plug-ins are on the GitHub page under **Releases** (`livelooper-macOS.zip`, `livelooper-Windows.zip`). A release is created automatically when a version tag (`v1.0.0` …) is pushed. Without a release: start a build under **Actions → CI → Run workflow** and download the artifact – or build it yourself ([DEVELOPMENT.md](DEVELOPMENT.md)).

## macOS (Ableton Live 11/12)

1. Unzip the file.
2. Copy:
   - `livelooper.component` → `~/Library/Audio/Plug-Ins/Components/`
   - `livelooper.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - optionally `livelooper.app` → `Applications`
3. The builds are not notarized by Apple. Allow them once in Terminal:
   ```bash
   xattr -cr ~/Library/Audio/Plug-Ins/Components/livelooper.component
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/livelooper.vst3
   ```
4. In Live: *Settings → Plug-ins* → turn on "Audio Units" or "VST3 Plug-in System Folders" and click **Rescan**.

Universal binary: runs natively on Apple Silicon and Intel.

## Windows

1. Unzip the file.
2. Copy `livelooper.vst3` to `C:\Program Files\Common Files\VST3\`.
3. In Live: *Options → Preferences → Plug-ins* → turn on "Use VST3 Plug-in System Folders" and click **Rescan**.

## Setting up Live

**Audio input**

1. Create an audio track, *Audio From* = your interface input (mono or stereo).
2. Set *Monitor* to **In** (always hear the input, even when the track is not armed).
3. Drag livelooper onto the track. The input runs through the input FX into the loops and – with the **INPUT** knob – straight to the output. If you already hear the direct signal through your interface, turn **INPUT** to 0.

**MIDI controller / foot switch** – two ways, both usable at the same time:

- *livelooper's own MIDI learn*: create a MIDI track, *MIDI From* = your controller, *Monitor* = **In**, *MIDI To* = the audio track with livelooper → "livelooper". Then right-click a control in the plug-in → **MIDI Learn**.
- *Live's MIDI map*: click **Configure** in the device, move the controls you want in the plug-in, then assign them as usual with *MIDI* (Cmd/Ctrl+M).

More in [MIDI.md](MIDI.md).

**Tempo**: with **HOST SYNC** (default) livelooper follows Live's tempo, time signature and position. When Live's transport is stopped, an internal clock keeps counting at Live's tempo.

## Standalone app: no input / recordings stay silent

The standalone app mutes the audio input by default to avoid feedback. Open **Options → Audio/MIDI Settings**, select your input device (e.g. the built-in microphone), enable its input channels and untick **Mute audio input**. Use headphones when recording with the built-in microphone. On macOS also allow the microphone under **System Settings → Privacy & Security → Microphone → livelooper**.

## Language

livelooper speaks English and German. By default it follows the language of your operating system; change it under **SETUP → Language / Sprache**. The choice is saved for all livelooper instances on this computer. Parameter names shown by Live switch after the plug-in is reloaded (e.g. reopen the set).

## System requirements

- macOS 11 or later, Windows 10/11 (64 bit)
- Ableton Live 10.1 or later (VST3), any edition
- Memory: each track reserves `2 × maximum loop length` of stereo audio (for undo). At 48 kHz and 60 s (default) that is about 230 MB for all five tracks; adjustable under **SETUP → Maximum loop length**.
