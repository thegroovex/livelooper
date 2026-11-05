# Live-Setup: Ableton Live + Push 1 mit mehreren Instrumenten

[English](../en/LIVE-PUSH-WORKFLOW.md) | **Deutsch**

Eine Schritt-für-Schritt-Anleitung für ein typisches Live-Setup: **Keyboards, Gesang, Gitarre und Drums** laufen alle in livelooper, ein **Ableton Push 1** steuert die Loops, und ein **zweites MIDI-Keyboard** spielt einen Software-Synth auf seiner eigenen Spur. Die Grundlagen stehen in [DAW-EINRICHTUNG.md](DAW-EINRICHTUNG.md), die komplette Push-Belegung in [PUSH.md](PUSH.md).

**Das Prinzip:** Alle Quellen laufen in eine Gruppe, livelooper sitzt auf dieser Gruppe, der Push steuert die Loops, das zweite Keyboard spielt nur den Synth.

```
Vocals ─┐
Guitar ─┤
Keys   ─┼──►  Gruppe „Looper“ mit livelooper  ──►  Master
Drums  ─┘
Push 1 ──► livelooper (User-Port)      MIDI-Keyboard ──► Synth auf „Keys“
```

## 1. Vorbereitung (einmalig)

1. **Audio-Einstellungen:** Puffer auf **64–128 Samples**. Mehr nur, wenn es knackst.
2. **Push** in Lives MIDI-Einstellungen ganz normal als Bedienoberfläche „Push“ eingerichtet lassen.
3. **Nur Windows:** Ein- und Ausgang **MIDIIN2 / MIDIOUT2 (Ableton Push)** komplett aus (Spur, Sync, Fern). Sonst kann livelooper den Push nicht öffnen.
4. **Zweites MIDI-Keyboard:** in den MIDI-Einstellungen beim Eingang **Spur = an**.
5. **Kopfhörer** für den Gesang, sonst nimmt jeder Overdub die Lautsprecher mit auf.

## 2. Spuren anlegen

| Spur | Typ | Eingang | Monitor |
|---|---|---|---|
| **Vocals** | Audio | Ext. In → Mikro-Eingang (mono) | **In** |
| **Guitar** | Audio | Ext. In → Gitarren-Eingang (mono) | **In** |
| **Keys** | MIDI + dein Software-Synth | **nur das zweite Keyboard**, alle Kanäle | **In** |
| **Drums** | Drum Rack / Drum-Plug-in (Clips oder Pads) oder Drum-Mikros | je nach Quelle | **In** bei Mikros |

**Tipp:** Stell bei **Keys** den Eingang fest auf das zweite Keyboard und Monitor auf **In**. Dann spielt der Synth immer, egal welche Spur gerade ausgewählt ist, und der Push bleibt komplett frei für den Looper.

## 3. Alles in den Looper schicken

1. Alle vier Spuren markieren und gruppieren (**Cmd/Strg+G**), die Gruppe z. B. „Looper“ nennen.
2. **livelooper auf die Gruppenspur** ziehen. Alles in der Gruppe läuft jetzt durch den Looper.
3. Der **Gruppenfader** ist der Pegel, der in die Loops geht. Die einzelnen Fader sind dein Mix der Quellen.
4. **Mithören** entweder über livelooper (Regler **EINGANG** auf 100 %) **oder** über das Direct Monitoring deines Interfaces (EINGANG auf 0). Nie beides, sonst klingt es doppelt.

## 4. Push auf livelooper schalten

1. **USER** drücken. Der Push wechselt zu livelooper, **SETUP → Ableton Push** zeigt „Verbunden“.
2. Nochmal **USER** = zurück zu Live (z. B. um das Drum Rack auf den Pads zu spielen oder Clips zu starten).
3. Optional **SETUP → Spuren → 8 Spuren**, wenn dir 5 Loop-Spuren nicht reichen.

## 5. Einmal die Latenz messen

**SETUP → Audio → Latenz → Latenz messen …**: ein Kabel von einem Interface-Ausgang in einen Eingang, der in der Gruppe landet. Sonst nichts abspielen. Danach sitzen Aufnahmen und Overdubs auf dem Punkt ([Bedienungsanleitung, Abschnitt 41](BEDIENUNGSANLEITUNG.md#41-latenz-messen-und-ausgleichen-neu-in-17)).

## 6. Transport und Tempo

- Tempo in Live einstellen und **Live starten (Leertaste)**. *Host-Transport folgen* ist an: Stoppt Live, stoppen auch die Loops.
- Willst du bei stehendem Live jammen: **SETUP → Looper → Host-Transport folgen** aus.

## 7. Spielen mit dem Push (LOOP-Modus, Taste SESSION)

| Push | Funktion |
|---|---|
| Unterste Pad-Reihe | **REC / PLAY / DUB** je Spur (Spalte = Loop-Spur) |
| Zweite Pad-Reihe | **STOP** |
| Pads 3–8 | Loop-Position: tippen springt, halten stottert |
| **FIXED LENGTH** halten + Pad | Loop-Länge der Spur: AUTO, 1, 2, 4, 8, 16 Takte |
| REC-Pad halten + Positions-Pad | Länge nur für diese Aufnahme |
| **MUTE / SOLO** + Pad | Spur stumm oder solo |
| **PLAY** | alle Start / Stop |
| **RECORD** | die letzten Takte nachträglich als Loop übernehmen |
| **UNDO** / SHIFT + UNDO | Rückgängig / Wiederholen |
| Szenen-Tasten | Szenen 1–8, SHIFT = speichern |
| Encoder | Pegel, Panorama, Tonhöhe, FX |
| Touchstrip | Tape-Stopp, SHIFT = Filter |
| Fußschalter 2 | REC / PLAY / DUB der gewählten Spur, doppelt = STOP, 1 s = löschen |
| Fußschalter 1 | nächste Spur |

## 8. Wichtig: Was landet in welchem Loop?

livelooper nimmt auf, **was gerade in der Gruppe klingt**. Du wählst die Quelle also durch Spielen:

- Beim Gitarren-Loop einfach nur Gitarre spielen, beim Synth-Loop nur das Keyboard.
- Drums, die als Clip laufen, vorher stoppen oder die Spur stummschalten, sonst sind sie im nächsten Loop mit drin.
- **Praktisch:** die Spur-Aktivatoren (Mute) von Vocals, Guitar, Keys und Drums per **Cmd/Strg+M** (Lives MIDI-Zuweisung) auf Tasten oder Pads des zweiten Keyboards legen. So schaltest du Quellen frei, ohne den Push zu verlassen.
- **SETUP → Auto-Aufnahme:** Spur scharf schalten, die Aufnahme startet mit dem ersten Ton. Die Hände bleiben an Gitarre oder Tasten.

## 9. Beispiel-Ablauf für einen Song

1. Live starten, FIXED LENGTH halten, Spur 1 auf **4 Takte**.
2. Spur 1: Drums aufnehmen (Clip laufen lassen oder einspielen), danach die Drum-Quelle stummschalten.
3. Spur 2: Gitarren-Riff.
4. Spur 3: Bass oder Fläche mit dem Synth über das zweite Keyboard.
5. Spur 4: Gesang, Spur 5: zweite Stimme oder Overdub.
6. Mit MUTE / SOLO, Szenen und Touchstrip arrangieren, Fußschalter für freihändige Aufnahmen.

## 10. Sichern

- Das Live-Projekt speichert den kompletten Looper inklusive Audio mit.
- Das **Auto-Backup** läuft im Hintergrund. Nach einem Absturz bietet livelooper beim nächsten Öffnen die Wiederherstellung an ([Bedienungsanleitung, Abschnitt 46](BEDIENUNGSANLEITUNG.md#46-automatische-sicherung-und-absturz-wiederherstellung-neu-in-17)).
