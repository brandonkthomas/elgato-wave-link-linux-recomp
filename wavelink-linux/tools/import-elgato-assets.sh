#!/usr/bin/env bash
# Copy Elgato's fonts, images and English UI strings from your own Wave Link installer into the place
# the app looks for them. They are Elgato's property: keep them out of anything you publish.
#   tools/import-elgato-assets.sh <WaveLink-Rev dir> [destination]
# <WaveLink-Rev dir> must contain extracted/win (the unpacked MSIX) and work/xaml/strings/en-US.json.
set -euo pipefail
SRC=${1:?usage: import-elgato-assets.sh <WaveLink-Rev dir> [destination]}
DEST=${2:-${XDG_DATA_HOME:-$HOME/.local/share}/wavelink-linux/elgato}
WIN="$SRC/extracted/win"
mkdir -p "$DEST/fonts" "$DEST/Assets" "$DEST/strings"
cp "$WIN/Elgato.BaseClasses.WinUI/Assets/Fonts/Elgato-Icons.ttf" "$DEST/fonts/Elgato-Icons.ttf"
cp "$WIN/Elgato.BaseClasses.WinUI/Assets/Fonts/Univers%20Next%20Pro.ttc" "$DEST/fonts/UniversNextPro.ttc"
cp -R "$WIN/Assets/." "$DEST/Assets/"
cp "$SRC/work/xaml/strings/en-US.json" "$DEST/strings/en-US.json"
echo "Elgato assets installed in $DEST"
