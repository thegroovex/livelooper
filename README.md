# livelooper

**English** | [Deutsch](README.de.md)

**livelooper** is a five-track loop station plug-in for Ableton Live (VST3 and Audio Unit, plus a standalone app). Record, overdub, effects, rhythm, and every single function freely mappable to MIDI – built for live looping on stage and in the studio.

![livelooper](docs/images/livelooper.png)

## Features

- **5 loop tracks** with REC → DUB → PLAY, STOP (hold = clear), undo/redo of the last overdub, level, pan, reverse, one shot, loop sync
- **Quantization** to the bar or beat, locked to Live's tempo, time signature and transport (or an internal clock with tap tempo)
- **Fixed loop lengths** (1, 2, 4, 8, 16 bars) or free length; pressing "too late" is corrected retroactively up to 15 % after the bar line
- **Input FX and track FX**: four slots each (A–D) with 12 effects – LPF, HPF, BPF, phaser, flanger, delay (tempo-synced), reverb, slicer, tremolo, auto pan, lo-fi, overdrive
- **Rhythm**: built-in drum machine with metronome and 11 patterns, always in sync
- **Start/stop modes**: immediate, fade in/fade out, stop at loop end; dub mode overdub or replace
- **Full MIDI control**: all 136 functions are host parameters (Live's MIDI map) and also have their own MIDI learn via right-click
- **Follow transport**: loops start and stop with Live
- **Saved with your project**: loops are stored losslessly (FLAC, 24 bit) inside the Live set
- **Import/export**: export loops as WAV, drag & drop audio files onto a track
- **English and German user interface**: switch under *SETUP → Language / Sprache* (automatic follows your system language)

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
