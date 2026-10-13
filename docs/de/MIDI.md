# MIDI-Steuerung

[English](../en/MIDI.md) | **Deutsch**

Jede Funktion von livelooper ist frei per MIDI steuerbar – jeder Knopf, jeder Regler, jede Auswahl, jeder Effekt-Slot. Es gibt zwei Wege, die sich beliebig kombinieren lassen.

![MIDI Learn](../images/livelooper-midi-learn-de.png)

## Weg 1: MIDI-Learn im Plug-in

Vorbereitung in Live (einmalig):

1. MIDI-Spur anlegen.
2. *MIDI From*: dein Controller / Fußschalter, *Monitor*: **In**.
3. *MIDI To*: die Audiospur mit livelooper, darunter **livelooper** wählen.

Zuweisen:

- **Rechtsklick** auf ein Bedienelement → **MIDI Learn** → Taste/Pad/Fußschalter drücken oder Regler bewegen. Fertig.
- Oder **MIDI LEARN** in der Kopfzeile einschalten: alle zuweisbaren Elemente werden lila umrandet (bereits zugewiesene mit Punkt). Element anklicken, Controller bewegen, nächstes Element anklicken … zum Beenden **MIDI LEARN** erneut klicken.
- Entfernen: Rechtsklick → **MIDI-Zuweisung entfernen**, oder im Menü **MIDI MAP** auf den Eintrag klicken.

Auch die Elemente im Spur-**SETUP** (Eingang, Start, Stop, Dub-Modus, Takte, Loop-Sync, Löschen) lassen sich per Rechtsklick zuweisen.

Die Zuweisungen werden mit dem Live-Set gespeichert.

### Wie MIDI-Nachrichten wirken

| Art der Funktion | Beispiele | Note | CC |
|---|---|---|---|
| Taster | REC/PLAY, STOP, LÖSCHEN, ZURÜCK (Undo), VOR (Redo), ALLE START/STOP, ALLE LÖSCHEN, TAP | Note-On löst aus | Wert steigt über 63 → löst aus (Loslassen löst nichts aus) |
| Schalter | RÜCKW., 1SHOT, FX, Loop-Sync, Effekt A–D an/aus, RHYTHMUS, HOST-SYNC | Note-On schaltet um | steigende Flanke schaltet um |
| Auswahl | Effekttyp, Quantisierung, Muster, Start-/Stopp-/Dub-Modus | Note-On → nächster Wert | CC wählt direkt (0–127 über alle Werte) |
| Regler | Pegel, Pan, Effekt-Parameter, Tempo, Fade, Tape-Stopp, Performance-Filter | Anschlagstärke setzt den Wert | CC setzt den Wert |
| Gehalten (seit 1.4) | Slice 1–6 (Loop-Cutting) | an, solange die Note gehalten wird, aus mit Note-Off | an, solange der CC ≥ 64 ist |

Die Slice-Funktionen verhalten sich wie die Positions-Pads am Push: kurz drücken springt (zur nächsten 16tel-Note) in dieses Sechstel des Loops und bleibt dort, länger als ¼ s halten stottert und springt beim Loslassen synchron zurück. *Stumm* und *Solo* sind für MIDI Schalter (drücken = an, nochmal = aus); das Stummschalten / Solo nur solange gehalten ist eine Push-Geste.

Damit funktionieren sowohl Fußschalter, die beim Drücken 127 und beim Loslassen 0 senden, als auch Pads mit Noten.

### Standard-Belegung

**MIDI MAP → Standard-Map laden** lädt eine fertige Belegung auf MIDI-Kanal 1:

| Note / CC | Funktion |
|---|---|
| C1–E1 (36–40) | REC/PLAY Spur 1–5 |
| F1–A1 (41–45) | STOP Spur 1–5 |
| A#1 (46) | ALLE START/STOP |
| B1 (47) | TAP |
| C2–E2 (48–52) | ZURÜCK (Undo) Spur 1–5 |
| C3–D#3 (60–63) | Eingangs-FX A–D an/aus |
| E3–G3 (64–67) | Spur-FX A–D an/aus |
| CC 20–24 | Pegel Spur 1–5 |
| CC 25 | Master |

## Weg 2: MIDI-Map von Live

Alle Funktionen sind als Plug-in-Parameter sichtbar und automatisierbar.

1. Im Gerätetitel von livelooper auf **Konfigurieren** klicken und die gewünschten Elemente im Plug-in-Fenster anklicken bzw. bewegen – sie erscheinen als Regler im Gerät. (Bei weniger als 128 Parametern zeigt Live sie auch direkt an.)
2. **MIDI** (Cmd+M / Strg+M) einschalten, Parameter anklicken, Controller bewegen.

Taster-Parameter (z. B. „S1 Rec/Play") springen nach dem Auslösen selbst wieder auf „aus". Dadurch löst jede Betätigung genau einmal aus – egal ob Live die Taste als Umschalter oder als Momentschalter zuordnet.

Auch Automation in der Arrangement-Ansicht ist möglich, z. B. eine Aufnahme exakt an einem Taktstrich starten.

## Parameterliste

372 Parameter. Mit deutscher Oberfläche zeigt Live die deutschen Namen, mit englischer die englischen (in Klammern). Die internen IDs und gespeicherte Zuweisungen bleiben in beiden Sprachen gleich – ein Sprachwechsel zerstört also keine MIDI-Map und keine Automation.

**Pro Spur (S1 … S5 / T1 … T5)** – je 16:
`Rec/Play`, `Stop`, `Löschen` (Clear), `Rückgängig` (Undo), `Wiederherstellen` (Redo) – Taster · `Pegel` (Level), `Panorama` (Pan) · `Rückwärts` (Reverse), `One Shot`, `Loop-Sync` (Loop Sync), `Spur-FX` (Track FX) – Schalter · `Eingang` (Input), `Startmodus` (Start Mode), `Stoppmodus` (Stop Mode), `Dub-Modus` (Dub Mode), `Takte` (Measures) – Auswahl

**Global** – 16:
`Alle Start/Stop` (All Start/Stop), `Alle Start` (All Start), `Alle Stop` (All Stop), `Alle löschen` (Clear All), `Tap Tempo` – Taster · `Master-Pegel` (Master Level), `Eingangsmonitor` (Input Monitor), `Internes Tempo` (Internal Tempo), `Fade-Zeit` (Fade Time) · `Quantisierung` (Quantize), `Aufnahme-Aktion` (Rec Action), `Rhythmus-Muster` (Rhythm Pattern), `Drum-Kit` (Drum Kit), `Metronomklang` (Metronome Sound) – Auswahl · `Host-Sync` (Sync to Host), `Transport folgen` (Follow Transport), `Rhythmus an` (Rhythm On) – Schalter · `Rhythmus-Pegel` (Rhythm Level), `Swing` (Swing), `Humanisierung` (Humanize)

**Effekte** – je Slot 5 (`Ein-FX A … D` / `In FX A … D`, `Sp-FX A … D` / `Trk FX A … D`):
`An` (On), `Typ` (Type), `Parameter 1` (Param 1), `Parameter 2` (Param 2), `Mix`

**Seit 1.3** – pro Spur: `Capture` (Taster), `Feedback`, `Ducking`, `Tonhöhe` (Pitch), `Ausgang` (Output, Auswahl), `Tempo folgen` (Follow Tempo, Schalter); global: `Capture` (Taster), `Capture-Takte` (Capture Bars), `Auto-Aufnahme` (Auto Record), Schwelle, Ducking-Quelle, `Szene 1 … 8` (Taster), Szene speichern. Spuren S1 … S8.

**Seit 1.4 (Performance)** – pro Spur 15 weitere: `Slice 1 … 6` (gehalten), `Zurück in Sync` (Back in Sync), `Loop verdoppeln` (Double Loop), `Loop halbieren` (Halve Loop), `Oktave hoch` (Octave Up), `Oktave runter` (Octave Down), `In leere Spur kopieren` (Copy to Empty Track) – Taster · `Stumm` (Mute), `Solo` – Schalter · `Aufnahmelänge` (Rec Length, Auswahl, die einmal auslöst: `1 Takt` … `32 Takte` für die nächste oder laufende erste Aufnahme, danach wieder `-`); global: `Tape-Stopp` (Tape Stop: 0 % = läuft, 100 % = das Band steht; zurück auf 0 % = läuft synchron wieder an), `Performance-Filter` (−100 = Tiefpass zu, 0 = aus, +100 = Hochpass offen).

Die internen Parameter-IDs (für Entwickler und gespeicherte Zuweisungen) lauten z. B. `t1_recplay`, `t3_level`, `allstartstop`, `inB_type`, `trD_mix`.
