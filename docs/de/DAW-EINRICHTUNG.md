# livelooper in deiner DAW

[English](../en/DAW-SETUP.md) | **Deutsch**

Dieses Kapitel zeigt, wie du das livelooper-Plug-in (VST3 / AU) so in deine DAW einbindest, dass du es **spielen** kannst: Mikro oder Gitarre loopen, deine Software-Instrumente loopen, beim Spielen zwischen Quellen wechseln und die Loops wieder herausholen. Die Installation steht in [INSTALLATION.md](INSTALLATION.md), die Funktionen selbst in der [Bedienungsanleitung](BEDIENUNGSANLEITUNG.md).

Kurz gesagt: **eine Audiospur, livelooper darauf, alles, was du loopen willst, geht in diese Spur – und deine Hände liegen auf einem [Ableton Push](PUSH.md).**

---

## 1. Das Grundprinzip

livelooper ist ein **Audio-Effekt mit MIDI-Eingang**. Du setzt ihn auf **eine Audiospur** – in dieser Anleitung heißt sie **Looper-Spur**.

```
Mikro / Gitarre / Instrumentenspuren  ──►  Eingang der Looper-Spur  ──►  livelooper  ──►  Master
```

- **Alles, was du loopen willst, muss am Eingang der Looper-Spur ankommen.** livelooper kann nur aufnehmen, was die Spur erreicht, auf der er sitzt. Er hat **einen Stereo- (oder Mono-)Eingang** und keinen Sidechain-Eingang – du bringst andere Quellen also *zur* Looper-Spur, nicht an eine Sidechain.
- **Der Ausgang von livelooper ersetzt das Signal der Spur**: Du hörst die Loops und – mit dem Regler **EINGANG** (MASTER-Bereich, als Host-Parameter *Input Monitor*) – den Live-Eingang direkt durchgeschleift.
- livelooper selbst fügt **keine zusätzliche Latenz** hinzu.
- Ein **Ableton Push** braucht überhaupt kein Routing: livelooper spricht ihn direkt über seinen User-Port an, in jeder DAW ([PUSH.md](PUSH.md)).

### Monitoring: Welcher Schalter macht was?

Es gibt drei Stellen, an denen du deinen Live-Eingang hören kannst. So setzt du sie ein:

| Schalter | Was er tut | Einstellung |
|---|---|---|
| **Eingangs-Monitoring der DAW** auf der Looper-Spur (*Monitor In*, Monitor-Knopf …) | lässt den Live-Eingang überhaupt erst **in die Spur und in livelooper** | **an** – sonst bekommt livelooper nichts und nimmt Stille auf |
| **Regler EINGANG in livelooper** | wie laut du den Live-Eingang über livelooper hörst (die Aufnahme ist davon unabhängig) | **100 %**, wenn du über den Rechner hörst; **0**, wenn du dich schon anders hörst |
| **Direct Monitoring deines Audio-Interfaces** | du hörst den Eingang ohne Umweg über den Rechner, ohne Latenz | entweder das **oder** der Regler EINGANG – nie beides |

Das DAW-Monitoring auf der Looper-Spur einzuschalten, **verdoppelt das Signal nicht**: livelooper sitzt auf dieser Spur, und was du von ihm hörst, regelt sein EINGANG-Regler. Doppelt oder „flammend“ klingt es nur, wenn dieselbe Quelle zweimal bei dir ankommt – z. B. Direct Monitoring des Interfaces *plus* EINGANG auf 100 %, oder eine Instrumentenspur, die auf den Master *und* in die Looper-Spur spielt. Abschnitt 7 zeigt die Lösungen.

**Faustregel:** DAW-Monitoring auf der Looper-Spur immer an, und den Eingang **entweder** über livelooper hören (EINGANG auf, kleiner Puffer) **oder** über das Direct Monitoring des Interfaces (EINGANG auf 0).

---

## 2. Mikrofon oder Gitarre loopen

1. **Eine Audiospur anlegen** – das ist deine Looper-Spur.
2. **Ihren Eingang** auf den Interface-Eingang stellen, an dem Mikro oder Gitarre steckt (Mono reicht; für eine Stereoquelle das Paar wählen).
3. **Eingangs-Monitoring** für die Spur einschalten (Tabelle unten).
4. **livelooper** auf die Spur setzen.
5. REC auf Spur 1 drücken (am Push, im Plug-in oder per MIDI) und spielen.

| DAW | Spureingang | Monitoring |
|---|---|---|
| Ableton Live | *Audio From* = *Ext. In* → dein Eingang | *Monitor* = **In** |
| Bitwig Studio | Eingangswahl = dein Audio-Eingang | Monitor-Knopf = **On** (Auto hört nur bei scharfgeschalteter Spur mit) |
| Cubase | *Eingangs-Routing* = dein Eingangsbus | **Monitor**-Knopf der Spur |
| Logic Pro | Eingang der Audiospur = dein Eingang; livelooper als **Audio Unit** in einem *Audio-Effekt*-Slot | Knopf **Input Monitoring** der Spur |
| Reaper | Rechtsklick auf den Aufnahmeknopf der Spur → Eingang | **Aufnahmebereit** + *Monitor input* (Rechtsklick-Menü) |
| Studio One | Eingang der Audiospur = dein Eingang | **Monitor**-Knopf der Spur |

**Muss die Spur aufnahmebereit sein?** In Live (Monitor **In**) und in Bitwig (Monitor **On**) nicht – die DAW nimmt nichts auf, das macht livelooper. In DAWs, in denen das Monitoring nur auf einer scharfgeschalteten Spur funktioniert (Reaper, je nach Einstellung auch Cubase, Logic oder Studio One), schaltest du sie scharf. Das schadet nicht, solange du nicht den Aufnahmeknopf der DAW drückst.

**Tipps**

- **Kleiner Puffer = tightes Spielen.** Stell die Puffergröße in den Audio-Einstellungen deiner DAW zum Spielen auf 64–128 Samples; erhöhe sie nur, wenn es knackst.
- **Kopfhörer** beim Mikrofon. Lautsprecher + offenes Mikro + Loops = Rückkopplung, und jeder Overdub nimmt die Lautsprecher mit auf.
- **Mikro und Gitarre gleichzeitig?** Beide als Stereopaar in die Looper-Spur schicken (z. B. Eingänge 1/2) und im SETUP jeder livelooper-Spur **Eingang = Links** oder **Rechts** wählen: Spur 1 nimmt nur die Gitarre auf, Spur 2 nur das Mikro – ganz ohne Umschalten in der DAW.
- Die **Input-FX** von livelooper (vier Slots) bearbeiten, was du spielst, bevor es aufgenommen wird – schnell ein Delay oder Filter auf die Gitarre, ohne weitere Plug-ins.

---

## 3. Software-Instrumente und andere Spuren loopen

Deine Synths, Drumcomputer und Sampler laufen auf eigenen Spuren. Um sie zu loopen, **schickst du ihr Audio in die Looper-Spur**. Das kann jede DAW, und zwar so:

### Ableton Live

**Variante A – die Looper-Spur hört auf eine Instrumentenspur**

1. Looper-Spur (Audiospur mit livelooper): *Audio From* = die Instrumentenspur, Abgriffspunkt **Post FX** (mit den Effekten des Instruments, ohne dessen Fader).
2. Looper-Spur: *Monitor* = **In**.
3. Die Instrumentenspur spielt weiterhin auch auf den Master – du würdest sie doppelt hören. Stell auf der Instrumentenspur *Audio To* = **Sends Only** (Sends zugedreht), dann hörst du sie nur über livelooper.

**Variante B – viele Spuren in livelooper: eine Gruppenspur**

1. Die Instrumentenspuren (und, wenn du willst, die Mikrospur) auswählen und gruppieren (Cmd/Strg+G).
2. livelooper auf die **Gruppenspur** setzen. Alles in der Gruppe läuft jetzt durch livelooper – auf der Gruppe ist kein Monitoring-Schalter nötig. (Eine Mikrospur in der Gruppe braucht weiterhin *Monitor* = **In**.)

### Bitwig Studio

- **Audiospur-Eingang von einer anderen Spur:** Auf der Looper-Spur die Eingangswahl öffnen und die Instrumentenspur wählen (Bitwig listet dort die Ausgänge aller anderen Spuren). Monitor-Knopf auf **On**. Den Ausgang der Instrumentenspur vom Master wegnehmen (oder sie herunterziehen), damit du sie nicht doppelt hörst.
- **Oder das Gerät Audio Receiver:** einen *Audio Receiver* vor livelooper auf die Looper-Spur setzen und darin die Quellspur wählen.
- **Oder eine Gruppe:** die Instrumentenspuren gruppieren und livelooper auf die Gruppenspur setzen.

### Cubase

- **Am einfachsten: ein Gruppenkanal.** Eine Gruppenkanalspur anlegen, livelooper als Insert darauf setzen und das *Ausgangs-Routing* jeder Instrumentenspur auf diese Gruppe stellen. Kein Monitoring nötig.
- Das Mikro dazu: die Audiospur des Mikros ebenfalls auf diese Gruppe routen (Audiospur mit eingeschaltetem Monitoring, ohne livelooper).
- Eine Audiospur kann auch einen Gruppenkanal als Eingang verwenden – praktisch, wenn du livelooper lieber auf einer Audiospur hast.

### Logic Pro

1. Den **Ausgang** des Software-Instrument-Kanalzugs auf einen **Bus** stellen (z. B. Bus 1). Logic legt einen **Aux**-Kanalzug an, der diesen Bus empfängt.
2. **livelooper (Audio Unit)** in einen *Audio-Effekt*-Slot des Aux setzen.
3. Mehrere Instrumente: deren Ausgänge auf denselben Bus. Fürs Mikro den Ausgang der Audiospur ebenfalls auf diesen Bus.

Der Aux hat keinen Monitoring-Schalter – er reicht immer durch, was ankommt.

### Reaper

In Reaper kann jede Spur von jeder anderen empfangen.

1. Das **Routing**-Fenster der Looper-Spur öffnen (Knopf *Route* / *IO*) und von jeder Instrumentenspur einen **Receive** hinzufügen – oder auf jeder Instrumentenspur einen Send zur Looper-Spur.
2. Auf den Instrumentenspuren **Master send** ausschalten, damit du sie nur über livelooper hörst.
3. Receives laufen durch die FX der Looper-Spur, dafür ist kein Monitoring nötig (der eigene Mikro-Eingang der Looper-Spur braucht weiterhin Aufnahmebereitschaft + Monitoring).

### Studio One

- **Bus:** einen **Bus**-Kanal anlegen, livelooper darauf setzen und den **Ausgang** jedes Instrumentenkanals (oder einen Send) auf diesen Bus stellen.
- **Oder** den Ausgang des Instruments als **Eingang einer Audiospur** wählen (Studio One listet Instrumentenausgänge und Busse im Eingangsmenü der Audiospur) und für diese Spur das Monitoring einschalten.

### Andere DAWs (FL Studio, GarageBand …)

Das Prinzip ist immer gleich: livelooper entweder auf einen **Bus / eine Gruppe / einen Aux** setzen, in den die Instrumentenspuren spielen, oder den **Eingang einer Audiospur** auf die Instrumentenspur stellen und das Monitoring einschalten. Achte darauf, dass das Instrument nicht zusätzlich direkt auf den Master spielt.

---

## 4. Beim Spielen zwischen Instrumenten und Quellen wechseln

Auf der Bühne willst du zwischen Gitarre, Synth und Stimme springen, ohne die Maus anzufassen. Wähl den Weg, der zu dir passt:

**a) Alles geht rein, du entscheidest, was klingt.** *Alle* Quellen gleichzeitig in die Looper-Spur routen (Gruppe / Bus / Receives aus Abschnitt 3). Was gerade klingt, wird aufgenommen – du wählst also, indem du das gewünschte Instrument **spielst** oder die anderen **stummschaltest** (Mute-Tasten auf deinen Controller legen). Mit **Eingang = Links / Rechts** pro Spur (Abschnitt 2) trennst du zwei Quellen ganz ohne Stummschalten.

**b) Eine Gruppe oder ein Bus sammelt die Instrumente.** Gleiche Idee, aufgeräumter: Alle Instrumentenspuren speisen eine Gruppe / einen Bus mit livelooper darauf. Der Gruppenfader ist der Pegel, der in die Loops geht.

**c) Das Instrument wählen, indem du seine MIDI-Spur scharfschaltest.** Dein Keyboard spielt die Instrumentenspur, die in der DAW scharfgeschaltet / ausgewählt ist, während **der Push auf livelooper bleibt**: livelooper nutzt den eigenen User-Port des Push, unabhängig von der DAW. Am Push 1 schaltet die Taste **USER** den Push zwischen livelooper und Live um. In Ableton Live:

1. Jedes Instrument auf eine eigene MIDI-Spur legen und alle in die Looper-Spur routen (Gruppe oder *Audio From*, Abschnitt 3).
2. *Monitor* jeder Instrumentenspur auf **Auto**: Ein Instrument hört dein Keyboard nur, solange seine Spur scharfgeschaltet ist.
3. Das Instrument scharfschalten, das du als Nächstes spielen willst (Cmd/Strg-Klick schaltet mehrere für Layer scharf). Der Push steuert weiter die Loops.

In anderen DAWs: die Instrumentenspur scharfschalten oder auswählen, um sie zu spielen (die meisten DAWs schicken das Keyboard an die scharfgeschaltete bzw. ausgewählte Spur) – das Routing in die Looper-Spur bleibt gleich.

**Tipp – zwei Controller:** Spiel deine Instrumente mit einem **MIDI-Keyboard** und steuere livelooper mit dem **Push**. Hände auf den Tasten, Loops auf den Pads, nichts umzuschalten.

---

## 5. Audio herausholen

| Weg | Was du bekommst | Wo |
|---|---|---|
| **Hauptausgang** | den ganzen livelooper-Mix auf der Looper-Spur – der Normalfall | Fader der Looper-Spur, dann Master |
| **Direkte Spurausgänge** | jede Loop-Spur auf einem eigenen Kanal im DAW-Mixer – eigene Effekte, Fader, Aufnahme | die zusätzlichen Ausgangsbusse „Track 1“ … „Track 8“ in der DAW aktivieren, pro Spur **AUSGANG** wählen ([Bedienungsanleitung §20](BEDIENUNGSANLEITUNG.md#20-direkte-spurausgänge)) |
| **Loop ziehen** | einen Loop als Clip / Region auf einer DAW-Spur, gleich im Raster | die Wellenform im Spur-Display packen ([Bedienungsanleitung §22](BEDIENUNGSANLEITUNG.md#22-loops-in-die-daw-ziehen)) |
| **Stems exportieren** | jeden Loop als 24-Bit-WAV mit Tempoangaben | **SETUP → Stems exportieren** ([Bedienungsanleitung §21](BEDIENUNGSANLEITUNG.md#21-8-spuren-geister-abspielmarke-stem-export)) |
| **In der DAW aufnehmen** | deine ganze Performance als Audio | eine zweite Audiospur mit *Eingang* = Looper-Spur (in Live: *Audio From* = Looper-Spur, *Monitor* = **Off**, scharfgeschaltet) |

Die direkten Ausgänge sind optionale Stereo-Busse des Plug-ins; wie du sie aktivierst, hängt von der DAW ab (in Ableton Live: eine neue Audiospur mit *Audio From* = Looper-Spur → **livelooper → Track 1**). Ohne aktiven direkten Ausgang spielt eine Spur immer auf dem Hauptausgang.

---

## 6. MIDI-Steuerung

Es gibt drei Wege, livelooper zu steuern, und sie lassen sich kombinieren:

1. **Ableton Push** – anschließen, **USER** drücken, fertig. Kein MIDI-Routing in der DAW ([PUSH.md](PUSH.md)).
2. **MIDI-Learn von livelooper** – das MIDI deines Controllers muss das Plug-in erreichen. Dann Rechtsklick auf ein Bedienelement → **MIDI Learn** ([MIDI.md](MIDI.md)).
3. **MIDI-Mapping deiner DAW** – alle Funktionen sind Host-Parameter; weise sie mit der Lernfunktion der DAW zu (funktioniert ohne MIDI-Routing zum Plug-in) ([MIDI.md](MIDI.md#weg-2-midi-mapping-deiner-daw)).

So kommt das Controller-MIDI zum Plug-in (für Weg 2):

| DAW | So geht's |
|---|---|
| Ableton Live | MIDI-Spur: *MIDI From* = dein Controller, *Monitor* = **In**, *MIDI To* = Looper-Spur → **livelooper** |
| Bitwig Studio | ein Gerät **Note Receiver** vor livelooper auf die Looper-Spur setzen, Quelle = eine Instrumentenspur, die deinen Controller empfängt (scharfgeschaltet oder mit Monitoring) |
| Cubase | MIDI-Spur: *Eingangs-Routing* = dein Controller, *Ausgangs-Routing* = livelooper (VST3-Effekte, die MIDI annehmen, erscheinen in dieser Liste), Monitoring an |
| Logic Pro | MIDI erreicht Audio-Unit-Effekte nur als **AU-MIDI-gesteuerter Effekt**: auf einer Software-Instrument-Spur im **Instrument**-Slot *AU MIDI-controlled Effects → livelooper* wählen und die Audioquelle im Menü **Side Chain** des Plug-in-Fensters wählen (mit livelooper noch nicht getestet). Einfacher: Push verwenden oder die Controller-Zuweisungen von Logic für die Host-Parameter (Weg 3) |
| Reaper | MIDI-Spur mit deinem Controller als Eingang (aufnahmebereit + Monitoring), dazu ein **Send** zur Looper-Spur mit **nur MIDI** |
| Studio One | Instrumentenspur: Eingang = dein Controller, **Ausgang = livelooper** auf der Looper-Spur / dem Bus (Plug-ins, die MIDI annehmen, erscheinen als Ziel) |

---

## 7. Fehlersuche

| Problem | Ursache | Lösung |
|---|---|---|
| **Es kommt nichts an / Loops bleiben stumm** | Eingangs-Monitoring auf der Looper-Spur aus, falscher Eingang oder die Quelle ist nicht in die Looper-Spur geroutet | Monitoring einschalten (Abschnitt 2), Spureingang / Receive / Bus prüfen; Pegelanzeige der Looper-Spur ansehen |
| **Klang doppelt, phasig oder „flammend“** | dieselbe Quelle kommt zweimal an | Direct Monitoring am Interface an → **EINGANG** in livelooper auf 0 (oder Direct Monitoring aus); Instrument spielt auf den Master *und* in die Looper-Spur → *Sends Only* (Live), Master send aus (Reaper), Ausgang nur auf den Bus (andere) |
| **Verzögerung zwischen Spielen und Hören** | Puffer zu groß | 64–128 Samples beim Spielen; oder direkt übers Interface hören und **EINGANG** auf 0 |
| **Pfeifen / Rückkopplung** | das Mikro hört die Lautsprecher | Kopfhörer, Lautsprecher leiser, Mikro weg von den Lautsprechern |
| **Loops folgen nicht dem DAW-Tempo** | HOST-SYNC aus | MASTER → **HOST-SYNC** an |
| **Push reagiert nicht** | Push nicht im User-Modus oder die DAW belegt den Port | **USER** drücken; unter Windows „MIDIIN2/MIDIOUT2 (Ableton Push)“ in der DAW ausschalten ([PUSH.md](PUSH.md#einrichtung)) |
| **Controller reagiert nicht** | MIDI erreicht das Plug-in nicht | MIDI-Routing prüfen (Abschnitt 6) oder das MIDI-Mapping der DAW verwenden |
| **Link zeigt „HOST-SYNC ZUERST“** | in einer DAW hat HOST-SYNC Vorrang vor Link | HOST-SYNC ausschalten oder Link in der DAW einschalten und HOST-SYNC anlassen (Abschnitt 8) |

Keine DAW zur Hand? Die **Standalone-App** funktioniert ohne – siehe [INSTALLATION.md](INSTALLATION.md).

## 8. Tempo mit Ableton Link (seit 1.5)

Es gibt zwei Wege, mit anderen Link-Apps im Takt zu spielen (ein zweiter Laptop, ein iPad, eine Drumcomputer-App):

- **Empfohlen in einer DAW**: Link in der **DAW** einschalten (z. B. Ableton Live: *Einstellungen → Link, Tempo & MIDI → Link-Schalter anzeigen*, dann der *Link*-Knopf) und **HOST-SYNC** in livelooper anlassen. livelooper folgt der DAW, die DAW folgt Link.
- **Das eigene Link von livelooper**: *SETUP → Ableton Link* – in einer DAW nur mit **HOST-SYNC aus** (sonst hat der Host Vorrang und das Display zeigt *LINK: HOST-SYNC ZUERST*). Das ist der Weg für die **Standalone-App**, in der Link den Takt von livelooper direkt vorgibt.

Start / Stopp synchronisieren ist optional (*SETUP → Ableton Link → Start / Stopp mit Link synchronisieren*). Das Beitreten ändert nie das Tempo einer Session.


<!-- v1.6 features -->
## 9. Aufnehmen, Clips und Grid-Controller in der DAW (seit 1.6)

- **Performance-Recorder**: Die runde rote Aufnahmetaste oben rechts (unter MIDI LEARN … ?) schreibt die Ausgabe des Loopers (was das Plug-in an seinem Hauptausgang verlässt) als WAV-Datei nach *Musik/livelooper/Recordings* – unabhängig von Transport und Aufnahme deiner DAW. Über den Parameter *Performance-Recorder* auf einen Fußschalter gelegt, startest und stoppst du die Aufnahme freihändig. Direkte Spurausgänge sind nicht Teil der Aufnahme, nur der Hauptausgang.
- **Clip-Export 9:16**: livelooper hält auch in der DAW die letzten 30 Sekunden seiner Ausgabe im Speicher – *SETUP → Clip-Export 9:16* macht aus den letzten 20 Sekunden ein Hochformat-Video.
- **Grid-Controller** (Launchpad, APC mini, Launchkey-Pads – siehe [Bedienungsanleitung, Abschnitt 38](BEDIENUNGSANLEITUNG.md#38-controller-vorlagen-neu-in-16)): livelooper öffnet den MIDI-Port des Controllers selbst, wie bei Push. Lass diesen Port in der DAW unbenutzt (keine Bedienoberfläche / kein Spur-Eingang darauf). Unter **Windows** kann ein Port nur von einem Programm geöffnet werden – schalte ihn in den MIDI-Einstellungen der DAW aus, sonst zeigt livelooper *Port belegt*. Bei mehreren livelooper-Instanzen steuert nur die erste den Controller.
- **Sets**: Eine `.livelooper`-Datei enthält einen kompletten Looper mit allem Audio – zieh sie auf das Plug-in-Fenster, um sie in diese Instanz zu laden (dein DAW-Projekt speichert sie dann wie gewohnt).
<!-- end v1.6 features -->
