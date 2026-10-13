# Installation

**English** | [Deutsch](../de/INSTALLATION.md)

## Download

Ready-made plug-ins are on the GitHub page under **Releases** (`livelooper-macOS.zip`, `livelooper-Windows.zip`). A release is created automatically when a version tag (`v1.0.0` …) is pushed. Without a release: start a build under **Actions → CI → Run workflow** and download the artifact – or build it yourself ([DEVELOPMENT.md](DEVELOPMENT.md)).

## macOS

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
4. Let your DAW rescan its plug-ins. Logic Pro and GarageBand use the Audio Unit (`.component`), most other hosts the VST3. In Ableton Live, for example: *Settings → Plug-ins* → turn on "Audio Units" or "VST3 Plug-in System Folders" and click **Rescan**.

Universal binary: runs natively on Apple Silicon and Intel.

## Windows

1. Unzip the file.
2. Copy `livelooper.vst3` to `C:\Program Files\Common Files\VST3\`.
3. Let your DAW rescan its VST3 plug-ins (this is the standard VST3 folder every host searches). In Ableton Live, for example: *Options → Preferences → Plug-ins* → turn on "Use VST3 Plug-in System Folders" and click **Rescan**.

## Setting up in your DAW

livelooper works the same way in every VST3/AU host. The short version is below; step-by-step setups for each DAW – looping software instruments, switching sources live, monitoring without doubled signal, direct outputs and MIDI routing – are in **[DAW-SETUP.md](DAW-SETUP.md)**.

1. Create an **audio track** whose input is your mic/instrument (mono or stereo).
2. **Enable input monitoring** on that track so you hear the input (and so it reaches the plug-in) even when the track is not record-armed – how this is done depends on your DAW.
3. **Insert livelooper** as an effect on that track. The input runs through the input FX into the loops and – with the **INPUT** knob – straight to the output. If you already hear the direct signal through your interface, turn **INPUT** to 0.
4. For **MIDI control**, send your controller's MIDI to livelooper – depending on the DAW through the track's MIDI input or a MIDI track routed to the plug-in – or map livelooper's host parameters with your DAW's own MIDI mapping. An Ableton Push needs none of this: livelooper talks to it directly ([PUSH.md](PUSH.md)).
5. With **HOST SYNC** (default) livelooper follows the host tempo, time signature and transport.

### Example: Ableton Live

**Audio input**

1. Create an audio track, *Audio From* = your interface input (mono or stereo).
2. Set *Monitor* to **In** (always hear the input, even when the track is not armed).
3. Drag livelooper onto the track. The input runs through the input FX into the loops and – with the **INPUT** knob – straight to the output. If you already hear the direct signal through your interface, turn **INPUT** to 0.

**MIDI controller / foot switch** – two ways, both usable at the same time:

- *livelooper's own MIDI learn*: create a MIDI track, *MIDI From* = your controller, *Monitor* = **In**, *MIDI To* = the audio track with livelooper → "livelooper". Then right-click a control in the plug-in → **MIDI Learn**.
- *Live's MIDI map* (your DAW's MIDI mapping): click **Configure** in the device, move the controls you want in the plug-in, then assign them as usual with *MIDI* (Cmd/Ctrl+M).

More in [MIDI.md](MIDI.md).

**Tempo**: with **HOST SYNC** (default) livelooper follows Live's tempo, time signature and position. When the transport is stopped, an internal clock keeps counting at the host tempo – in Live as in any other DAW.

## Standalone app

You don't need a DAW at all: the **standalone app** (part of the same download) is livelooper on its own – pick your audio interface under **Options → Audio/MIDI Settings**, connect Push and play. It adds a cue output for the metronome and direct track outputs on your interface's outputs (user guide, sections 14 and 20). Use the plug-in when you want your DAW's instruments, effects and recording around it ([DAW-SETUP.md](DAW-SETUP.md)).

## Standalone app: no input / recordings stay silent

The standalone app mutes the audio input by default to avoid feedback. Open **Options → Audio/MIDI Settings**, select your input device (e.g. the built-in microphone), enable its input channels and untick **Mute audio input**. Use headphones when recording with the built-in microphone. On macOS also allow the microphone under **System Settings → Privacy & Security → Microphone → livelooper**.

## Standalone app: computer keyboard

The standalone app can be played with the computer keyboard (1–5 REC/PLAY, Q–T STOP, Space ALL START/STOP, Return TAP …). The livelooper window must be the active window. The mapping can be changed under **SETUP → Keyboard mapping …** – see the user guide, section 10.

## Language

livelooper speaks English and German. By default it follows the language of your operating system; change it under **SETUP → Language / Sprache**. The choice is saved for all livelooper instances on this computer. Parameter names shown by your DAW switch after the plug-in is reloaded (e.g. reopen the project).

## System requirements

- macOS 11 or later, Windows 10/11 (64 bit)
- A VST3 host (Windows, macOS) or Audio Unit host (macOS), e.g. Ableton Live (10.1 or later, any edition), Bitwig Studio, Cubase, Studio One, Reaper, FL Studio, Logic Pro or GarageBand – or no DAW at all with the standalone app
- Memory: each track reserves `2 × maximum loop length` of stereo audio (for undo). At 48 kHz and 60 s (default) that is about 230 MB for five tracks (eight tracks: about 370 MB); adjustable under **SETUP → Maximum loop length**.
