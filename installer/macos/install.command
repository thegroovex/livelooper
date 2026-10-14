#!/bin/bash
# livelooper - simple install script (alternative to the .pkg installer and to copying by hand).
# Double-click it in Finder (if macOS blocks it: System Settings > Privacy & Security >
# Security > Open Anyway, then confirm), or run it in Terminal:  bash install.command
# Installs for the current user, no administrator password needed:
#   livelooper.app        -> ~/Applications
#   livelooper.vst3       -> ~/Library/Audio/Plug-Ins/VST3
#   livelooper.component  -> ~/Library/Audio/Plug-Ins/Components
# and removes the download quarantine, so macOS does not block the files.
set -e
cd "$(dirname "$0")"
install_bundle () {
    local src="$1" dst="$2"
    [ -e "$src" ] || return 0
    mkdir -p "$dst"
    rm -rf "$dst/$(basename "$src")"
    cp -R "$src" "$dst/"
    xattr -dr com.apple.quarantine "$dst/$(basename "$src")" 2>/dev/null || true
    codesign --force --deep --sign - "$dst/$(basename "$src")" 2>/dev/null || true
    echo "installed: $dst/$(basename "$src")"
}
install_bundle "livelooper.app" "$HOME/Applications"
install_bundle "livelooper.vst3" "$HOME/Library/Audio/Plug-Ins/VST3"
install_bundle "livelooper.component" "$HOME/Library/Audio/Plug-Ins/Components"
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo
echo "Done / Fertig. Start livelooper from ~/Applications, rescan plug-ins in your DAW."
