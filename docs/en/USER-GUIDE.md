# User Guide

**English** | [Deutsch](../de/BEDIENUNGSANLEITUNG.md)

![Interface](../images/livelooper.png)

The interface is laid out like a hardware looper:

| Area | Contents |
|---|---|
| Header | display (tempo, bar/beat, quantization, messages), **MIDI LEARN**, **MIDI MAP**, **SETUP** |
| INPUT FX (left) | four effect slots A–D for the input signal |
| FX EDIT (centre) | effect type and knobs of the selected slot |
| TRACK FX (right) | four effect slots A–D for the tracks with **FX** switched on |
| TRACK 1–5 (or 1–8) | the loop tracks (SETUP → Tracks) |
| RHYTHM | drum machine, tempo, tap |
| MASTER | quantization, host sync, levels, fade time, ALL START/STOP, CAPTURE, CLEAR ALL |
| Footer | 8 scene buttons, STORE |

Drag the bottom right corner to resize the window; the interface scales with it. Hover over any control to see a tooltip.

---

## 1. A track

![Track setup](../images/livelooper-track-setup.png)

### REC/PLAY pad (the big round button)

| Track state | Pressing REC/PLAY |
|---|---|
| empty | **recording** starts (quantized: on the next bar/beat) |
| recording | recording ends → **overdub** (default "Rec > Dub") or **play** ("Rec > Play") |
| overdub | → **play** |
| play | → **overdub** |
| stopped | → **play** (quantized) |
| waiting (blinking) | pressing again cancels the wait |

The LED ring shows the state and the position in the loop:

- **red** recording · **orange** overdub · **green** play · **dim green** stopped, loop present · **off** empty
- **blinking**: waiting for the next bar (recording, start or end of recording)
- the bright segments run around the ring and show the playback position

### STOP

- short press: the track stops (depending on the **Stop** mode immediately, with fade out or at the loop end)
- press while recording: the recording ends and the track stops
- **hold for 1.5 s: clear the track** (the red bar shows the progress)

### Other track controls

| Control | Function |
|---|---|
| Display | state, length in bars, level, progress bar |
| **REV** | play the loop backwards (also while overdubbing) |
| **1SHOT** | play the loop once, then stop |
| **FX** | send the track through the track FX |
| Fader | level 0–200 % (double-click = 100 %), level meter next to it |
| **PAN** | panorama L50–R50 (double-click = centre) |
| **UNDO / REDO** | take back / restore the last overdub. Undo right after the first recording empties the track, redo brings it back |
| **SETUP** | track settings (picture above) |

### Track settings (SETUP)

| Setting | Values | Meaning |
|---|---|---|
| Input | Stereo · Left · Right | which input channel is recorded (Left/Right = mono on both sides) |
| Start | Immediate · Fade In | playback starts immediately or fades in (fade time in MASTER) |
| Stop | Immediate · Fade Out · Loop End | stop immediately, fade out, or stop at the end of the loop |
| Dub mode | Overdub · Replace | add to or replace the loop while overdubbing |
| Measures | Auto · 1 · 2 · 4 · 8 · 16 | Auto = length set by the second press; a fixed number = recording ends automatically after that many bars |
| LOOP SYNC | on/off | the loop stays locked to the bar grid: after jumping in Live or restarting it continues at the right spot |
| CLEAR | | clear the track |
| EXPORT WAV | | save the loop as a 24-bit WAV |
| IMPORT | | load an audio file (WAV, AIFF, FLAC, OGG, MP3) into the track. Faster: drag & drop the file onto the track |

---

## 2. Quantization, tempo and sync

**QUANTIZE** (MASTER):

- **Measure** (default): start of recording, end of recording and start of playback happen on the next bar line. Loops are therefore always whole bars long.
- **Beat**: the same on quarter-note beats.
- **Off**: everything immediately, free loop length (sample-accurate between the two presses).

**Retroactive ending**: if you press to end a recording up to 15 % of a grid step *after* the bar line, the loop still ends exactly on the bar line – whatever you played after the bar line is moved to the start of the loop (into the overdub). If you press too early, the looper simply waits for the bar line.

**HOST SYNC** (on by default): tempo, time signature and position come from Live. When Live's transport is stopped, an internal clock continues at the current Live tempo as soon as a track records or plays. Without host sync the internal tempo applies (knob **TEMPO**, tap **TAP** several times in time, or click the tempo on the display and type it in – see section 11).

**Follow host transport** (SETUP menu): loops start with Live's play button and stop with Live's stop button.

Note: if the tempo changes after a loop was recorded, the loop is not time-stretched – it keeps playing at its original speed.

---

## 3. Effects

There are two effect banks with four slots each:

- **INPUT FX** process the input signal – what you hear is also what gets recorded.
- **TRACK FX** process all tracks that have **FX** switched on (a shared effect bus), without changing the recording.

The **A/B/C/D** button switches the slot on and off and selects it for editing at the same time; clicking the effect name below it only selects it. In the **FX EDIT** area you set the type, two parameters and the mix. The slots of a bank are connected in series (A → B → C → D).

| Effect | P1 | P2 | Mix |
|---|---|---|---|
| LPF / HPF / BPF | cutoff | resonance | dry/wet |
| Phaser | depth | rate (0.05–5 Hz) | dry/wet |
| Flanger | feedback | rate | dry/wet |
| Delay | feedback | time: 1/16 · 1/8 · 1/8. · 1/4 · 1/4. · 1/2 (tempo-synced) | echo level |
| Reverb | room size | damping | reverb level |
| Slicer | depth | pattern (8 rhythms in 16ths) | dry/wet |
| Tremolo | depth | rate: 1 bar … 1/32 (tempo-synced) | dry/wet |
| Auto Pan | depth | rate (tempo-synced) | dry/wet |
| Lo-Fi | bit depth (16 → 3 bit) | sample rate reduction | dry/wet |
| Overdrive | drive | tone | dry/wet |

Switching on and off is crossfaded without clicks.

---

## 4. Rhythm

**RHYTHM** switches the drum machine on; choose the pattern next to it: Metronome, Rock, Pop, Funk, Hip Hop, House, Techno, Breakbeat, Reggae, Bossa, Shuffle, Half Time. **LEVEL** sets the volume. The rhythm is locked to the bar grid (host or internal clock) and is not recorded. The metronome accents the downbeat.

---

## 5. MASTER

| Control | Function |
|---|---|
| QUANTIZE | Off · Beat · Measure |
| HOST SYNC | follow Live's tempo/transport |
| MASTER | overall level 0–200 % |
| INPUT | level of the input passed straight through, 0–200 % (recording is independent of it) |
| FADE | fade time for fade in/fade out (10 ms – 8 s) |
| ALL START / STOP | if any track is playing: stop all; otherwise start all tracks that have a loop (quantized) |
| CLEAR ALL | clear all tracks (asks first) |

---

## 6. SETUP menu

| Entry | Meaning |
|---|---|
| Record action | "Rec > Dub > Play" (default) or "Rec > Play > Dub". The REC button legend ("DUB / PLAY" or "PLAY / DUB") and the bottom display line show the active mode. |
| Follow host transport | loops start/stop with Live |
| Maximum loop length | 30 · 60 · 120 · 240 · 480 seconds per track (determines memory use; existing loops are kept) |
| Save loops with the project | store loops in the Live set (on by default). Off = smaller sets, loops are gone after loading |
| Computer keyboard control | standalone app only: keys on/off (section 10) |
| Keyboard mapping … | standalone app only: own key mapping (section 10) |
| Type in tempo (BPM) … | type the internal tempo (section 11) |
| Language / Sprache | Automatic (system language) · English · Deutsch |

---

## 7. Language

The whole interface – buttons, displays, menus, tooltips, dialogs and parameter names – is available in English and German.

- **SETUP → Language / Sprache → Automatic** (default) uses German if your operating system is set to German, otherwise English.
- **English** or **Deutsch** fixes the language. The choice is saved on this computer and applies to all livelooper instances, in Live and in the standalone app.
- The interface switches immediately. The parameter names Live shows in the device and in its MIDI map are read when the plug-in is loaded, so they switch after reloading the plug-in (e.g. reopening the set). Existing MIDI maps and automation are not affected.

![German interface](../images/livelooper-de.png)

---

## 8. Saving and loading

The Live set stores all parameters, the MIDI assignments and – if switched on – the loops (lossless FLAC, 24 bit). After loading, the tracks are stopped and start with REC/PLAY or ALL START. If the project's sample rate changes, the loops are converted automatically.

## 9. Display

- big number: current tempo – click it to type in the BPM
- line 1: tempo source (HOST, internal clock)
- line 2: bar . beat and quantization
- line 3: messages (export, import, MIDI learn) or the memory per track
- dot at top right: lights up yellow on incoming MIDI

---

## 10. Computer keyboard (standalone app)

In the standalone app the computer keyboard plays livelooper like a hardware unit – handy on a MacBook without a controller. In Live the keyboard belongs to Live, so this is a standalone feature (MIDI and Live's own key mapping work there instead).

Default mapping – one keyboard column per track:

| Key | Function |
|---|---|
| **1 2 3 4 5** (6 7 8) | REC/PLAY of track 1–5 (6–8) |
| **Q W E R T** (Y U I) | STOP track 1–5 (6–8) |
| **Shift + Q … T** | CLEAR track 1–5 – hold the key for 1.5 s, like the STOP button (a short tap does nothing) |
| **A S D F G** (H J L) | UNDO track 1–5 (6–8) |
| **Shift + A … G** | REDO track 1–5 |
| **Space** | ALL START / STOP |
| **Shift + Backspace** | CLEAR ALL (asks first) |
| **Return** | TAP tempo |
| **B** | type in the tempo (BPM) |
| **Arrow Up / Down** | tempo +1 / −1 BPM (Shift: ±0.1) |
| **M** | RHYTHM on/off |
| **N** | next rhythm pattern |
| **K** | next quantize setting |

The display briefly shows which key did what (e.g. `KEY Q > T1 STOP`). Holding a key does not repeat it – except tempo up/down.

**Own key mapping:** *SETUP → Keyboard mapping …* lists every function (all track functions incl. reverse, one shot, loop sync and FX, all global functions, FX slots A–D, tempo). Click a function and press its new key – modifiers (Shift, Ctrl, Option, Cmd) count. A key that was already used moves to the new function. Esc cancels, right-click removes a key, **DEFAULTS** restores the table above, **REMOVE ALL** empties it, **KEYBOARD CONTROL** switches keyboard control on/off (also *SETUP → Computer keyboard control*). The mapping is saved together with the app's state.

![Keyboard mapping](../images/livelooper-keyboard.png)

## 11. Typing in the tempo

Click the big tempo number on the display (or press **B**, or right-click the TEMPO knob → *Type in tempo (BPM) …*, or *SETUP → Type in tempo (BPM) …*), type the BPM and press Return. Decimals work with a point or a comma (`97.5`, `97,5`); the range is 40–300 BPM, Esc cancels. The value sets the internal tempo – while HOST SYNC follows a running host, the host tempo stays in charge.

![Typing in the tempo](../images/livelooper-tempo-entry.png)

## 12. Meters, waveform and clip LED

- **Track meter** (stereo, next to the level fader): while a track records or overdubs it shows the **input** level (orange, caption **IN**), otherwise the track's **output** level (green, **OUT**). Yellow from -12 dB, red from -1 dB; the thin line holds the peak for about a second.
- **Master meter**: the top row of the display (**MASTER**) shows the level of the main output, left and right.
- **Waveform**: the track display shows an overview of the loop instead of a plain progress bar. The part already played is bright, a white line marks the play position. While recording, the waveform grows from left to right; overdubs and undo show up right away. Reversed tracks show the waveform mirrored.
- **Clip LED** (top right of every track): off = empty, green = the track holds a loop, red = first recording, blinking red = waiting for the next bar to start recording.

## 13. Looper sets

A set stores the whole looper in one file (`.llset`): all loops, levels, pan, track settings, effects, rhythm, tempo and quantize settings.

- **SETUP → Save set ...** writes a set (a set always contains its loops, even if *Save loops with the project* is off).
- **SETUP → Load set ...** loads one; you can also drop a `.llset` file onto the window.
- Your MIDI mapping, keyboard mapping and cue routing belong to the app and stay as they are when you load a set.

## 14. Cue output (standalone app)

The standalone app has a second output bus for the **rhythm and metronome**, so you can hear the click on headphones without it reaching the audience.

1. Open **Options → Audio/MIDI Settings** and enable the extra outputs of your audio interface (outputs 3 and up).
2. **SETUP → Cue output (click / rhythm)**: choose the output pair (e.g. *Outputs 3 / 4*) and where the rhythm plays: **Main output**, **Cue output only** or **Main and cue output**.

Without a cue pair (or in a plug-in) the rhythm always plays on the main output, so the click is never lost. The setting is saved with the app.

## 15. Metronome sound

**SETUP → Metronome sound** switches between **Beep** (classic) and **Woodblock**. The drum patterns use synthesised kick, snare, hand clap and metallic hi-hats.

## 16. Capture (retrospective recording)

You played something great without pressing record? Press **CAPTURE** (MASTER panel). The last bars you played become a loop on the first empty track and play on at once, in time. The length is set in **SETUP → Capture length** (1, 2, 4 or 8 bars). If less was played, livelooper takes the largest length that fits. Every track's SETUP also has a **CAPTURE** button for that track. **UNDO** removes a captured loop.

## 17. Auto record

**SETUP → Auto record**: armed tracks (REC pressed, display **AUTO REC**) wait until the input passes the threshold (-50 to -12 dB) and start recording with the first note. Hands stay on the instrument. Pressing REC again cancels.

## 18. Track settings: feedback, ducking, pitch, follow tempo, output

![Track setup](../images/livelooper-track-setup.png)

| Setting | Meaning |
|---|---|
| **FEEDBACK** | how much of the loop remains with every overdub pass. 100 % = all (classic overdub); lower values let older layers fade away like tape echo |
| **DUCKING** | lowers the track while the ducking source plays (**SETUP → Ducking source**: the rhythm's kick or track 1). Gives pumping, sidechain-style grooves |
| **PITCH** | -12 to +12 semitones, the length stays the same |
| **FOLLOW TEMPO** | the loop follows tempo changes (host or internal) without changing pitch (time-stretch) |
| **OUTPUT** | Main · Direct · Main + Direct: where the track is heard (section 20) |
| **Input → Resample** | the track records the mix of the other tracks (bounce): combine loops on one track and free the others |

## 19. Scenes

The footer has **8 scene buttons**. Pressing an empty scene stores which tracks are playing. Pressing a stored scene starts its tracks and stops the others, quantized to the next bar or beat. **STORE** + scene button overwrites a scene. A dim LED shows that a scene is stored, a bright one shows the last scene you called up. Scenes are saved with the project and in sets, and they are MIDI-mappable.

## 20. Direct track outputs

- **Plug-in**: livelooper has an optional stereo output per track ("Track 1" … "Track 8"). Enable it in your host, for example in Ableton Live through a second audio track with *Audio From: livelooper → Track 1*.
- **Standalone app**: **SETUP → Direct track outputs** assigns each track an output pair of the audio interface (outputs 3/4 and up, not the cue pair).
- In the track SETUP, **OUTPUT** decides between *Main*, *Direct* (only the direct output) and *Main + Direct*. Without an active direct output the track always plays on the main output.

## 21. 8 tracks, ghost play head, stem export

- **SETUP → Tracks → 8 tracks** shows three more tracks and makes the window wider. Hidden tracks use no memory. Switching back to 5 tracks clears tracks 6 to 8.
- **Ghost play head**: a stopped track shows a dotted line where it would be playing now, so you can bring it back in on cue.
- **SETUP → Export stems** writes every track with a loop as a 24-bit WAV (`livelooper-track1-120bpm.wav`) with tempo and beat metadata, so Ableton Live and other DAWs drop it straight onto the grid.

![8 tracks](../images/livelooper-8-tracks.png)
