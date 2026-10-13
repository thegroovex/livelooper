# Development

**English** | [Deutsch](../de/ENTWICKLUNG.md)

## Requirements

| Platform | Tools |
|---|---|
| macOS | Xcode 14+ (command line tools), CMake ≥ 3.22 |
| Windows | Visual Studio 2022 (C++ desktop), CMake ≥ 3.22 |
| Linux (development/tests) | GCC 11+ or Clang 14+, CMake, Ninja, `libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev`, `xvfb` for tests without a display |

JUCE 8.0.15 is downloaded by CMake automatically. Use a local JUCE copy with `-DLIVELOOPER_JUCE_PATH=/path/to/JUCE`.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Results in `build/livelooper_artefacts/Release/`:

- `VST3/livelooper.vst3` (macOS, Windows, Linux)
- `AU/livelooper.component` (macOS only)
- `Standalone/livelooper(.app/.exe)`

With `-DLIVELOOPER_COPY_PLUGIN=ON` the build copies the plug-ins straight into the system folders. Universal binary on the Mac: `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

## Tests

```bash
ctest --test-dir build -C Release --output-on-failure      # Linux without a display: xvfb-run -a ctest ...
```

| Test | What is checked |
|---|---|
| `unit_tests` (`livelooper_tests`) | Engine: free and quantized recording (bar, beat), retroactive ending, fixed bar count, cancelling, large host blocks, input routing, overdub, replace, undo/redo (including the first recording), clear, reverse, one shot, stop modes, level/pan, all start/stop, track FX routing, loop sync after transport jumps, quantized start in phase, follow transport, internal clock, rhythm. Effects: all 12 types stable, bypass bit-exact, filter response, delay time, slicer grid. MIDI: learn, button/switch/selector/continuous, CC edges, uniqueness, saving. Processor: 137 unique MIDI-capable parameters, button parameters, MIDI via processBlock, learn, project save including loop audio, sample rate change, WAV export/import, editor. Features 1.2: cue routing (main / cue / both, no cue → main), both metronome sounds, all drum patterns in range, track meter input/output, master meter, waveform incl. overdub/undo, standalone cue bus channel pairs, sets save/load keeping mappings, STOP button release. Localisation: English/German texts, every choice/effect has a translation, parameter names, live switching |
| `vst3_host_test` (`livelooper_hosttest`) | loads the built `livelooper.vst3` through JUCE's VST3 hosting like a DAW: name, effect category, MIDI input, parameter count, recording and playback via parameters, stop, saving/loading state in a second instance, editor |

Single group: `livelooper_tests engine`, or `midi`, `plugin`.

The CI also runs [pluginval](https://github.com/Tracktion/pluginval) at strictness 5 on Linux, macOS and Windows.

Documentation screenshots: `livelooper_screenshot docs/images` (on Linux with `xvfb-run -a`). It writes every picture in English (`livelooper*.png`) and German (`livelooper*-de.png`).

## Architecture

```
src/
  engine/                 UI-free DSP core (juce_audio_basics only)
    Types.h               settings, enums, TimeInfo
    LoopTrack.*           one track: recording, overdub, playback, undo/redo, fades
    LooperEngine.*        5 tracks, timeline/quantization, commands, FX buses, rhythm, status
    Effects.*             FxUnit (12 effects) and FxChain (4 slots)
    RhythmGenerator.*     synthetic drum machine, 12 patterns
    MidiMap.*             MIDI learn table (lock-free)
  plugin/
    Localisation.*        English/German texts, language setting
    Parameters.*          all 247 parameters (IDs, ranges, groups, English and localised names)
    PluginProcessor.*     AudioProcessor: parameters → engine, buttons, MIDI, state, import/export
    PluginEditor.*        interface, MIDI learn mode, menus, drag & drop
    Widgets.*             REC/PLAY pad, STOP pad, display, FX slot, track, track setup
    LooperLookAndFeel.*   hardware look
tests/                    unit tests and VST3 host test
tools/Screenshot.cpp      renders the interface as PNG
```

### Real-time rules

- The audio thread neither allocates nor blocks. Memory is reserved in `prepareToPlay`.
- **Undo without copying**: each track has two layers. The loop is split into chunks of 512 samples, and for each chunk it is noted which layer is valid. When an overdub touches a chunk for the first time, only that chunk is copied into the other layer. Undo/redo flips the assignment of the touched chunks – O(chunks), no big copies.
- Commands (buttons, MIDI) are queued and executed at the start of a block; quantized actions are triggered sample-accurately within the block.
- Import and loading write under a track lock; the audio thread only takes it with `tryEnter` (if it is busy, the track is silent for one block).
- Saving and export read without a lock, so playback does not drop out while saving.

### Timeline

The position in quarter notes (ppq) comes from the host while its transport is running. Otherwise the engine keeps an internal ppq count that starts at 0 with the first recording or continues seamlessly from the last host position. Each track remembers the ppq anchor of its recording; with loop sync the playback position is recalculated from it after transport jumps.

### Button parameters

Momentary functions are bool parameters. The processor reacts to the rising edge and resets the parameter to 0 via a timer on the message thread. That way every function can be automated and assigned with the host's MIDI mapping (e.g. Live's MIDI map).

### Translations

- All texts in the code are English. Everything the user sees goes through `i18n::tr()` (`src/plugin/Localisation.cpp`), which looks the English text up in the German table. Texts with numbers or names use placeholders: `tr ("TRACK %1", "3")` → "SPUR 3".
- Button texts and tooltips are written in English and translated when drawn (`LooperLookAndFeel`), combo boxes (`LLCombo::setEnglishItems`) and the editor switch live when the language changes.
- Parameter names are built from the parameter ID (`params::englishName`, `params::localisedName`) and fixed when the plug-in is created. IDs never change, so projects and MIDI maps keep working in both languages.
- The language setting is stored in the user settings file `livelooper.settings` (macOS `~/Library/Application Support/livelooper/`, Windows `%APPDATA%\livelooper\`, Linux `~/.config/livelooper/`). The environment variable `LIVELOOPER_LANGUAGE=en|de` overrides it (used by the host test).
- New text: write it in English in the code with `tr()`, add the German translation to the table in `Localisation.cpp`; the test "Every choice, effect and effect parameter has a German text" catches missing choices and effects.

## CI / releases

`.github/workflows/ci.yml`

- every push / pull request: Linux build, all tests, pluginval
- tag `v*` or manual start: additionally macOS (universal, VST3 + AU + app) and Windows (VST3 + app), tests, pluginval, ZIP artifacts
- tag `v*`: GitHub release with the ZIPs

Creating a release:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## Known limitations

- No time-stretching: tempo changes after recording do not change the loop speed.
- One shared stereo output (no separate outputs per track).
- One undo step per track (the last overdub pass).
- When the sample rate or the maximum loop length changes, loops are converted; recordings in progress are lost.
- The macOS builds are not signed/notarized (see INSTALLATION.md).
- Parameter names in the host change language only after reloading the plug-in.
