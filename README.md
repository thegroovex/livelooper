# livelooper

**English** | [Deutsch](README.de.md)

**livelooper is an instrument, not a recorder.** It is a five- or eight-track loop station for your DAW – a VST3 and Audio Unit plug-in for every compatible host (Ableton Live, Bitwig Studio, Cubase, Studio One, Reaper, FL Studio, Logic Pro, GarageBand …) plus a standalone app – built to be *played*: to explore an idea, layer it, bend it and perform it live – with your hands on an **Ableton Push**, a MIDI controller or the computer keyboard, and your eyes on the audience instead of the screen.

![livelooper](docs/images/livelooper.png)

## The idea

- **Live performance comes first.** Every function reacts instantly, lands on the beat and never stops the music. Nothing needs a menu while you play.
- **Explore and play.** Capture what you just improvised, throw an effect on it with the XY pad, pitch it, reverse it, let it decay like tape – and keep what works.
- **Hands on hardware.** Plug in an Ableton Push and livelooper takes it over – no mapping. Any other controller works through MIDI learn, every one of the 372 functions is mappable.
- **Your loops belong in your song.** Drag a loop straight from livelooper onto a track in your DAW, export stems, or keep everything saved with your project.

## Play it with Ableton Push

![livelooper on Ableton Push](docs/images/livelooper-live-push.jpg)

Connect a Push (1 and up), press **User**, and livelooper turns it into a dedicated looper surface – in the plug-in in any DAW and in the standalone app, because livelooper talks to Push directly over its User port:

- **One pad column per track**: REC / PLAY / DUB and STOP, the loop position running up the column as light, colours showing the state, blinking while waiting for the bar
- **Encoders** for level, pan, pitch, feedback or effects, a **master section** (master level, tempo) and swing on the swing encoder
- **Scenes** on the scene buttons, **capture**, undo, metronome, tap tempo, **DELETE + pad** clears a track
- **Loop cutting**: tap a position pad and the loop jumps there on the next 16th, hold it to stutter, let go and it is back in sync
- **Performance moves**: hold MUTE / SOLO + pad for drops, DOUBLE = loop × 2 (SHIFT = half), OCTAVE ±12, DUPLICATE + pads copies a loop, REC pad + position pad = 1–32 bars
- **Touch strip** = tape stop that springs back in sync (SHIFT = filter sweep), **footswitch** = REC/PLAY, double tap STOP, hold CLEAR
- **FX XY pad**: the 8×8 grid becomes a performance pad – touch, slide, press harder for more, let go and it snaps back
- **Display** with names, values, track states, tempo and level meters

Setup and the full layout: **[PUSH.md](docs/en/PUSH.md)**.

## Features

**Perform**
- **5 or 8 loop tracks**: REC → DUB → PLAY on one pad, STOP (hold = clear), undo/redo, reverse, one shot, level, pan
- **Scenes**: 8 scene buttons start and stop groups of tracks in time
- **Capture**: turn the last 1–8 bars you played into a loop afterwards – no record press needed
- **Auto record**: armed tracks start with the first note above a threshold – hands stay on the instrument
- **Quantization** to the bar or beat; pressing "too late" is corrected up to 15 % after the bar line; fixed lengths 1–16 bars or free
- **Start/stop modes**: immediate, fade in/out, stop at loop end; overdub or replace
- **Ghost play head**: stopped tracks show where they would be now, so you drop them back in exactly
- **Loop cutting and stutter**, **momentary mute / solo**, **loop × 2 / half**, **copy to another track**, **tape stop** and a **performance filter** – everything quantized musically, nothing ever stops the music
- **Ableton Push**, **MIDI learn** for any controller (right-click any control), and **computer keyboard** control in the standalone app

**Shape the sound**
- **Input FX and track FX**: four slots each with 12 effects – LPF, HPF, BPF, phaser, flanger, delay (tempo-synced), reverb, slicer, tremolo, auto pan, lo-fi, overdrive
- **Per track**: feedback (tape-style decay), sidechain ducking, pitch ±12 semitones, follow tempo (time-stretch, Signalsmith Stretch), resampling of the other tracks
- **Rhythm**: 4 kits (Studio, 808, 909, Lo-Fi), 21 grooves from rock to trap, dembow, UK garage and Afro house, swing and feel; metronome with 4 sounds

**Work with your DAW**
- **In sync with your DAW**: host tempo, time signature and transport, or an internal clock with tap tempo / typed-in tempo
- **Drag loops into your DAW**: drag a track's waveform onto an audio track or into the DAW's browser – a tempo-tagged WAV (ACID metadata), which most DAWs place right on the grid
- **Stem export** with tempo metadata, WAV export per track, drag & drop audio files onto a track to import
- **Direct track outputs** (plug-in multi-out – each track on its own output in your DAW – or interface outputs in the standalone app), **cue output** for the metronome
- **Saved with your project**: loops stored losslessly (FLAC, 24 bit) in your DAW project; whole **looper sets** as `.llset` files
- **Meters and waveforms**: stereo meter per track, master meter, waveform with play position, clip LED
- **All 372 functions** are host parameters for your DAW's MIDI mapping and automation
- **English and German** interface (*SETUP → Language / Sprache*)

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
| [INSTALLATION.md](docs/en/INSTALLATION.md) | Download, installation on macOS and Windows, setup in your DAW (with Ableton Live as an example) |
| [DAW-SETUP.md](docs/en/DAW-SETUP.md) | Using the plug-in in your DAW: looper track, monitoring, looping mic/guitar and software instruments, switching sources live, getting audio out, MIDI routing (Live, Bitwig, Cubase, Logic, Reaper, Studio One) |
| [USER-GUIDE.md](docs/en/USER-GUIDE.md) | Interface, all functions, effects, rhythm, setup, language |
| [MIDI.md](docs/en/MIDI.md) | MIDI learn, your DAW's MIDI mapping, default mapping, parameter list |
| [PUSH.md](docs/en/PUSH.md) | Ableton Push control (in any DAW and standalone): setup, pad layout, encoders, FX XY pad |
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
