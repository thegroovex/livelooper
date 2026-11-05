# livelooper sound packs

*Since version 1.6.* A sound pack adds drum kits and BEATS patterns to livelooper. Packs are
plain folders (or a `.zip` of such a folder) with a description file in JSON, the samples and
optional patterns – easy to make by hand, easy to share or sell.

The built-in kits (Studio, 808, 909 Style, Lo-Fi, House, Techno, Boom Bap, Trap, Breaks, Rock,
Disco, Afro Latin) are part of livelooper itself, not packs.

## Installing a pack

- **BEATS → IMPORT PACK** and choose the pack folder or the `.zip` file, or
- copy the pack folder into the packs folder yourself and choose **BEATS → PACKS → Rescan packs folder**.

| | Packs folder | User patterns |
|---|---|---|
| macOS | `~/Music/livelooper/Sound Packs` | `~/Music/livelooper/Patterns` |
| Windows | `Music\livelooper\Sound Packs` (your user's *Music* folder) | `Music\livelooper\Patterns` |
| Linux | `~/Music/livelooper/Sound Packs` | `~/Music/livelooper/Patterns` |

**BEATS → PACKS** lists the installed packs and opens these folders.

Every kit of a pack gets one of the eight slots **Pack Kit 1 … 8** in the kit selector (the
selector shows *pack / kit*). A kit keeps its slot on this computer, so sets that use it find it
again. When a pack is missing, its slot plays the Studio sound. Pack patterns appear in
**BEATS → LOAD → Sound packs**.

## Folder layout

```
My Pack/
  livelooper-pack.json      description (required, this exact name)
  kick_soft.wav             samples: WAV, AIFF or FLAC, mono or stereo (mixed to mono),
  kick_hard.wav             any sample rate, at most 4 seconds are used
  snare_1.wav
  ...
  LICENSE.txt               recommended: the licence of your samples
```

A `.zip` may contain the files directly or inside one folder.

## livelooper-pack.json

```json
{
  "format": "livelooper-pack",
  "version": 1,
  "name": "Dusty Boxes",
  "author": "Your Name",
  "license": "CC0 1.0",
  "description": "Crunchy drum machine kit with swing grooves",
  "kits": [
    {
      "name": "Dusty",
      "room": 0.1,
      "fill": "snare roll",
      "voices": {
        "kick":       { "name": "Kick", "level": 1.0,
                        "samples": [ { "file": "kick_soft.wav", "velocity": 0.4 },
                                     { "file": "kick_med.wav",  "velocity": 0.7 },
                                     { "file": "kick_hard.wav", "velocity": 1.0 },
                                     { "file": "kick_hard2.wav", "velocity": 1.0 } ] },
        "snare":      { "level": 0.8, "samples": [ "snare_1.wav", "snare_2.wav" ] },
        "clap":       { "samples": [ "clap.wav" ] },
        "hat_closed": { "level": 0.45, "samples": [ "hat_1.wav", "hat_2.wav", "hat_3.wav" ] },
        "hat_open":   { "samples": [ "open_hat.wav" ] },
        "rim":        { "samples": [ "rim.wav" ] },
        "shaker":     { "name": "Tambourine", "samples": [ "tamb.wav" ] },
        "tom":        { "name": "Conga", "samples": [ "conga_lo.wav", "conga_hi.wav" ] }
      }
    }
  ],
  "patterns": [
    {
      "name": "Dusty Swing",
      "steps_per_bar": 16,
      "bars": 1,
      "rows": {
        "kick":       "X......xX.x.....",
        "snare":      "....X.......X..g",
        "hat_closed": "x.x.x.x.x.x.x.x."
      }
    }
  ]
}
```

### Fields

| Field | Meaning |
|---|---|
| `format` | always `"livelooper-pack"` |
| `version` | format version, `1` |
| `name`, `author`, `license`, `description` | shown in **BEATS → PACKS**; `name` + kit name identify a kit |
| `kits[]` | 0 or more kits (livelooper shows 8 pack kits at a time) |
| `kits[].name` | kit name |
| `kits[].room` | small room on snare / clap / rim, `0` … `0.5` (default `0.08`) |
| `kits[].fill` | fill style for fill on demand and automatic fills: `toms`, `snare roll`, `kick roll`, `hat roll`, `breaks`, `conga`, `clap roll`, `disco` (default `toms`) |
| `kits[].voices` | the eight voices: `kick`, `snare`, `clap`, `hat_closed`, `hat_open`, `rim`, `shaker`, `tom` – any of them may be missing (a kit needs at least kick, snare or closed hat) |
| `voices.X.name` | name shown on the BEATS page and on Push (e.g. "Conga" for the tom voice) |
| `voices.X.level` | level of the voice, `0` … `2` (default `0.8`) |
| `voices.X.samples` | up to 32 samples: a file name, or `{ "file": …, "velocity": 0.05 … 1 }` |

### Velocity layers and round robin

`velocity` is the layer of a sample: soft hits play the samples with a low value, hard hits the
ones with a high value. Give several samples the **same** velocity and livelooper alternates them
(round robin – never the same sample twice in a row). With the **VARIATION** control livelooper
also uses the neighbouring layer now and then, blends two takes and changes pitch and decay a
little on every hit. Good packs have 3–4 layers with 2–3 takes each for kick, snare and closed hat.

Tips: trim the silence before the hit, normalise each voice (keep the relative levels between
layers), fade out the tail, 44.1 or 48 kHz.

### Patterns (also `.llpattern` files)

| Field | Meaning |
|---|---|
| `name` | pattern name |
| `steps_per_bar` | `16` (16th notes) or `32` (32nd notes) |
| `bars` | `1` … `4` |
| `rows` | one entry per voice (same keys as above): a text or a list of numbers |

Text: one character per step – `.` or `-` = off, `g` = ghost note (velocity 40), `x` = normal
(96), `X` = accent (127), `1` … `9` = velocity 14 … 126. Spaces and `|` are ignored, so
`"X...|x...|X...|x..."` is fine. A list of numbers gives the velocity of each step (0 = off,
1 … 127).

**BEATS → SAVE** writes the same format (with `"format": "livelooper-pattern"`) as
`<name>.llpattern` into the beat library (user patterns folder); you can copy such files into the
`patterns` list of a pack. Since 1.6 a saved beat may also carry `"format_version": 2`,
`"swing"` (50 – 75 %) and a `"mixer"` object (`"kick": { "level": 1.0, "pitch": 0, "mute": false,
"solo": false }`, …); both are optional, and packs ignore them. A row that plays an own sample
adds `"sample": "<absolute path of the audio file>"` to its mixer entry (`""` = the kit sample).

## Licences and selling packs

livelooper does not check licences. Only put samples into a pack that you made yourself or may
pass on (e.g. CC0), and write the licence into `license` and a `LICENSE.txt`. The pack format is
open; you may make, share and sell packs. Do not use names of drum machine makers or their
products in pack or kit names.
