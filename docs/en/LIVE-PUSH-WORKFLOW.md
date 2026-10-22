# Live setup: Ableton Live + Push 1 with several instruments

**English** | [Deutsch](../de/LIVE-PUSH-WORKFLOW.md)

A step-by-step guide for a typical live setup: **keyboards, vocals, guitar and drums** all run into livelooper, an **Ableton Push 1** controls the loops, and a **second MIDI keyboard** plays a software synth on its own track. The basics are in [DAW-SETUP.md](DAW-SETUP.md), the full Push mapping in [PUSH.md](PUSH.md).

**The idea:** all sources run into one group, livelooper sits on that group, Push controls the loops, the second keyboard only plays the synth.

```
Vocals ─┐
Guitar ─┤
Keys   ─┼──►  group "Looper" with livelooper  ──►  Master
Drums  ─┘
Push 1 ──► livelooper (user port)      MIDI keyboard ──► synth on "Keys"
```

## 1. Preparation (once)

1. **Audio settings:** buffer at **64–128 samples**. Raise it only if you hear clicks.
2. Leave **Push** set up as usual as the "Push" control surface in Live's MIDI settings.
3. **Windows only:** turn input and output **MIDIIN2 / MIDIOUT2 (Ableton Push)** fully off (Track, Sync, Remote). Otherwise livelooper cannot open Push.
4. **Second MIDI keyboard:** in the MIDI settings, set **Track = on** for its input.
5. **Headphones** for vocals, otherwise every overdub records the speakers too.

## 2. Create the tracks

| Track | Type | Input | Monitor |
|---|---|---|---|
| **Vocals** | Audio | Ext. In → mic input (mono) | **In** |
| **Guitar** | Audio | Ext. In → guitar input (mono) | **In** |
| **Keys** | MIDI + your software synth | **only the second keyboard**, all channels | **In** |
| **Drums** | Drum Rack / drum plug-in (clips or pads) or drum mics | depends on the source | **In** for mics |

**Tip:** set the **Keys** input to the second keyboard and Monitor to **In**. The synth then always plays, whichever track is selected, and Push stays completely free for the looper.

## 3. Send everything into the looper

1. Select all four tracks and group them (**Cmd/Ctrl+G**); name the group e.g. "Looper".
2. Drag **livelooper onto the group track**. Everything in the group now runs through the looper.
3. The **group fader** is the level that goes into the loops. The individual faders are your mix of the sources.
4. **Listen** either through livelooper (**INPUT** knob at 100 %) **or** through your interface's direct monitoring (INPUT at 0). Never both, or it sounds doubled.

## 4. Switch Push to livelooper

1. Press **USER**. Push switches to livelooper, **SETUP → Ableton Push** shows "Connected".
2. Press **USER** again to go back to Live (e.g. to play the Drum Rack on the pads or launch clips).
3. Optional: **SETUP → Tracks → 8 tracks** if 5 loop tracks are not enough.

## 5. Measure latency once

**SETUP → Audio → Latency → Measure latency …**: a cable from an interface output into an input that ends up in the group. Play nothing else. Recordings and overdubs then land on the beat ([user guide, section 41](USER-GUIDE.md#41-latency-measurement-and-compensation-new-in-17)).

## 6. Transport and tempo

- Set the tempo in Live and **start Live (space bar)**. *Follow host transport* is on: when Live stops, the loops stop too.
- Want to jam while Live is stopped? Turn **SETUP → Looper → Follow host transport** off.

## 7. Playing with Push (LOOP mode, SESSION button)

| Push | Function |
|---|---|
| Bottom pad row | **REC / PLAY / DUB** per track (column = loop track) |
| Second pad row | **STOP** |
| Pads 3–8 | loop position: tap jumps, hold stutters |
| Hold **FIXED LENGTH** + pad | track loop length: AUTO, 1, 2, 4, 8, 16 bars |
| Hold REC pad + position pad | length for this recording only |
| **MUTE / SOLO** + pad | mute or solo a track |
| **PLAY** | start / stop all |
| **RECORD** | capture the last bars as a loop |
| **UNDO** / SHIFT + UNDO | undo / redo |
| Scene buttons | scenes 1–8, SHIFT = store |
| Encoders | level, pan, pitch, FX |
| Touch strip | tape stop, SHIFT = filter |
| Footswitch 2 | REC / PLAY / DUB of the selected track, double tap = STOP, hold 1 s = clear |
| Footswitch 1 | next track |

## 8. Important: what ends up in which loop?

livelooper records **whatever is sounding in the group right now**. So you choose the source by playing:

- For a guitar loop, play only guitar; for a synth loop, only the keyboard.
- Stop drum clips or mute the drum track first, otherwise they end up in the next loop.
- **Handy:** map the track activators (mute) of Vocals, Guitar, Keys and Drums to buttons or pads of the second keyboard with **Cmd/Ctrl+M** (Live's MIDI mapping). You can then switch sources without leaving Push.
- **SETUP → Auto record:** arm a track and recording starts with the first note. Your hands stay on the guitar or keys.

## 9. Example song flow

1. Start Live, hold FIXED LENGTH, set track 1 to **4 bars**.
2. Track 1: record drums (let a clip run or play them in), then mute the drum source.
3. Track 2: guitar riff.
4. Track 3: bass or pad with the synth on the second keyboard.
5. Track 4: vocals; track 5: harmony or overdub.
6. Arrange with MUTE / SOLO, scenes and the touch strip; use the footswitches for hands-free recording.

## 10. Saving

- The Live project saves the whole looper including audio.
- **Auto backup** runs in the background. After a crash, livelooper offers to restore the next time it opens ([user guide, section 46](USER-GUIDE.md#46-auto-backup-and-crash-recovery-new-in-17)).
