# livelooper Sound-Packs

*Seit Version 1.6.* Ein Sound-Pack bringt Drum-Kits und BEATS-Patterns in livelooper. Packs
sind einfache Ordner (oder ein `.zip` eines solchen Ordners) mit einer Beschreibungsdatei in JSON,
den Samples und optionalen Patterns – leicht von Hand zu bauen, leicht weiterzugeben oder zu
verkaufen.

Die eingebauten Kits (Studio, 808, 909 Style, Lo-Fi, House, Techno, Boom Bap, Trap, Breaks, Rock,
Disco, Afro Latin) gehören zu livelooper selbst und sind keine Packs.

## Ein Pack installieren

- **BEATS → PACK IMPORT** und den Pack-Ordner oder die `.zip`-Datei wählen, oder
- den Pack-Ordner selbst in den Pack-Ordner kopieren und **BEATS → PACKS → Pack-Ordner neu einlesen** wählen.

| | Pack-Ordner | User-Patterns |
|---|---|---|
| macOS | `~/Music/livelooper/Sound Packs` (Ordner *Musik*) | `~/Music/livelooper/Patterns` |
| Windows | `Musik\livelooper\Sound Packs` (der Ordner *Musik* deines Benutzers) | `Musik\livelooper\Patterns` |
| Linux | `~/Music/livelooper/Sound Packs` | `~/Music/livelooper/Patterns` |

**BEATS → PACKS** listet die installierten Packs und öffnet diese Ordner.

Jedes Kit eines Packs bekommt einen der acht Plätze **Pack-Kit 1 … 8** in der Kit-Auswahl (die
Auswahl zeigt *Pack / Kit*). Ein Kit behält seinen Platz auf diesem Computer, so finden Sets, die
es benutzen, es wieder. Fehlt ein Pack, spielt sein Platz den Studio-Klang. Pack-Patterns stehen
unter **BEATS → LADEN → Sound-Packs**.

## Aufbau des Ordners

```
Mein Pack/
  livelooper-pack.json      Beschreibung (Pflicht, genau dieser Name)
  kick_soft.wav             Samples: WAV, AIFF oder FLAC, mono oder stereo (wird zu mono gemischt),
  kick_hard.wav             beliebige Samplerate, höchstens 4 Sekunden werden genutzt
  snare_1.wav
  ...
  LICENSE.txt               empfohlen: die Lizenz deiner Samples
```

Ein `.zip` darf die Dateien direkt oder in einem Ordner enthalten.

## livelooper-pack.json

```json
{
  "format": "livelooper-pack",
  "version": 1,
  "name": "Dusty Boxes",
  "author": "Dein Name",
  "license": "CC0 1.0",
  "description": "Crunchiges Drumcomputer-Kit mit Swing-Grooves",
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

### Felder

| Feld | Bedeutung |
|---|---|
| `format` | immer `"livelooper-pack"` |
| `version` | Format-Version, `1` |
| `name`, `author`, `license`, `description` | stehen unter **BEATS → PACKS**; `name` + Kit-Name kennzeichnen ein Kit |
| `kits[]` | 0 oder mehr Kits (livelooper zeigt 8 Pack-Kits gleichzeitig) |
| `kits[].name` | Name des Kits |
| `kits[].room` | kleiner Raum auf Snare / Clap / Rim, `0` … `0.5` (Standard `0.08`) |
| `kits[].fill` | Fill-Stil für FILL auf Knopfdruck und automatische Fills: `toms`, `snare roll`, `kick roll`, `hat roll`, `breaks`, `conga`, `clap roll`, `disco` (Standard `toms`) |
| `kits[].voices` | die acht Instrumente: `kick`, `snare`, `clap`, `hat_closed`, `hat_open`, `rim`, `shaker`, `tom` – jedes darf fehlen (ein Kit braucht mindestens Kick, Snare oder geschlossene Hi-Hat) |
| `voices.X.name` | Name auf der BEATS-Seite und auf Push (z. B. „Conga“ für das Tom-Instrument) |
| `voices.X.level` | Pegel des Instruments, `0` … `2` (Standard `0.8`) |
| `voices.X.samples` | bis zu 32 Samples: ein Dateiname oder `{ "file": …, "velocity": 0.05 … 1 }` |

### Velocity-Layer und Round Robin

`velocity` ist der Layer eines Samples: leise Schläge spielen die Samples mit kleinem Wert, harte
die mit großem. Gib mehreren Samples **denselben** Wert, dann wechselt livelooper zwischen ihnen
(Round Robin – nie zweimal hintereinander dasselbe Sample). Mit **VARIATION** nimmt livelooper
ab und zu auch den Nachbar-Layer, mischt zwei Takes und verändert Tonhöhe und Ausklang bei jedem
Schlag ein wenig. Gute Packs haben 3–4 Layer mit je 2–3 Takes für Kick, Snare und geschlossene
Hi-Hat.

Tipps: Stille vor dem Schlag abschneiden, jedes Instrument normalisieren (die Pegel zwischen den
Layern beibehalten), das Ende ausblenden, 44,1 oder 48 kHz.

### Patterns (auch `.llpattern`-Dateien)

| Feld | Bedeutung |
|---|---|
| `name` | Name des Patterns |
| `steps_per_bar` | `16` (16tel) oder `32` (32stel) |
| `bars` | `1` … `4` |
| `rows` | ein Eintrag pro Instrument (Schlüssel wie oben): ein Text oder eine Zahlenliste |

Text: ein Zeichen pro Schritt – `.` oder `-` = aus, `g` = Ghost Note (Velocity 40), `x` = normal
(96), `X` = Akzent (127), `1` … `9` = Velocity 14 … 126. Leerzeichen und `|` werden ignoriert,
`"X...|x...|X...|x..."` geht also auch. Eine Zahlenliste gibt die Velocity jedes Schritts an
(0 = aus, 1 … 127).

**BEATS → IN BIBLIOTHEK** schreibt dasselbe Format (mit `"format": "livelooper-pattern"`) als
`<Name>.llpattern` in den User-Pattern-Ordner; solche Dateien kannst du in die `patterns`-Liste
eines Packs übernehmen.

## Lizenzen und Packs verkaufen

livelooper prüft keine Lizenzen. Lege nur Samples in ein Pack, die du selbst gemacht hast oder
weitergeben darfst (z. B. CC0), und trage die Lizenz in `license` und eine `LICENSE.txt` ein. Das
Pack-Format ist offen; du darfst Packs bauen, weitergeben und verkaufen. Verwende keine Namen von
Drumcomputer-Herstellern oder deren Produkten in Pack- oder Kit-Namen.
