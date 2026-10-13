# Ableton Push control

livelooper talks **directly** to an Ableton Push (1) through its **User port**, both in the plug-in (VST3 / AU) and in the standalone app. Pads light up with each track's state and loop position, the display shows names, values and tempo.

Push 2 and Push 3 send the same pad and button messages, so the controls should work on them too (not tested yet). LED colours may differ there, and their display is not used.

## Setup

**In Ableton Live**
1. Leave Push set up as usual (control surface "Push" in Live's MIDI preferences).
2. Load livelooper. **SETUP → Ableton Push** shows "Connected".
3. Press **USER** on Push: Push switches to livelooper. Press **USER** again to return to Live.

**Windows:** a MIDI port can only be used by one program at a time. In Live's MIDI preferences, leave the input and output **"MIDIIN2 (Ableton Push)" / "MIDIOUT2 (Ableton Push)"** switched off (Track, Sync and Remote off). Otherwise livelooper cannot open the port. On macOS this is not needed.

**Standalone app:** connect Push and start livelooper. The app switches Push to User mode on its own.

Only one livelooper instance controls Push at a time. **SETUP → Ableton Push** switches the control on or off.

## Layout

### LOOP mode (SESSION button)

Each of the 8 pad columns is one track (with 5 tracks, columns 6 to 8 stay dark).

| Pad row | Function | LED |
|---|---|---|
| 1 (bottom) | REC / PLAY / DUB | red = recording, orange = overdub, green = playing, dim green = stopped, blinking = waiting for the bar |
| 2 | STOP | yellow = playing, dim = stopped |
| 3 – 8 | loop position (press: select the track) | the play head walks up the column; stopped tracks show the ghost play head |

| Button | Function |
|---|---|
| top row 1 – 8 | track FX on / off |
| second row 1 – 8 | reverse on / off |
| scene buttons (right) | scenes 1 – 8 (top = 1). **SHIFT** + scene = store |
| PLAY | all start / stop |
| STOP | stop all |
| RECORD | capture (the last bars become a loop) |
| METRONOME | rhythm on / off |
| TAP TEMPO | tap tempo |
| UNDO | undo of the selected track (**SHIFT** = redo) |
| DELETE + pad | clear the track |
| MUTE + pad | stop the track |
| SHIFT + row 1 / row 2 | undo / redo of that track |
| AUTOMATION | auto record on / off |
| NEW | store mode for scenes |
| QUANTIZE | next quantize value |

### FX PAD mode (NOTE button)

The 8 × 8 pads become an **XY pad** for one effect: X (left to right) = parameter 1, Y (bottom to top) = parameter 2. Touching switches the effect on. Releasing the pads **snaps back** to the previous settings. Hold **SHIFT** while releasing to keep the effect as it is. The top buttons 1 – 4 choose input FX A – D, buttons 5 – 8 choose track FX A – D. **SESSION** returns to LOOP mode.

### Encoders

| Mode button | Encoders 1 – 8 |
|---|---|
| VOLUME | level of tracks 1 – 8 |
| PAN & SEND | pan |
| TRACK | pitch (semitones) |
| CLIP | feedback |
| DEVICE | parameter 1 of input FX A – D (1 – 4) and track FX A – D (5 – 8). **SHIFT** = mix |

Master encoder = master level · tempo encoder = tempo (1 BPM, **SHIFT** 0.1 BPM) · swing encoder = rhythm level. **SHIFT** turns finely.

### Display

Line 1: what the encoders control · line 2: their values · line 3: track states (`>` = selected track) · line 4: tempo, last scene, auto record, XY effect.
