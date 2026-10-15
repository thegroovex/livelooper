# livelooper

**English** | [Deutsch](README.de.md)

**livelooper is an instrument, not a recorder.** It is a five- or eight-track loop station for your DAW – a VST3 and Audio Unit plug-in for every compatible host (Ableton Live, Bitwig Studio, Cubase, Studio One, Reaper, FL Studio, Logic Pro, GarageBand …) plus a standalone app – built to be *played*: to explore an idea, layer it, bend it and perform it live – with your hands on an **Ableton Push**, a MIDI controller or the computer keyboard, and your eyes on the audience instead of the screen.

![livelooper](docs/images/livelooper.png)

## The idea

- **Live performance comes first.** Every function reacts instantly, lands on the beat and never stops the music. Nothing needs a menu while you play.
- **Fun to play.** Beat FX, beat repeat and stutter, colour FX and a groove that feels like a drummer turn a loop into a performance – press, hold, let go, and you are back on the beat.
- **Explore and play.** Capture what you just improvised, throw an effect on it with the XY pad, pitch it, reverse it, let it decay like tape – and keep what works.
- **Hands on hardware.** Plug in an Ableton Push and livelooper takes it over – no mapping. Any other controller works through MIDI learn, every one of the 465 functions is mappable.
- **Your loops belong in your song.** Drag a loop straight from livelooper onto a track in your DAW, export stems, or keep everything saved with your project.

## Play it with Ableton Push

![livelooper on Push 1](docs/images/livelooper-push1.png)

Connect a Push (1 and up), press **User**, and livelooper turns it into a dedicated looper surface – in the plug-in in any DAW and in the standalone app, because livelooper talks to Push directly over its User port:

- **One pad column per track**: REC / PLAY / DUB and STOP, the loop position running up the column as light, colours showing the state, blinking while waiting for the bar
- **Encoders** for level, pan, pitch, feedback or effects, a **master section** (master level, tempo) and swing on the swing encoder
- **Scenes** on the scene buttons, **capture**, undo, metronome, tap tempo, **DELETE + pad** clears a track
- **Loop cutting**: tap a position pad and the loop jumps there on the next 16th, hold it to stutter, let go and it is back in sync
- **Performance moves**: hold MUTE / SOLO + pad for drops, DOUBLE = loop × 2 (SHIFT = half), OCTAVE ±12, DUPLICATE + pads copies a loop, REC pad + position pad = 1–32 bars
- **Touch strip** = tape stop that springs back in sync (SHIFT = filter sweep), **footswitch** = REC/PLAY, double tap STOP, hold CLEAR
- **FX XY pad**: the 8×8 grid becomes a performance pad – touch, slide, press harder for more, let go and it snaps back
- **Performance FX (1.5)**: ADD EFFECT = beat FX on / off, ADD TRACK = beat FX while held, LEFT / RIGHT = beat, BROWSE = encoders for type, beat, level, channel, colour FX, repeat / stutter timing; REPEAT / ACCENT = beat repeat / stutter while held (SHIFT = latch), hold them and slide on the touch strip for 1/2 … 1/32; FIXED LENGTH = capture 1–32 bars; SHIFT + METRONOME = Ableton Link
- **Display** with names, values, track states, tempo and level meters

Setup and the full layout: **[PUSH.md](docs/en/PUSH.md)**.

## Features

**Perform**
- **5 or 8 loop tracks**: REC → DUB → PLAY on one pad, STOP (hold = clear), undo/redo, reverse, one shot, level, pan
- **Scenes**: 8 scene buttons start and stop groups of tracks in time
- **Capture**: turn the last 1–32 bars you played into a loop afterwards – no record press needed
- **Auto record**: armed tracks start with the first note above a threshold – hands stay on the instrument
- **Quantization** to the bar or beat; pressing "too late" is corrected up to 15 % after the bar line; fixed lengths 1–16 bars or free
- **Start/stop modes**: immediate, fade in/out, stop at loop end; overdub or replace
- **Ghost play head**: stopped tracks show where they would be now, so you drop them back in exactly
- **Loop cutting and stutter**, **momentary mute / solo**, **loop × 2 / half**, **copy to another track**, **tape stop** and a **performance filter** – everything quantized musically, nothing ever stops the music
- **Ableton Push**, **MIDI learn** for any controller (right-click any control), and **computer keyboard** control in the standalone app

<!-- v1.6 features -->
**New in 1.6 (in development)**
- **Welcome screen, demo set and guided tour**: on the first start choose *demo set* (a groove and three loops – press ALL START / STOP and you hear music in seconds) or *empty set*; a 60-second tour shows record, overdub, groove, beat FX and scenes
- **Loop tricks**: half speed, double speed and reversing the loop audio – quantized, undoable, MIDI-mappable, on Push 1 with SHIFT
- **Performance recorder**: the whole livelooper output as WAV with one round record button (top right, blinks with the beat), with the elapsed time
- **Own samples on the BEATS rows**: drop any audio file on a row (or *Load sample …*), back to the kit sample with one click – saved with the set
- **Clip export 9:16**: the last 20 seconds (or a recording) as a branded vertical video for Reels, TikTok and Shorts
- **Share sets**: one `.livelooper` file with all audio, opened by drag and drop – the demo set is in [`presets`](presets/)
- **Controller templates** for Launchpad Mini MK3 / X, APC mini mk2 and Launchkey MK3 / MK4 pads (auto-detected, with LED feedback) – *experimental – untested on hardware, feedback welcome*

*Drums and BEATS*

- **12 drum kits**: Studio, 808, 909 Style, Lo-Fi plus House, Techno, Boom Bap, Trap, Breaks, Rock, Disco and Afro Latin – real CC0 recordings (acoustic kits, hand percussion, sampled machines), each with matching grooves (39 in total) and its own fill style
- **Lively drums**: more takes, velocity layers, round robin across layers, subtle pitch / decay variation and accent kicks (VARIATION) – no machine gun
- **BEATS step sequencer**: 8 voices × 16 or 32 steps, 1 – 4 bars, velocity / accent per step, mute / solo / level / pitch per row, copy / paste / light variation, 16 user patterns per set plus a pattern library; pattern changes switch on the next beat or bar
- **FILL on demand**: hold FILL for a fill, let go and the groove lands on the next downbeat
- **Drums through beat FX and beat repeat / stutter** – a switch per effect group
- **Sound packs**: import kits and patterns (folder or zip, open format) – see [SOUND-PACKS.md](docs/en/SOUND-PACKS.md)
- **Push 1 STEP mode**: the pads as a step sequencer, FILL on IN
- **Beat library**: SAVE stores a beat by name (steps, velocities, length, swing, row mixer), LOAD lists your saved beats or opens a file; step through them with the host parameters *Next / Previous Library Beat*
<!-- end v1.6 features -->

**Live performance FX (new in 1.5)**
- **Beat FX** in the style of a DJ mixer: 16 beat-synced effects (delay, echo, ping pong, spiral, reverb, trans, filter, flanger, phaser, pitch, roll, slip roll, reverse roll, vinyl brake, helix, noise) with beat fraction 1/32 … 16 beats, level / depth, ON or HOLD, on the master, the loops, the input or one track
- **Beat repeat and stutter**: hold (or latch) to repeat 1/2 … 1/32 notes from the next grid line; the music runs on underneath, so releasing is always in time
- **Colour FX**: one knob per track (COLOR) – filter, dub echo, space, crush, noise, sweep; centre = off
- **Metronome block**: click on / off, accent, level and sound right on the surface
- **Ableton Link**: tempo and beat with other apps and devices on the same network (standalone app, or in a DAW with HOST SYNC off)

**Shape the sound**
- **Input FX and track FX**: four slots each with 12 effects – LPF, HPF, BPF, phaser, flanger, delay (tempo-synced), reverb, slicer, tremolo, auto pan, lo-fi, overdrive
- **Per track**: feedback (tape-style decay), sidechain ducking, pitch ±12 semitones, follow tempo (time-stretch, Signalsmith Stretch), resampling of the other tracks
- **Rhythm** with recorded drum samples (CC0 / public domain): 4 kits (Studio, 808, 909 Style, Lo-Fi), 21 patterns from rock to trap, dembow, UK garage and Afro house, 6 groove feels (straight, natural, laid back, pushed, drunk, drive), automatic fills every 4 or 8 bars, swing and feel; metronome with 4 sounds

**Work with your DAW**
- **In sync with your DAW**: host tempo, time signature and transport, or an internal clock with tap tempo / typed-in tempo
- **Drag loops into your DAW**: drag a track's waveform onto an audio track or into the DAW's browser – a tempo-tagged WAV (ACID metadata), which most DAWs place right on the grid
- **Stem export** with tempo metadata, WAV export per track, drag & drop audio files onto a track to import
- **Direct track outputs** (plug-in multi-out – each track on its own output in your DAW – or interface outputs in the standalone app), **cue output** for the metronome
- **Saved with your project**: loops stored losslessly (FLAC, 24 bit) in your DAW project; whole **looper sets** as `.llset` files
- **Meters and waveforms**: stereo meter per track, master VU in dB on the display (green / yellow / red), waveform with play position, clip LED
- **Comfort**: in-app help (searchable user guide), about window with update check, start with the last used or the default settings, list of the last used sets
- **All 465 functions** are host parameters for your DAW's MIDI mapping and automation
- **English and German** interface (*SETUP → Language / Sprache*)

## Download

Get the latest version from the **[releases page](https://github.com/thegroovex/livelooper/releases)**:

| System | Recommended | Alternative (unchanged) |
|---|---|---|
| macOS (Apple silicon and Intel) | `livelooper-macOS.pkg` – installer for the app, VST3 and AU | `livelooper-macOS.zip` – copy by hand or double-click `install.command` |
| Windows (64-bit) | `livelooper-Windows-Setup.exe` – installer for the app (Start menu) and VST3 | `livelooper-Windows.zip` – copy by hand |

livelooper is not signed with an Apple Developer ID or a Windows certificate yet, so macOS (Gatekeeper) and Windows (SmartScreen) warn once – [INSTALLATION.md](docs/en/INSTALLATION.md) shows step by step how to open it anyway. If your DAW still blocks the plug-in, run `fix-gatekeeper.command` (in the ZIP, and after the installer in `/Library/Application Support/livelooper`).

## Which DAWs?

livelooper runs in every host that loads **VST3** plug-ins (Windows and macOS) or **Audio Units** (macOS) – for example Ableton Live, Bitwig Studio, Cubase, Studio One, Reaper, FL Studio (VST3), Logic Pro and GarageBand (AU) – and as a **standalone app** without any DAW. It is an audio effect with MIDI input: put it on an audio track, and it follows the host's tempo and transport.

## Why a native plug-in?

livelooper is a JUCE plug-in (C++). As a native plug-in it runs in any VST3/AU host instead of being tied to one DAW or one edition of it, the audio engine runs sample-accurate and real-time safe in native code, and it can be tested automatically.

## Quick start

1. Install the plug-in – see [Installation](docs/en/INSTALLATION.md)
2. In your DAW, create an audio track with your mic/instrument as input, enable input monitoring and insert **livelooper** (VST3 or AU) on the track – in Ableton Live, for example: Monitor **In**, then drag livelooper from *Plug-ins* onto the track. Or simply start the standalone app.
3. Click the big REC/PLAY pad of track 1: recording starts on the next bar; click again: the loop plays

Everything else is in the **[User Guide](docs/en/USER-GUIDE.md)**, MIDI control in **[MIDI](docs/en/MIDI.md)**.

## Documentation

| Document | Contents |
|---|---|
| [INSTALLATION.md](docs/en/INSTALLATION.md) | Download, installers and manual installation on macOS and Windows, Gatekeeper / SmartScreen, microphone permission, setup in your DAW |
| [DAW-SETUP.md](docs/en/DAW-SETUP.md) | Using the plug-in in your DAW: looper track, monitoring, looping mic/guitar and software instruments, switching sources live, getting audio out, MIDI routing (Live, Bitwig, Cubase, Logic, Reaper, Studio One) |
| [USER-GUIDE.md](docs/en/USER-GUIDE.md) | Interface, all functions, beat FX, beat repeat / stutter, colour FX, rhythm, Link, setup, language |
| [MIDI.md](docs/en/MIDI.md) | MIDI learn, your DAW's MIDI mapping, default mapping, parameter list |
| [PUSH.md](docs/en/PUSH.md) | Ableton Push control (in any DAW and standalone): setup, pad layout, encoders, FX XY pad, performance FX, STEP mode |
| [SOUND-PACKS.md](docs/en/SOUND-PACKS.md) | Sound packs: installing, folder layout, the pack and pattern format (for making and selling packs) |
| [DEVELOPMENT.md](docs/en/DEVELOPMENT.md) | Building from source, architecture, tests, CI, translations |

German documentation: [docs/de](docs/de/).

## Build it yourself

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

JUCE 8 is downloaded automatically during configuration. Details in [DEVELOPMENT.md](docs/en/DEVELOPMENT.md).

## License

livelooper is free software: you can redistribute it and/or modify it under the terms of the **GNU Affero General Public License version 3** (AGPLv3) as published by the Free Software Foundation – see [LICENSE](LICENSE). It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY.

Why AGPLv3: livelooper is built on JUCE 8 (used under its AGPLv3 option) and includes Ableton Link (GPLv2 or later), so the combined work is AGPLv3. If you pass on livelooper (binaries or modified versions), you must also offer the corresponding source code under the same license.

Third-party components and their licenses – JUCE (AGPLv3), Ableton Link and asio-standalone, Signalsmith Stretch / Linear (MIT) and the drum samples – are listed in [THIRD-PARTY.md](THIRD-PARTY.md).

Ableton, Push, Link and all other product names mentioned are trademarks of their respective owners; livelooper is not affiliated with or endorsed by them.
