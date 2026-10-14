# Installation

[English](../en/INSTALLATION.md) | **Deutsch**

## Download

Fertige Builds gibt es auf der GitHub-Seite unter **[Releases](https://github.com/thegroovex/livelooper/releases)**:

| Datei | Was es ist |
|---|---|
| `livelooper-macOS.pkg` | **macOS-Installer (empfohlen)**: App, VST3 und Audio Unit in einem Rutsch |
| `livelooper-macOS.zip` | macOS, manuelle Installation (unverändert): App, VST3, AU und `install.command` |
| `livelooper-Windows-Setup.exe` | **Windows-Installer (empfohlen)**: App mit Startmenü-Eintrag und VST3, mit Deinstallation |
| `livelooper-Windows.zip` | Windows, manuelle Installation (unverändert): App und VST3 |

Ein Release entsteht automatisch, wenn ein Versions-Tag (`v1.5.0` …) gepusht wird. Ohne Release: unter **Actions → CI → Run workflow** einen Build starten und die Artefakte herunterladen – oder selbst bauen ([ENTWICKLUNG.md](ENTWICKLUNG.md)).

livelooper ist freie Software und **noch nicht mit einer Apple-Developer-ID oder einem Windows-Codesignatur-Zertifikat signiert**. Deshalb zeigen macOS und Windows beim ersten Mal eine Warnung – mit den Schritten unten kommst du einmalig daran vorbei. Verschwinden werden die Warnungen erst mit einer künftigen Developer-ID-Signatur und Apple-Notarisierung (und einem Windows-Zertifikat).

## macOS

Universal Binary: läuft nativ auf Apple Silicon und Intel.

### A. Installer (empfohlen)

1. `livelooper-macOS.pkg` doppelklicken. macOS blockiert ihn beim ersten Mal („Apple konnte nicht überprüfen …“) – auf **Fertig** klicken (nicht *In den Papierkorb legen*).
2. **Systemeinstellungen → Datenschutz & Sicherheit** öffnen und nach unten zum Abschnitt **Sicherheit** scrollen. Dort steht, dass `livelooper-macOS.pkg` blockiert wurde – auf **Dennoch öffnen** klicken.
3. Mit deinem Passwort oder Touch ID bestätigen und im erneut erscheinenden Dialog auf **Öffnen** klicken.
4. Dem Installer folgen. Unter **Anpassen** kannst du Teile weglassen:
   - `livelooper.app` → `/Programme` (`/Applications`)
   - `livelooper.vst3` → `/Library/Audio/Plug-Ins/VST3`
   - `livelooper.component` → `/Library/Audio/Plug-Ins/Components`
5. Die DAW ihre Plug-ins neu scannen lassen (Schritt „Neu scannen“ unten).

Der Installer entfernt die Download-Quarantäne von den installierten Dateien und frischt den Audio-Unit-Cache auf, damit App und Plug-ins ohne weitere Warnung starten.

### B. ZIP mit install.command

1. `livelooper-macOS.zip` entpacken.
2. `install.command` doppelklicken. Blockiert macOS das Skript, erlaube es wie beim Installer (**Systemeinstellungen → Datenschutz & Sicherheit → Sicherheit → Dennoch öffnen**, mit Passwort / Touch ID bestätigen) und doppelklicke es erneut.
3. Es installiert für dein Benutzerkonto, ohne Administrator-Passwort: die App nach `~/Programme` (`~/Applications`), die Plug-ins nach `~/Library/Audio/Plug-Ins/VST3` und `~/Library/Audio/Plug-Ins/Components`, und entfernt die Quarantäne.

### C. ZIP, von Hand (unverändert)

1. `livelooper-macOS.zip` entpacken.
2. Kopieren:
   - `livelooper.component` → `~/Library/Audio/Plug-Ins/Components/`
   - `livelooper.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - optional `livelooper.app` → `Programme`
3. Die Dateien einmalig erlauben – entweder mit **Dennoch öffnen** (unten) oder im Terminal (entfernt die Download-Quarantäne von der App und beiden Plug-in-Bundles):
   ```bash
   xattr -dr com.apple.quarantine /Applications/livelooper.app
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/livelooper.component
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/livelooper.vst3
   ```

### D. Plug-in weiterhin blockiert? fix-gatekeeper.command

Meldet deine DAW livelooper weiter als blockiert („Apple konnte nicht überprüfen …“) und hilft **Dennoch öffnen** nicht – das passiert bei Plug-ins, die in einem Host laden –, starte das Reparatur-Skript. Es findet App, VST3 und AU im Benutzer- und Systemordner, entfernt die Quarantäne und signiert sie neu (ad hoc):

- im ZIP: `fix-gatekeeper.command`
- nach dem `.pkg`-Installer: `/Library/Application Support/livelooper/fix-gatekeeper.command`

Weil das Skript selbst heruntergeladen ist, startest du es im Terminal: `bash ` (mit Leerzeichen) tippen, die Datei ins Fenster ziehen und Return drücken. Bei Nachfrage dein Mac-Passwort eingeben. Danach die DAW beenden, neu starten und Plug-ins neu scannen (in Ableton Live: **Wahltaste** gedrückt halten und auf **Neu scannen** klicken für einen vollständigen Scan).

Der Installer und `install.command` erledigen das bereits automatisch.

### „livelooper kann nicht geöffnet werden“ – Dennoch öffnen, Schritt für Schritt

Wenn macOS die App blockiert (oder eine DAW das Plug-in als beschädigt / nicht überprüft meldet):

1. `livelooper.app` einmal öffnen. macOS meldet „Apple konnte nicht überprüfen, ob ‚livelooper‘ frei von Schadsoftware ist …“ – auf **Fertig** klicken.
2. **Apple-Menü → Systemeinstellungen → Datenschutz & Sicherheit** öffnen und nach unten zu **Sicherheit** scrollen.
3. Neben dem Hinweis zu livelooper auf **Dennoch öffnen** klicken. (Die Taste erscheint nur etwa eine Stunde nach dem Öffnungsversuch.)
4. Mit deinem Anmeldepasswort oder Touch ID bestätigen, dann auf **Öffnen** klicken.
5. Ab jetzt startet livelooper ganz normal per Doppelklick.

Hinweis: Auf aktuellem macOS (ab Sequoia 15) **umgeht Rechtsklick → Öffnen diese Warnung nicht mehr** – nimm **Dennoch öffnen** in den Systemeinstellungen oder den Terminal-Befehl oben. Für Plug-ins ist der Terminal-Befehl (oder der Installer, der das für dich erledigt) der zuverlässigste Weg.

### Neu scannen in der DAW

Logic Pro und GarageBand nutzen die Audio Unit (`.component`), die meisten anderen Hosts das VST3. In Ableton Live zum Beispiel: *Einstellungen → Plug-ins* → „Audio Units“ bzw. „VST3-Plug-in-Systemordner verwenden“ einschalten und **Neu scannen**.

### Mikrofon-Freigabe (Standalone-App)

Wenn die Standalone-App zum ersten Mal einen Eingang nutzt, fragt macOS nach dem Mikrofonzugriff – auf **Erlauben** klicken. livelooper selbst fragt nur einmal. Jeder Build trägt dieselbe Kennung (`com.groovex.livelooper`); solange livelooper keine Developer-ID-Signatur hat, kann macOS nach einem Update aber erneut fragen. Hast du *Nicht erlauben* geklickt oder bleiben Aufnahmen stumm:

- **Systemeinstellungen → Datenschutz & Sicherheit → Mikrofon** → **livelooper** einschalten, oder
- die Entscheidung im Terminal zurücksetzen und livelooper neu starten (macOS fragt dann erneut):
  ```bash
  tccutil reset Microphone com.groovex.livelooper
  ```

In einer DAW gehört die Mikrofon-Freigabe der DAW, nicht dem Plug-in.

## Windows

### A. Installer (empfohlen)

1. `livelooper-Windows-Setup.exe` doppelklicken.
2. Windows SmartScreen zeigt eventuell „Der Computer wurde durch Windows geschützt“ – auf **Weitere Informationen** klicken, dann auf **Trotzdem ausführen**.
3. Sprache wählen (English / Deutsch), Lizenz (AGPLv3) annehmen und Komponenten wählen:
   - livelooper-App → `C:\Programme\livelooper` (`C:\Program Files\livelooper`) mit Startmenü-Eintrag (optional Desktop-Verknüpfung)
   - VST3-Plug-in → `C:\Program Files\Common Files\VST3\livelooper.vst3`
4. Die DAW ihre VST3-Plug-ins neu scannen lassen.

Deinstallieren über *Einstellungen → Apps → livelooper → Deinstallieren* (oder den Startmenü-Eintrag); dabei werden App und VST3 entfernt.

### B. ZIP, von Hand (unverändert)

1. `livelooper-Windows.zip` entpacken.
2. `livelooper.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren; `livelooper.exe` kannst du ablegen, wo du willst.
3. Warnt SmartScreen beim Start von `livelooper.exe`: **Weitere Informationen → Trotzdem ausführen**.
4. Die DAW ihre VST3-Plug-ins neu scannen lassen (das ist der Standard-VST3-Ordner, den jeder Host durchsucht). In Ableton Live zum Beispiel: *Optionen → Einstellungen → Plug-ins* → „VST3-Plug-in-Systemordner verwenden“ einschalten und **Neu scannen**.

## Einrichtung in deiner DAW

livelooper funktioniert in jedem VST3-/AU-Host gleich. Unten die Kurzfassung; Schritt-für-Schritt-Anleitungen für jede DAW – Software-Instrumente loopen, live zwischen Quellen wechseln, Monitoring ohne doppeltes Signal, direkte Ausgänge und MIDI-Routing – stehen in **[DAW-EINRICHTUNG.md](DAW-EINRICHTUNG.md)**.

1. Eine **Audiospur** anlegen, deren Eingang dein Mikro/Instrument ist (Mono oder Stereo).
2. **Eingangs-Monitoring einschalten**, damit du den Eingang hörst (und er das Plug-in erreicht), auch wenn die Spur nicht aufnahmebereit ist – wie das geht, hängt von deiner DAW ab.
3. **livelooper** als Effekt auf diese Spur setzen. Das Eingangssignal läuft durch die Input-FX in die Loops und – mit dem Regler **EINGANG** – direkt auf den Ausgang. Wer das Direktsignal schon über das Interface hört, dreht **EINGANG** auf 0.
4. Für die **MIDI-Steuerung** das MIDI deines Controllers an livelooper schicken – je nach DAW über den MIDI-Eingang der Spur oder eine MIDI-Spur, die auf das Plug-in geroutet ist – oder die Host-Parameter von livelooper mit dem MIDI-Mapping deiner DAW zuweisen. Ein Ableton Push braucht nichts davon: livelooper spricht ihn direkt an ([PUSH.md](PUSH.md)).
5. Mit **HOST-SYNC** (Standard) folgt livelooper Tempo, Taktart und Transport des Hosts.

### Beispiel: Ableton Live

**Audio-Eingang**

1. Audiospur anlegen, *Audio From* = dein Interface-Eingang (Mono oder Stereo).
2. *Monitor* auf **In** (dauerhaft mithören, auch wenn die Spur nicht scharf ist).
3. livelooper auf die Spur ziehen. Das Eingangssignal läuft durch die Input-FX in die Loops und – mit dem Regler **EINGANG** – direkt auf den Ausgang. Wer das Direktsignal schon über das Interface hört, dreht **EINGANG** auf 0.

**MIDI-Controller / Fußschalter** – zwei Wege, beide gleichzeitig nutzbar:

- *Eigenes MIDI-Learn von livelooper*: eine MIDI-Spur anlegen, *MIDI From* = dein Controller, *Monitor* = **In**, *MIDI To* = die Audiospur mit livelooper → „livelooper". Dann im Plug-in Rechtsklick auf ein Bedienelement → **MIDI Learn**.
- *Live-MIDI-Map* (das MIDI-Mapping der DAW): im Gerät auf **Konfigurieren** klicken, die gewünschten Regler im Plug-in bewegen, dann wie gewohnt mit *MIDI* (Cmd/Strg+M) zuweisen.

Mehr dazu in [MIDI.md](MIDI.md).

**Tempo**: Mit **HOST-SYNC** (Standard) folgt livelooper dem Tempo, der Taktart und der Position von Live. Läuft der Transport nicht, zählt eine interne Clock im Host-Tempo weiter – in Live wie in jeder anderen DAW.

## Standalone-App

Du brauchst gar keine DAW: Die **Standalone-App** (im selben Download) ist livelooper für sich allein – Audio-Interface mit dem Knopf **Audio-/MIDI-Einstellungen** oben links im Fenster wählen, Push anschließen und spielen. Sie bietet zusätzlich einen Cue-Ausgang fürs Metronom und direkte Spurausgänge auf den Ausgängen deines Interfaces (Bedienungsanleitung, Abschnitte 14 und 20). Das Plug-in nimmst du, wenn du die Instrumente, Effekte und Aufnahme deiner DAW drumherum willst ([DAW-EINRICHTUNG.md](DAW-EINRICHTUNG.md)).

## Standalone-App: kein Eingang / Aufnahmen bleiben stumm

Die Standalone-App schaltet den Audio-Eingang standardmäßig stumm, um Rückkopplungen zu vermeiden. Klicke auf **Audio-/MIDI-Einstellungen** (oben links im Fenster) → *Audio-/MIDI-Einstellungen …*, wähle dein Eingangsgerät (z. B. das eingebaute Mikrofon), aktiviere dessen Eingangskanäle und entferne den Haken bei **Mute audio input**. Nimm mit dem eingebauten Mikrofon am besten mit Kopfhörern auf. Unter macOS zusätzlich das Mikrofon erlauben (siehe *Mikrofon-Freigabe* oben).

## Standalone-App: Computertastatur

Die Standalone-App lässt sich mit der Computertastatur spielen (1–5 REC/PLAY, Q–T STOP, Leertaste ALLE START/STOP, Eingabe TAP …). Das livelooper-Fenster muss dafür das aktive Fenster sein. Die Belegung lässt sich unter **SETUP → Tastaturbelegung …** ändern – siehe Bedienungsanleitung, Abschnitt 10.

## Erster Start: Einstellungen und Updates

Beim ersten Start fragt livelooper einmal, ob es mit den **zuletzt verwendeten Einstellungen** oder den **Standard-Einstellungen** starten soll (jederzeit änderbar unter **SETUP → Einstellungen beim Start**). Updates: **SETUP → Updates** oder das Info-Fenster (**SETUP → Über livelooper …**) prüfen auf GitHub, ob es eine neue Version gibt; die App kann sie laden und beim Beenden installieren, Plug-ins werden vorbereitet und bei geschlossener DAW installiert. Geprüft wird nur, wenn du es einschaltest.

## Sprache

livelooper spricht Deutsch und Englisch. Standardmäßig folgt es der Sprache des Betriebssystems; umstellen unter **SETUP → Language / Sprache**. Die Wahl gilt für alle livelooper-Instanzen auf diesem Rechner. Die Parameternamen, die deine DAW anzeigt, wechseln nach dem Neuladen des Plug-ins (z. B. Projekt neu öffnen).

## Systemanforderungen

- macOS 11 oder neuer, Windows 10/11 (64 Bit)
- Ein VST3-Host (Windows, macOS) oder Audio-Unit-Host (macOS), z. B. Ableton Live (10.1 oder neuer, jede Edition), Bitwig Studio, Cubase, Studio One, Reaper, FL Studio, Logic Pro oder GarageBand – oder ganz ohne DAW mit der Standalone-App
- Arbeitsspeicher: pro Spur werden `2 × Max. Loop-Länge` Stereo-Audio reserviert (für Undo). Bei 48 kHz und 60 s (Standard) sind das rund 230 MB für fünf Spuren (acht Spuren: rund 370 MB); einstellbar unter **SETUP → Maximale Loop-Länge**.
