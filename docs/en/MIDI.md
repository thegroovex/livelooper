# MIDI control

**English** | [Deutsch](../de/MIDI.md)

Every function of livelooper can be controlled freely via MIDI – every button, every knob, every selector, every effect slot. There are two ways, and you can combine them as you like.

![MIDI Learn](../images/livelooper-midi-learn.png)

## Way 1: MIDI learn inside the plug-in

Preparation in Live (once):

1. Create a MIDI track.
2. *MIDI From*: your controller / foot switch, *Monitor*: **In**.
3. *MIDI To*: the audio track with livelooper, below that choose **livelooper**.

Assigning:

- **Right-click** a control → **MIDI Learn** → press a key/pad/foot switch or move a knob. Done.
- Or switch on **MIDI LEARN** in the header: all assignable controls get a purple outline (already assigned ones show a dot). Click a control, move the controller, click the next control … click **MIDI LEARN** again to finish.
- Removing: right-click → **Remove MIDI mapping**, or click the entry in the **MIDI MAP** menu.

The controls in the track **SETUP** (Input, Start, Stop, Dub mode, Measures, Loop Sync, Clear) can be assigned by right-click as well.

Assignments are saved with the Live set.

### How MIDI messages act

| Kind of function | Examples | Note | CC |
|---|---|---|---|
| Button | REC/PLAY, STOP, CLEAR, UNDO, REDO, ALL START/STOP, CLEAR ALL, TAP | note-on triggers | value rising above 63 → triggers (releasing does nothing) |
| Switch | REV, 1SHOT, FX, Loop Sync, effect A–D on/off, RHYTHM, HOST SYNC | note-on toggles | rising edge toggles |
| Selector | effect type, Quantize, pattern, start/stop/dub mode | note-on → next value | CC selects directly (0–127 across all values) |
| Continuous | level, pan, effect parameters, tempo, fade | velocity sets the value | CC sets the value |

This works with foot switches that send 127 on press and 0 on release as well as with pads that send notes.

### Default mapping

**MIDI MAP → Load default map** loads a ready-made mapping on MIDI channel 1:

| Note / CC | Function |
|---|---|
| C1–E1 (36–40) | REC/PLAY track 1–5 |
| F1–A1 (41–45) | STOP track 1–5 |
| A#1 (46) | ALL START/STOP |
| B1 (47) | TAP |
| C2–E2 (48–52) | UNDO track 1–5 |
| C3–D#3 (60–63) | Input FX A–D on/off |
| E3–G3 (64–67) | Track FX A–D on/off |
| CC 20–24 | Level track 1–5 |
| CC 25 | Master |

## Way 2: Live's MIDI map

All functions are visible and automatable as plug-in parameters.

1. Click **Configure** in livelooper's device title bar and click or move the controls you want in the plug-in window – they appear as controls in the device. (With fewer than 128 parameters Live also shows them directly.)
2. Switch on **MIDI** (Cmd+M / Ctrl+M), click a parameter, move your controller.

Button parameters (e.g. "T1 Rec/Play") jump back to "off" by themselves after triggering. That way every press triggers exactly once – whether Live maps the button as toggle or momentary.

Automation in Arrangement view works too, e.g. to start a recording exactly at a bar line.

## Parameter list

247 parameters, names as shown in Live with the English interface (with the German interface they are localised, e.g. "S1 Pegel" instead of "T1 Level"; the internal IDs and your saved assignments stay the same):

**Per track (T1 … T5)** – 16 each:
`Rec/Play`, `Stop`, `Clear`, `Undo`, `Redo` (buttons) · `Level`, `Pan` · `Reverse`, `One Shot`, `Loop Sync`, `Track FX` (switches) · `Input`, `Start Mode`, `Stop Mode`, `Dub Mode`, `Measures` (selectors)

**Global** – 16:
`All Start/Stop`, `All Start`, `All Stop`, `Clear All`, `Tap Tempo` (buttons) · `Master Level`, `Input Monitor`, `Internal Tempo`, `Fade Time` · `Quantize`, `Rec Action`, `Rhythm Pattern`, `Metronome Sound` (selectors) · `Sync to Host`, `Follow Transport`, `Rhythm On` (switches) · `Rhythm Level`

**Effects** – 5 per slot (`In FX A … D`, `Trk FX A … D`):
`On`, `Type`, `Param 1`, `Param 2`, `Mix`

The internal parameter IDs (for developers and saved assignments) are e.g. `t1_recplay`, `t3_level`, `allstartstop`, `inB_type`, `trD_mix`.

Tip: the host identifies parameters by their internal ID, not by their name, so switching the language does not break existing MIDI maps or automation.
