#!/usr/bin/env bash
# Copy Elgato's fonts, images and English UI strings from your own Wave Link installer into the place
# the app looks for them. They are Elgato's property: keep them out of anything you publish.
#   tools/import-elgato-assets.sh <Elgato.WaveLink_*.msix, or the folder it was unzipped to> [destination]
# The strings are read from the installer's .pri files with PriDump: the release bundle ships it next to
# this script; in a source checkout it is built from tools/xaml-extract, which needs git and either the
# .NET 10 SDK (dotnet) or Docker.
set -euo pipefail
SRC=${1:?usage: import-elgato-assets.sh <Elgato.WaveLink_*.msix or unzipped folder> [destination]}
DEST=${2:-${XDG_DATA_HOME:-$HOME/.local/share}/wavelink-linux/elgato}
HERE=$(cd "$(dirname "$0")" && pwd)
XE="$HERE/../../tools/xaml-extract"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

if [ -f "$SRC" ]; then
  echo "Unpacking $SRC"
  mkdir "$TMP/msix"
  if command -v unzip >/dev/null; then unzip -q "$SRC" -d "$TMP/msix"; else bsdtar -xf "$SRC" -C "$TMP/msix"; fi
  WIN="$TMP/msix"
else
  WIN=$(cd "$SRC" && pwd)
fi
[ -f "$WIN/resources.pri" ] || { echo "error: $SRC is not the Wave Link .msix (no resources.pri inside)" >&2; exit 1; }

echo "Extracting UI strings"
mkdir "$TMP/pri"
if [ -x "$HERE/pridump" ]; then
  "$HERE/pridump" "$TMP/pri" "$WIN"/*.pri >/dev/null
else
  XE=$(cd "$XE" && pwd)
  if [ ! -d "$XE/vendor/PriTools" ]; then
    echo "Fetching PriTools"
    git clone -q https://github.com/chausner/PriTools.git "$XE/vendor/PriTools"
    git -C "$XE/vendor/PriTools" checkout -q 765d4544fd48aaaebf848b5b1b24ade2cf1770a0
  fi
  if command -v dotnet >/dev/null; then
    dotnet run -c Release --project "$XE/src/PriDump" -- "$TMP/pri" "$WIN"/*.pri >/dev/null
  else
    docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -e DOTNET_CLI_TELEMETRY_OPTOUT=1 \
      -v "$XE:/xe" -v "$WIN:/msix:ro" -v "$TMP/pri:/out" mcr.microsoft.com/dotnet/sdk:10.0 \
      bash -c 'dotnet run -c Release --project /xe/src/PriDump -- /out /msix/*.pri' >/dev/null
  fi
fi

mkdir -p "$DEST/fonts" "$DEST/Assets" "$DEST/strings"
cp "$WIN/Elgato.BaseClasses.WinUI/Assets/Fonts/Elgato-Icons.ttf" "$DEST/fonts/Elgato-Icons.ttf"
cp "$WIN/Elgato.BaseClasses.WinUI/Assets/Fonts/Univers%20Next%20Pro.ttc" "$DEST/fonts/UniversNextPro.ttc"
cp -R "$WIN/Assets/." "$DEST/Assets/"
cp "$TMP/pri/strings/en-US.json" "$DEST/strings/en-US.json"
echo "Elgato assets installed in $DEST"
