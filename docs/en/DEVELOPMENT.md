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

Ableton Link is a git submodule – clone with submodules (or fetch them later):

```bash
git clone --recursive https://github.com/thegroovex/livelooper.git
# existing clone: git submodule update --init --recursive
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
| `unit_tests` (`livelooper_tests`) | Engine: free and quantized recording (bar, beat), retroactive ending, fixed bar count, cancelling, large host blocks, input routing, overdub, replace, undo/redo (including the first recording), clear, reverse, one shot, stop modes, level/pan, all start/stop, track FX routing, loop sync after transport jumps, quantized start in phase, follow transport, internal clock, rhythm. Effects: all 12 types stable, bypass bit-exact, filter response, delay time, slicer grid. MIDI: learn, button/switch/selector/continuous, CC edges, uniqueness, saving. Processor: 137 unique MIDI-capable parameters, button parameters, MIDI via processBlock, learn, project save including loop audio, sample rate change, WAV export/import, editor. Features 1.2: cue routing (main / cue / both, no cue → main), both metronome sounds, all drum patterns in range, track meter input/output, master meter, waveform incl. overdub/undo, standalone cue bus channel pairs, sets save/load keeping mappings, STOP button release. Localisation: English/German texts, every choice/effect has a translation, parameter names, live switching. Features 1.4: loop cutting on the 16th grid (crossfaded, tap stays, hold stutters, back in sync), mute/solo, multiply/halve with undo, copy, rec length, tape stop, performance filter, slice parameters as MIDI momentary; Push: cutting gestures, REC + position pad length with progress bar, footswitches, touch strip, pad pressure, DOUBLE/OCTAVE/DUPLICATE, MUTE/SOLO hold, level meters with display glyphs (rate-limited). Features 1.5: beat FX (all 16 types stable, beat sync, tails, channel routing), colour FX (centre bit-exact), beat repeat / stutter on the grid and back in sync, metronome block, sample kits / grooves / fills, capture 16 / 32 bars, master VU, help parser and search; Push 1.5: beat FX buttons and BROWSE encoders, REPEAT / ACCENT momentary + latch, touch strip rate 1/2 … 1/32, FIXED LENGTH capture length, SHIFT + METRONOME Link, SHIFT + TAP metronome; Link enable / disable (no network needed) and Link driving the timeline, app settings persistence in a temporary folder (start mode, last settings, recent sets, update options), `compareVersions` (pre-releases, v prefix) |
| `vst3_host_test` (`livelooper_hosttest`) | loads the built `livelooper.vst3` through JUCE's VST3 hosting like a DAW: name, effect category, MIDI input, parameter count, recording and playback via parameters, stop, saving/loading state in a second instance, editor |

Single group: `livelooper_tests engine`, or `midi`, `plugin`.

The CI also runs [pluginval](https://github.com/Tracktion/pluginval) at strictness 5 on Linux, macOS and Windows.

Documentation screenshots: `livelooper_screenshot docs/images` (on Linux with `xvfb-run -a`). It writes every picture in English (`livelooper*.png`) and German (`livelooper*-de.png`): main view, live performance (beat FX, colour FX, repeat latch, metronome and Link on), MIDI learn, tempo entry, keyboard mapping, track setup, 8 tracks, help and about window.

## Architecture

```
src/
  engine/                 UI-free DSP core (juce_audio_basics only)
    Types.h               settings, enums, TimeInfo
    LoopTrack.*           one track: recording, overdub, playback, undo/redo, fades
    LooperEngine.*        5 tracks, timeline/quantization, commands, FX buses, rhythm, status
    Effects.*             FxUnit (12 effects) and FxChain (4 slots)
    Performance.*         tape stop and performance filter on the loop bus (1.4)
    RhythmGenerator.*     drum machine: 21 patterns, sample kits (1.5), grooves and fills
    DrumSamples.inc       sample slots of the built-in kits
    BeatFx.*              beat FX (16 types), colour FX, beat repeat / stutter (1.5)
    MidiMap.*             MIDI learn table (lock-free)
  plugin/
    Localisation.*        English/German texts, language setting
    Parameters.*          all 400 parameters (IDs append-only, ranges, groups, English and localised names)
    LinkSync.*            Ableton Link: tempo / beat / start-stop, latency-compensated (1.5)
    AppSettings.*         settings of the user / computer: start mode, last settings, recent sets, updates (1.5)
    Updates.*, InfoWindows.*  GitHub release check, download + verify, help window, about window (1.5)
    DrumSampleLoader.*    decodes the embedded FLAC drum samples
    push/                 Ableton Push (1) over its User port: protocol, device, surface
    PluginProcessor.*     AudioProcessor: parameters → engine, buttons, MIDI, state, import/export
    PluginEditor.*        interface, MIDI learn mode, menus, drag & drop
    Widgets.*             REC/PLAY pad, STOP pad, display, FX slot, track, track setup
    LooperLookAndFeel.*   hardware look
  standalone/
    StandaloneApp.cpp     custom standalone window (Audio/MIDI Settings button, start settings)
    MacMicPermission.mm   asks for the microphone permission once (macOS)
tests/                    unit tests and VST3 host test
installer/macos/          .pkg: distribution.xml, welcome / readme (EN / DE), postinstall, install.command
installer/windows/        livelooper.iss (Inno Setup: app + VST3, EN / DE, AGPLv3 page, uninstaller)
resources/macos/          livelooper.entitlements (audio input)
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
- checkout with submodules (recursive) for Ableton Link
- tag `v*` or manual start: additionally macOS (universal, VST3 + AU + app) and Windows (VST3 + app), tests, pluginval, and:
  - **macOS**: ad-hoc code signature with a stable identifier (`codesign --sign - --identifier com.groovex.livelooper --entitlements resources/macos/livelooper.entitlements`, hardened runtime off), ZIP (with `install.command`, made with `ditto`) and **`livelooper-macOS.pkg`** (`pkgbuild` per component + `productbuild` with `installer/macos/distribution.xml`)
  - **Windows**: ZIP and **`livelooper-Windows-Setup.exe`** (Inno Setup, installed with Chocolatey when `iscc` is missing)
- tag `v*`: GitHub release with the ZIPs (still the primary / manual way) plus `livelooper-macOS.pkg` and `livelooper-Windows-Setup.exe`

Signing: there is no Apple Developer ID / notarisation and no Windows certificate yet, so Gatekeeper and SmartScreen warn once (INSTALLATION.md). With a Developer ID, replace the ad-hoc `codesign` step (hardened runtime + entitlements), sign the pkg with `productsign` and notarise with `notarytool`.

Creating a release:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## Known limitations

- Without *Follow tempo* (track SETUP) tempo changes after recording do not change the loop speed.
- One undo step per track (the last overdub pass).
- When the sample rate or the maximum loop length changes, loops are converted; recordings in progress are lost.
- The builds are only ad-hoc signed (macOS) / unsigned (Windows): Gatekeeper and SmartScreen warn once (see INSTALLATION.md).
- Parameter names in the host change language only after reloading the plug-in.
