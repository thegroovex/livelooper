# MIDI-Steuerung

Jede Funktion von livelooper ist frei per MIDI steuerbar – jeder Knopf, jeder Regler, jede Auswahl, jeder Effekt-Slot. Es gibt zwei Wege, die sich beliebig kombinieren lassen.

![MIDI Learn](images/livelooper-midi-learn.png)

## Weg 1: MIDI-Learn im Plug-in

Vorbereitung in Live (einmalig):

1. MIDI-Spur anlegen.
2. *MIDI From*: dein Controller / Fußschalter, *Monitor*: **In**.
3. *MIDI To*: die Audiospur mit livelooper, darunter **livelooper** wählen.

Zuweisen:

- **Rechtsklick** auf ein Bedienelement → **MIDI Learn** → Taste/Pad/Fußschalter drücken oder Regler bewegen. Fertig.
- Oder **MIDI LEARN** in der Kopfzeile einschalten: alle zuweisbaren Elemente werden lila umrandet (bereits zugewiesene mit Punkt). Element anklicken, Controller bewegen, nächstes Element anklicken … zum Beenden **MIDI LEARN** erneut klicken.
- Entfernen: Rechtsklick → **Remove MIDI mapping**, oder im Menü **MIDI MAP** auf den Eintrag klicken.

Auch die Elemente im Spur-**SETUP** (Input, Start, Stop, Dub mode, Measures, Loop Sync, Clear) lassen sich per Rechtsklick zuweisen.

Die Zuweisungen werden mit dem Live-Set gespeichert.

### Wie MIDI-Nachrichten wirken

| Art der Funktion | Beispiele | Note | CC |
|---|---|---|---|
| Taster | REC/PLAY, STOP, CLEAR, UNDO, REDO, ALL START/STOP, CLEAR ALL, TAP | Note-On löst aus | Wert steigt über 63 → löst aus (Loslassen löst nichts aus) |
| Schalter | REV, 1SHOT, FX, Loop Sync, Effekt A–D an/aus, RHYTHM, HOST SYNC | Note-On schaltet um | steigende Flanke schaltet um |
| Auswahl | Effekttyp, Quantize, Pattern, Start/Stop/Dub-Modus | Note-On → nächster Wert | CC wählt direkt (0–127 über alle Werte) |
| Regler | Level, Pan, Effekt-Parameter, Tempo, Fade | Anschlagstärke setzt den Wert | CC setzt den Wert |

Damit funktionieren sowohl Fußschalter, die beim Drücken 127 und beim Loslassen 0 senden, als auch Pads mit Noten.

### Standard-Belegung

**MIDI MAP → Load default map** lädt eine fertige Belegung auf MIDI-Kanal 1:

| Note / CC | Funktion |
|---|---|
| C1–E1 (36–40) | REC/PLAY Track 1–5 |
| F1–A1 (41–45) | STOP Track 1–5 |
| A#1 (46) | ALL START/STOP |
| B1 (47) | TAP |
| C2–E2 (48–52) | UNDO Track 1–5 |
| C3–D#3 (60–63) | Input FX A–D an/aus |
| E3–G3 (64–67) | Track FX A–D an/aus |
| CC 20–24 | Level Track 1–5 |
| CC 25 | Master |

## Weg 2: MIDI-Map von Live

Alle Funktionen sind als Plug-in-Parameter sichtbar und automatisierbar.

1. Im Gerätetitel von livelooper auf **Konfigurieren** klicken und die gewünschten Elemente im Plug-in-Fenster anklicken bzw. bewegen – sie erscheinen als Regler im Gerät. (Bei weniger als 128 Parametern zeigt Live sie auch direkt an.)
2. **MIDI** (Cmd+M / Strg+M) einschalten, Parameter anklicken, Controller bewegen.

Taster-Parameter (z. B. „T1 Rec/Play") springen nach dem Auslösen selbst wieder auf „aus". Dadurch löst jede Betätigung genau einmal aus – egal ob Live die Taste als Umschalter oder als Momentschalter zuordnet.

Auch Automation in der Arrangement-Ansicht ist möglich, z. B. eine Aufnahme exakt an einem Taktstrich starten.

## Parameterliste

136 Parameter, Bezeichnungen wie in Live:

**Pro Spur (T1 … T5)** – je 16:
`Rec/Play`, `Stop`, `Clear`, `Undo`, `Redo` (Taster) · `Level`, `Pan` · `Reverse`, `One Shot`, `Loop Sync`, `Track FX` (Schalter) · `Input`, `Start Mode`, `Stop Mode`, `Dub Mode`, `Measures` (Auswahl)

**Global** – 16:
`All Start/Stop`, `All Start`, `All Stop`, `Clear All`, `Tap Tempo` (Taster) · `Master Level`, `Input Monitor`, `Internal Tempo`, `Fade Time` · `Quantize`, `Rec Action`, `Rhythm Pattern` (Auswahl) · `Sync to Host`, `Follow Transport`, `Rhythm On` (Schalter) · `Rhythm Level`

**Effekte** – je Slot 5 (`In FX A … D`, `Trk FX A … D`):
`On`, `Type`, `Param 1`, `Param 2`, `Mix`

Die internen Parameter-IDs (für Entwickler und gespeicherte Zuweisungen) lauten z. B. `t1_recplay`, `t3_level`, `allstartstop`, `inB_type`, `trD_mix`.
