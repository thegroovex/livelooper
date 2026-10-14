# Installation

**English** | [Deutsch](../de/INSTALLATION.md)

## Download

Ready-made builds are on the GitHub page under **[Releases](https://github.com/thegroovex/livelooper/releases)**:

| File | What it is |
|---|---|
| `livelooper-macOS.pkg` | **macOS installer (recommended)**: app, VST3 and Audio Unit in one go |
| `livelooper-macOS.zip` | macOS, manual installation (unchanged): app, VST3, AU and `install.command` |
| `livelooper-Windows-Setup.exe` | **Windows installer (recommended)**: app with Start menu shortcut and VST3, with uninstaller |
| `livelooper-Windows.zip` | Windows, manual installation (unchanged): app and VST3 |

A release is created automatically when a version tag (`v1.5.0` …) is pushed. Without a release: start a build under **Actions → CI → Run workflow** and download the artifacts – or build it yourself ([DEVELOPMENT.md](DEVELOPMENT.md)).

livelooper is free software and is **not yet signed with an Apple Developer ID or a Windows code-signing certificate**. That is why macOS and Windows show a warning the first time – the steps below get you past it once. The warnings will only disappear with a future Developer ID signature and Apple notarisation (and a Windows certificate).

## macOS

Universal binary: runs natively on Apple silicon and Intel.

### A. Installer (recommended)

1. Double-click `livelooper-macOS.pkg`. macOS blocks it the first time ("Apple could not verify …") – click **Done** (not *Move to Trash*).
2. Open **System Settings → Privacy & Security**, scroll down to the **Security** section. It says that `livelooper-macOS.pkg` was blocked – click **Open Anyway**.
3. Confirm with your password or Touch ID and click **Open** in the dialog that appears again.
4. Follow the installer. Under **Customize** you can leave out parts:
   - `livelooper.app` → `/Applications`
   - `livelooper.vst3` → `/Library/Audio/Plug-Ins/VST3`
   - `livelooper.component` → `/Library/Audio/Plug-Ins/Components`
5. Let your DAW rescan its plug-ins (step "Rescan" below).

The installer removes the download quarantine from the installed files and refreshes the Audio Unit cache, so the app and the plug-ins open without a further warning.

### B. ZIP with install.command

1. Unzip `livelooper-macOS.zip`.
2. Double-click `install.command`. If macOS blocks it, allow it like the installer (**System Settings → Privacy & Security → Security → Open Anyway**, confirm with your password / Touch ID) and double-click it again.
3. It installs for your user account, without an administrator password: the app to `~/Applications`, the plug-ins to `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`, and removes the quarantine.

### C. ZIP, by hand (unchanged)

1. Unzip `livelooper-macOS.zip`.
2. Copy:
   - `livelooper.component` → `~/Library/Audio/Plug-Ins/Components/`
   - `livelooper.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - optionally `livelooper.app` → `Applications`
3. Allow the files once – either with **Open Anyway** (below) or in Terminal (removes the download quarantine from the app and both plug-in bundles):
   ```bash
   xattr -dr com.apple.quarantine /Applications/livelooper.app
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/livelooper.component
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/livelooper.vst3
   ```

### "livelooper can't be opened" – Open Anyway, step by step

If macOS blocks the app (or a DAW reports the plug-in as damaged / not verified):

1. Try to open `livelooper.app` once. macOS shows "Apple could not verify 'livelooper' is free of malware …" – click **Done**.
2. Open **Apple menu → System Settings → Privacy & Security** and scroll down to **Security**.
3. Next to the message about livelooper, click **Open Anyway**. (The button is only shown for about an hour after the attempt.)
4. Confirm with your login password or Touch ID, then click **Open**.
5. From now on livelooper opens normally with a double-click.

Note: on current macOS (Sequoia 15 and later) **right-click → Open no longer bypasses this warning** – use **Open Anyway** in System Settings or the Terminal command above. For plug-ins, the Terminal command (or the installer, which does it for you) is the most reliable way.

### Rescan in your DAW

Logic Pro and GarageBand use the Audio Unit (`.component`), most other hosts the VST3. In Ableton Live, for example: *Settings → Plug-ins* → turn on "Audio Units" or "VST3 Plug-in System Folders" and click **Rescan**.

### Microphone permission (standalone app)

The first time the standalone app uses an input, macOS asks for microphone access – click **Allow**. livelooper itself asks only once. Every build carries the same identifier (`com.groovex.livelooper`), but as long as livelooper has no Developer ID signature, macOS may ask again after an update. If you clicked *Don't Allow* or recordings stay silent:

- **System Settings → Privacy & Security → Microphone** → switch **livelooper** on, or
- reset the decision in Terminal and start livelooper again (macOS asks again):
  ```bash
  tccutil reset Microphone com.groovex.livelooper
  ```

In a DAW the microphone permission belongs to the DAW, not to the plug-in.

## Windows

### A. Installer (recommended)

1. Double-click `livelooper-Windows-Setup.exe`.
2. Windows SmartScreen may show "Windows protected your PC" – click **More info**, then **Run anyway**.
3. Choose the language (English / Deutsch), accept the license (AGPLv3) and the components:
   - livelooper app → `C:\Program Files\livelooper` with a Start menu shortcut (optionally a desktop shortcut)
   - VST3 plug-in → `C:\Program Files\Common Files\VST3\livelooper.vst3`
4. Let your DAW rescan its VST3 plug-ins.

Uninstall with *Settings → Apps → livelooper → Uninstall* (or the Start menu entry); it removes the app and the VST3.

### B. ZIP, by hand (unchanged)

1. Unzip `livelooper-Windows.zip`.
2. Copy `livelooper.vst3` to `C:\Program Files\Common Files\VST3\`; put `livelooper.exe` wherever you like.
3. If SmartScreen warns when you start `livelooper.exe`: **More info → Run anyway**.
4. Let your DAW rescan its VST3 plug-ins (this is the standard VST3 folder every host searches). In Ableton Live, for example: *Options → Preferences → Plug-ins* → turn on "Use VST3 Plug-in System Folders" and click **Rescan**.

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

You don't need a DAW at all: the **standalone app** (part of the same download) is livelooper on its own – pick your audio interface with the **Audio/MIDI Settings** button at the top left of the window, connect Push and play. It adds a cue output for the metronome and direct track outputs on your interface's outputs (user guide, sections 14 and 20). Use the plug-in when you want your DAW's instruments, effects and recording around it ([DAW-SETUP.md](DAW-SETUP.md)).

## Standalone app: no input / recordings stay silent

The standalone app mutes the audio input by default to avoid feedback. Click **Audio/MIDI Settings** (top left of the window) → *Audio/MIDI Settings …*, select your input device (e.g. the built-in microphone), enable its input channels and untick **Mute audio input**. Use headphones when recording with the built-in microphone. On macOS also allow the microphone (see *Microphone permission* above).

## Standalone app: computer keyboard

The standalone app can be played with the computer keyboard (1–5 REC/PLAY, Q–T STOP, Space ALL START/STOP, Return TAP …). The livelooper window must be the active window. The mapping can be changed under **SETUP → Keyboard mapping …** – see the user guide, section 10.

## First start: settings and updates

On the first start livelooper asks once whether it should start with the **last used settings** or the **default settings** (change it any time under **SETUP → Settings on start**). Updates: **SETUP → Updates** or the **About** window (**SETUP → About livelooper …**) check GitHub for a new version; the app can download and install it when you quit, plug-ins are prepared and installed with your DAW closed. Nothing is checked unless you switch it on.

## Language

livelooper speaks English and German. By default it follows the language of your operating system; change it under **SETUP → Language / Sprache**. The choice is saved for all livelooper instances on this computer. Parameter names shown by your DAW switch after the plug-in is reloaded (e.g. reopen the project).

## System requirements

- macOS 11 or later, Windows 10/11 (64 bit)
- A VST3 host (Windows, macOS) or Audio Unit host (macOS), e.g. Ableton Live (10.1 or later, any edition), Bitwig Studio, Cubase, Studio One, Reaper, FL Studio, Logic Pro or GarageBand – or no DAW at all with the standalone app
- Memory: each track reserves `2 × maximum loop length` of stereo audio (for undo). At 48 kHz and 60 s (default) that is about 230 MB for five tracks (eight tracks: about 370 MB); adjustable under **SETUP → Maximum loop length**.
