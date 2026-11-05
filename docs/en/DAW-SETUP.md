# Using livelooper in your DAW

**English** | [Deutsch](../de/DAW-EINRICHTUNG.md)

This chapter shows how to wire the livelooper plug-in (VST3 / AU) into your DAW so you can **play** it: loop a mic or a guitar, loop your software instruments, switch between sources while you perform, and get the loops back out. Installing the plug-in is covered in [INSTALLATION.md](INSTALLATION.md), the functions themselves in the [User Guide](USER-GUIDE.md).

Short version: **one audio track, livelooper on it, everything you want to loop goes into that track, and your hands go on an [Ableton Push](PUSH.md).**

---

## 1. The basic idea

livelooper is an **audio effect with MIDI input**. You put it on **one audio track** – in this guide we call it the **looper track**.

```
mic / guitar / instrument tracks  ──►  looper track input  ──►  livelooper  ──►  master
```

- **Everything you want to loop must arrive at the looper track's input.** livelooper can only record what reaches the track it sits on. It has **one stereo (or mono) input** and no side-chain input – so you bring other sources *to* the looper track, not to a side-chain.
- **livelooper's output replaces the track's signal**: you hear the loops plus – with the **INPUT** knob (MASTER section, *Input Monitor* as a host parameter) – the live input passed straight through.
- livelooper adds **no extra latency** of its own.
- An **Ableton Push** needs no routing at all: livelooper talks to it directly over its User port, in every DAW ([PUSH.md](PUSH.md)).

### Monitoring: which switch does what?

There are three places where you might hear your live input. Use them like this:

| Switch | What it does | Setting |
|---|---|---|
| **DAW input monitoring** on the looper track (*Monitor In*, monitor button …) | lets the live input **into the track and into livelooper** at all | **on** – otherwise livelooper gets nothing and records silence |
| **livelooper's INPUT knob** | how loud you hear the live input through livelooper (recording is not affected) | **100 %** if you listen through the computer; **0** if you already hear yourself another way |
| **Direct monitoring of your audio interface** | you hear the input without the computer, with zero latency | either this **or** the INPUT knob – never both |

Turning on DAW monitoring on the looper track does **not** double the signal: livelooper sits on that track, and what you hear from it is controlled by its INPUT knob. You get a doubled or "flamming" signal only if the same source reaches your ears twice – e.g. interface direct monitoring *plus* INPUT at 100 %, or an instrument track that plays to the master *and* into the looper track. Section 7 shows the fixes.

**Rule of thumb:** DAW monitoring on the looper track always on, and hear your input **either** through livelooper (INPUT up, small buffer) **or** through the interface's direct monitoring (INPUT at 0).

---

## 2. Looping a microphone or guitar

1. **Create an audio track** – this is your looper track.
2. **Set its input** to the interface input your mic or guitar is plugged into (mono is fine; for a stereo source pick the pair).
3. **Turn on input monitoring** for the track (DAW table below).
4. **Insert livelooper** on the track.
5. Press REC on track 1 (on Push, in the plug-in or via MIDI) and play.

| DAW | Track input | Monitoring |
|---|---|---|
| Ableton Live | *Audio From* = *Ext. In* → your input | *Monitor* = **In** |
| Bitwig Studio | input chooser = your audio input | monitor button = **On** (Auto only monitors while armed) |
| Cubase | *Input Routing* = your input bus | **Monitor** button on the track |
| Logic Pro | audio track input = your input; livelooper as **Audio Unit** in an *Audio Effect* slot | **Input Monitoring** button on the track |
| Reaper | right-click the record-arm button → input | **Record arm** + *Monitor input* (right-click menu) |
| Studio One | audio track input = your input | **Monitor** button on the track |

**Do you have to record-arm the track?** Not in Live (Monitor **In**) and not in Bitwig (monitor **On**) – the track does not record anything in the DAW, livelooper does the recording. In DAWs where monitoring only works on an armed track (Reaper, and depending on your settings Cubase, Logic or Studio One), arm it. That is harmless as long as you don't press the DAW's own record button.

**Tips**

- **Small buffer = tight playing.** Set the buffer size in your DAW's audio settings to 64–128 samples while performing; raise it only if you hear crackles.
- **Wear headphones** with a microphone. Speakers + open mic + loops = feedback, and every overdub records the speakers too.
- **One mic and one guitar at once?** Feed both into the looper track as a stereo pair (e.g. inputs 1/2), then in each livelooper track's SETUP choose **Input = Left** or **Right**: track 1 records only the guitar, track 2 only the mic – no switching in the DAW.
- livelooper's **Input FX** (four slots) process what you play before it is recorded – a quick way to add delay or filter to the guitar without extra plug-ins.

---

## 3. Looping software instruments and other tracks

Your synths, drum machines and samplers run on their own tracks. To loop them, **send their audio into the looper track**. Every DAW can do this; here is how.

### Ableton Live

**Option A – the looper track listens to one instrument track**

1. Looper track (audio track with livelooper): *Audio From* = the instrument track, tap point **Post FX** (with the instrument's effects, without its fader).
2. Looper track: *Monitor* = **In**.
3. The instrument track still plays to the master as well – you would hear it twice. On the instrument track set *Audio To* = **Sends Only** (sends turned down), so you hear it only through livelooper.

**Option B – many tracks into livelooper: use a Group Track**

1. Select the instrument tracks (and the mic track, if you like) and group them (Cmd/Ctrl+G).
2. Put livelooper on the **Group Track**. Everything inside the group now goes through livelooper – no monitoring switch needed on the group. (A mic track inside the group still needs *Monitor* = **In**.)

### Bitwig Studio

- **Audio track input from another track:** on the looper track, open the input chooser and pick the instrument track (Bitwig lists the outputs of all other tracks there). Set the monitor button to **On**. Route the instrument track's output away from the master (or turn it down) to avoid hearing it twice.
- **Or the Audio Receiver device:** put an *Audio Receiver* in front of livelooper on the looper track and choose the source track in it.
- **Or a Group:** group the instrument tracks and put livelooper on the group track.

### Cubase

- **Easiest: a Group Channel.** Create a Group Channel track, put livelooper on it as an insert, and set the *Output Routing* of each instrument track to that group. No monitoring needed.
- If you also want the mic: route the mic's audio track to the same group (audio track with monitoring on, without livelooper).
- An audio track can also use a group channel as its input – useful if you prefer livelooper on an audio track.

### Logic Pro

1. Set the **output** of the software instrument channel strip to a **Bus** (e.g. Bus 1). Logic creates an **Aux** channel strip that receives that bus.
2. Insert **livelooper (Audio Unit)** in an *Audio Effect* slot of the Aux.
3. Several instruments: set their outputs to the same bus. For the mic, set the audio track's output to that bus too.

The Aux has no monitoring switch – it always passes what it receives.

### Reaper

In Reaper every track can receive from every other track.

1. Open the **routing** window of the looper track (the *Route* / *IO* button) and **add a receive** from each instrument track – or add a send to the looper track on each instrument track.
2. On the instrument tracks, switch off **Master send** so you hear them only through livelooper.
3. Receives are processed by the looper track's FX, so no monitoring is needed for them (the looper track's own mic input still needs record arm + monitoring).

### Studio One

- **Bus:** create a **Bus** channel, insert livelooper there, and set the **output** of each instrument channel (or a send) to that bus.
- **Or** use the instrument's output as the **input of an audio track** (Studio One lists instrument outputs and buses in the audio track's input menu) and turn on monitoring for that track.

### Other DAWs (FL Studio, GarageBand …)

The principle is always the same: either put livelooper on a **bus / group / aux** that the instrument tracks feed, or set an **audio track's input** to the instrument track and turn monitoring on. Make sure the instrument is not also playing straight to the master.

---

## 4. Switching between instruments and sources while playing

When you perform, you want to jump between guitar, synth and voice without touching the mouse. Pick the way that suits you:

**a) Everything goes in, you choose what plays.** Route *all* sources into the looper track at the same time (group / bus / receives from section 3). Whatever is sounding gets recorded – so you choose by **playing** the instrument you want, or by **muting** the others (map the mute buttons to your controller). livelooper's **Input = Left / Right** per track (section 2) splits two sources without any muting.

**b) A group or bus collects the instruments.** Same idea, tidier: all instrument tracks feed one group/bus with livelooper on it. The group fader is the level going into the loops.

**c) Choose the instrument by arming its MIDI track.** Your keyboard plays whichever instrument track is armed / selected in the DAW, while **Push stays on livelooper**: livelooper uses Push's own User port, independent of the DAW. On Push 1 the **USER** button switches Push between livelooper and Live. In Ableton Live:

1. Put each instrument on its own MIDI track and route all of them into the looper track (group or *Audio From*, section 3).
2. Set each instrument track's *Monitor* to **Auto**: an instrument only hears your keyboard while its track is armed.
3. Arm the instrument you want to play next (Cmd/Ctrl-click arms several at once for layers). Push keeps controlling the loops.

In other DAWs: arm or select the instrument track to play it (most DAWs send your keyboard to the armed or selected track) – the routing into the looper track stays the same.

**Complete walkthrough:** Ableton Live + Push 1 with several instruments and a second keyboard → [LIVE-PUSH-WORKFLOW.md](LIVE-PUSH-WORKFLOW.md).

**Tip – two controllers:** play your instruments from a **MIDI keyboard** and drive livelooper with **Push**. Hands on the keys, loops on the pads, nothing to switch.

---

## 5. Getting the audio out

| Way | What you get | Where |
|---|---|---|
| **Main output** | the whole livelooper mix on the looper track – the normal case | the looper track's fader, then the master |
| **Direct track outputs** | each loop track on its own channel in your DAW mixer – separate effects, faders, recording | enable the extra output buses "Track 1" … "Track 8" in your DAW, set **OUTPUT** per track ([User Guide §20](USER-GUIDE.md#20-direct-track-outputs)) |
| **Drag a loop** | a loop as a clip / region on a DAW track, already on the grid | grab the waveform in the track display ([User Guide §22](USER-GUIDE.md#22-drag-loops-into-your-daw)) |
| **Export stems** | every loop as a 24-bit WAV with tempo information | **SETUP → Export stems** ([User Guide §21](USER-GUIDE.md#21-8-tracks-ghost-play-head-stem-export)) |
| **Record in the DAW** | your whole performance as audio | a second audio track with *input* = the looper track (in Live: *Audio From* = looper track, *Monitor* = **Off**, armed) |

The direct outputs are optional stereo buses of the plug-in; how you enable them depends on the DAW (in Ableton Live: a new audio track with *Audio From* = the looper track → **livelooper → Track 1**). Without an active direct output a track always plays on the main output.

---

## 6. MIDI control

You have three ways to control livelooper, and they combine:

1. **Ableton Push** – plug it in, press **USER**, done. No MIDI routing in the DAW ([PUSH.md](PUSH.md)).
2. **livelooper's MIDI learn** – your controller's MIDI has to reach the plug-in. Then right-click a control → **MIDI Learn** ([MIDI.md](MIDI.md)).
3. **Your DAW's MIDI mapping** – all functions are host parameters; map them with the DAW's own learn function (works without routing MIDI to the plug-in) ([MIDI.md](MIDI.md#way-2-your-daws-midi-mapping)).

How to get controller MIDI to the plug-in (for way 2):

| DAW | How |
|---|---|
| Ableton Live | MIDI track: *MIDI From* = your controller, *Monitor* = **In**, *MIDI To* = looper track → **livelooper** |
| Bitwig Studio | put a **Note Receiver** device in front of livelooper on the looper track, source = an instrument track that receives your controller (armed or monitored) |
| Cubase | MIDI track: *Input Routing* = your controller, *Output Routing* = livelooper (VST3 effects that accept MIDI appear in that list), monitor on |
| Logic Pro | MIDI reaches Audio Unit effects only as an **AU MIDI-controlled effect**: on a software instrument track, choose *AU MIDI-controlled Effects → livelooper* in the **Instrument** slot and pick the audio source in the plug-in window's **Side Chain** menu (not tested with livelooper yet). Simpler: use Push, or Logic's controller assignments for the host parameters (way 3) |
| Reaper | MIDI track with your controller as input (armed + monitoring), plus a **send** to the looper track with **MIDI only** |
| Studio One | instrument track: input = your controller, **output = livelooper** on the looper track / bus (plug-ins that accept MIDI appear as destinations) |

---

## 7. Troubleshooting

| Problem | Cause | Fix |
|---|---|---|
| **No sound comes in / loops stay silent** | input monitoring off on the looper track, wrong input, or the source is not routed to the looper track | turn monitoring on (section 2), check the track input / receive / bus; check the meter on the looper track |
| **Sound is doubled, phasey or "flams"** | the same source is heard twice | interface direct monitoring on → turn livelooper's **INPUT** to 0 (or switch direct monitoring off); instrument plays to the master *and* into the looper track → *Sends Only* (Live), Master send off (Reaper), output only to the bus (others) |
| **Delay between playing and hearing** | buffer size too large | 64–128 samples while playing; or monitor directly through the interface and turn **INPUT** to 0 |
| **Whistling / feedback** | microphone hears the speakers | headphones, turn speakers down, mic away from the speakers |
| **Loops don't follow the DAW tempo** | HOST SYNC off | MASTER → **HOST SYNC** on |
| **Push does nothing** | Push is not in User mode or the DAW holds the port | press **USER**; on Windows switch off "MIDIIN2/MIDIOUT2 (Ableton Push)" in the DAW ([PUSH.md](PUSH.md#setup)) |
| **Controller does nothing** | MIDI doesn't reach the plug-in | check the MIDI routing (section 6) or use your DAW's MIDI mapping |
| **Link shows "HOST SYNC HAS PRIORITY"** | in a DAW, HOST SYNC wins over Link | switch HOST SYNC off, or turn on Link in your DAW and leave HOST SYNC on (section 8) |

No DAW at hand? The **standalone app** works without one – see [INSTALLATION.md](INSTALLATION.md).

## 8. Tempo with Ableton Link (since 1.5)

There are two ways to play in time with other Link apps (another laptop, an iPad, a drum machine app):

- **Recommended in a DAW**: switch on Link in your **DAW** (e.g. Ableton Live: *Settings → Link, Tempo & MIDI → Show Link Toggle*, then the *Link* button) and leave livelooper's **HOST SYNC** on. livelooper follows the DAW, the DAW follows Link.
- **livelooper's own Link**: *SETUP → Ableton Link* – in a DAW only with **HOST SYNC off** (otherwise the host wins and the display says *LINK (HOST SYNC HAS PRIORITY)*). This is the way for the **standalone app**, where Link drives livelooper's clock directly.

Start / stop sync is optional (*SETUP → Ableton Link → Sync start / stop with Link*). Joining a session never changes its tempo.


<!-- v1.6 features -->
## 9. Recording, clips and grid controllers in a DAW (since 1.6)

- **Performance recorder**: the round red record button at the top right (below MIDI LEARN … ?) writes the looper's output (what leaves the plug-in on its main output) as a WAV file into *Music/livelooper/Recordings* – independent of your DAW's transport and recording. Mapped to a footswitch via the parameter *Performance Recorder* it starts and stops the recording hands-free. Direct track outputs are not part of the recording, only the main output.
- **Clip export 9:16**: livelooper keeps the last 30 seconds of its output in memory, also in a DAW – *SETUP → Clip export 9:16* turns the last 20 seconds into a vertical video.
- **Grid controllers** (Launchpad, APC mini, Launchkey pads – see the [user guide, section 38](USER-GUIDE.md#38-controller-templates-new-in-16)): livelooper opens the controller's MIDI port itself, like it does with Push. In the DAW, leave that port unused (no control surface / track input on it). On **Windows** a port can only be opened by one program – switch it off in the DAW's MIDI settings, otherwise livelooper shows *port in use*. With several livelooper instances only the first one drives the controller.
- **Sets**: a `.livelooper` file holds a complete looper with all audio – drop it onto the plug-in window to load it into this instance (your DAW project then saves it as usual).
<!-- end v1.6 features -->

## 10. Transport and latency (since 1.7)

- **Follow host transport is on by default**: press stop in your DAW and livelooper stops its loops; press play and they start again in sync. Want livelooper to keep looping while the DAW stands still (e.g. jamming without the arrangement running)? Switch it off in *SETUP → Looper → Follow host transport* (host parameter *Follow Transport*). Projects saved with older versions keep their setting.
- **Latency compensation**: plug-in formats do not tell a plug-in how late the audio device is, so livelooper cannot take that value from the DAW. Measure it once in the DAW: *SETUP → Audio → Latency → Measure latency …* with a cable from an interface output to the looper track's input (input monitoring on, nothing else playing). The result is stored per host and applies to every recording, overdub and capture; fine-tune with the host parameter *Latency Offset*. Details: [User Guide, section 41](USER-GUIDE.md#41-latency-measurement-and-compensation-new-in-17).
