# livelooper

**English** | [Deutsch](README.de.md)

**livelooper is an instrument, not a recorder.** It is a five- or eight-track loop station for Ableton Live (VST3 and Audio Unit, plus a standalone app) built to be *played*: to explore an idea, layer it, bend it and perform it live – with your hands on an **Ableton Push**, a MIDI controller or the computer keyboard, and your eyes on the audience instead of the screen.

![livelooper](docs/images/livelooper.png)

## The idea

- **Live performance comes first.** Every function reacts instantly, lands on the beat and never stops the music. Nothing needs a menu while you play.
- **Explore and play.** Capture what you just improvised, throw an effect on it with the XY pad, pitch it, reverse it, let it decay like tape – and keep what works.
- **Hands on hardware.** Plug in an Ableton Push and livelooper takes it over – no mapping. Any other controller works through MIDI learn, every one of the 372 functions is mappable.
- **Your loops belong in your song.** Drag a loop straight from livelooper onto a track in Live, export stems, or keep everything in the Live set.

## Play it with Ableton Push

![livelooper on Ableton Push](docs/images/livelooper-live-push.jpg)

Connect a Push (1 and up), press **User**, and livelooper turns it into a dedicated looper surface – in the plug-in and in the standalone app:

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

**Work with Live**
- **In sync with Live**: tempo, time signature and transport, or an internal clock with tap tempo / typed-in tempo
- **Drag loops into Live**: drag a track's waveform onto an audio track or into the browser – a tempo-tagged WAV lands on Live's grid
- **Stem export** with tempo metadata, WAV export per track, drag & drop audio files onto a track to import
- **Direct track outputs** (plug-in multi-out, interface outputs in the standalone app), **cue output** for the metronome
- **Saved with your project**: loops stored losslessly (FLAC, 24 bit) in the Live set; whole **looper sets** as `.llset` files
- **Meters and waveforms**: stereo meter per track, master meter, waveform with play position, clip LED
- **All 372 functions** are host parameters for Live's MIDI map
- **English and German** interface (*SETUP → Language / Sprache*)

## Why VST3/AU?

livelooper is a JUCE plug-in (C++). Compared with a Max for Live device this has three advantages: it runs in every edition of Live (not only Suite), the audio engine runs sample-accurate and real-time safe in native code, and it can be tested automatically. The same plug-in also works in other DAWs and as a standalone app.

## Quick start

1. Install the plug-in – see [Installation](docs/en/INSTALLATION.md)
2. In Live, create an audio track with your mic/instrument as input, set Monitor to **In** and drag **livelooper** from *Plug-ins → Audio Units / VST3* onto the track
3. Click the big REC/PLAY pad of track 1: recording starts on the next bar; click again: the loop plays

Everything else is in the **[User Guide](docs/en/USER-GUIDE.md)**, MIDI control in **[MIDI](docs/en/MIDI.md)**.

## Documentation

| Document | Contents |
|---|---|
| [INSTALLATION.md](docs/en/INSTALLATION.md) | Download, installation on macOS and Windows, setup in Live |
| [USER-GUIDE.md](docs/en/USER-GUIDE.md) | Interface, all functions, effects, rhythm, setup, language |
| [MIDI.md](docs/en/MIDI.md) | MIDI learn, Live's MIDI map, default mapping, parameter list |
| [PUSH.md](docs/en/PUSH.md) | Ableton Push control: setup, pad layout, encoders, FX XY pad |
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

No license has been chosen for the livelooper source code yet. livelooper is based on the JUCE framework, which is available under AGPLv3 or a commercial JUCE license – its terms apply when distributing binaries.

Third-party code in `third_party/`: [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) and Signalsmith Linear (time-stretch / pitch shift), both MIT licensed – see their `LICENSE.txt`.
