#!/bin/bash
# livelooper – Gatekeeper fix / Gatekeeper-Freigabe
# Removes the macOS quarantine flag from installed livelooper plug-ins and the
# standalone app and re-signs them ad hoc, so hosts can load them.
# Entfernt die Quarantäne-Markierung von installierten livelooper-Plug-ins und
# der App und signiert sie neu (ad hoc), damit DAWs sie laden.

echo "livelooper – Gatekeeper fix"
echo "Your Mac password may be required. / Evtl. wird dein Mac-Passwort abgefragt."
echo

PATHS=(
  "$HOME/Library/Audio/Plug-Ins/VST3/livelooper.vst3"
  "/Library/Audio/Plug-Ins/VST3/livelooper.vst3"
  "$HOME/Library/Audio/Plug-Ins/Components/livelooper.component"
  "/Library/Audio/Plug-Ins/Components/livelooper.component"
  "/Applications/livelooper.app"
  "$HOME/Applications/livelooper.app"
)

found=0
for P in "${PATHS[@]}"; do
  if [ -e "$P" ]; then
    found=1
    echo "-> $P"
    sudo xattr -dr com.apple.quarantine "$P"
    sudo codesign --force --deep --sign - "$P" && echo "   OK"
  fi
done

if [ $found -eq 0 ]; then
  echo "No livelooper installation found. / Keine livelooper-Installation gefunden."
else
  echo
  echo "Done. Restart your DAW and rescan plug-ins."
  echo "Fertig. DAW neu starten und Plug-ins neu scannen"
  echo "(Ableton Live: Settings → Plug-Ins → hold Option/Wahltaste + Rescan)."
fi
echo
read -n 1 -s -r -p "Press any key to close / Taste drücken zum Schließen"
echo
