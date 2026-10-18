#!/usr/bin/env bash
# Install Wave Link for Linux for the current user (survives SteamOS updates: everything lives in
# $HOME except the udev rule, which goes to /etc/udev/rules.d).
#   ./install.sh [path to wavelink-elgato-assets.tar.gz]
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
PREFIX=${PREFIX:-$HOME/.local/opt/wavelink-linux}
DATA=${XDG_DATA_HOME:-$HOME/.local/share}
CONFIG=${XDG_CONFIG_HOME:-$HOME/.config}
ASSETS="$DATA/wavelink-linux/elgato"

echo "Installing to $PREFIX"
rm -rf "${PREFIX:?}.new"
mkdir -p "${PREFIX:?}.new"
cp -a "$HERE"/bin "$HERE"/lib "$HERE"/plugins "$HERE"/qml "${PREFIX:?}.new"/
[ -f "$HERE/README.md" ] && cp "$HERE/README.md" "${PREFIX:?}.new"/
rm -rf "${PREFIX:?}" && mv "${PREFIX:?}.new" "${PREFIX:?}"
mkdir -p "$HOME/.local/bin"
ln -sf "$PREFIX/bin/wavelink" "$HOME/.local/bin/wavelink"
ln -sf "$PREFIX/bin/wavelinkd" "$HOME/.local/bin/wavelinkd"

if [ "${1:-}" != "" ]; then
  echo "Importing Elgato assets from $1"
  mkdir -p "$ASSETS"
  tar -xzf "$1" -C "$ASSETS"
fi
[ -f "$ASSETS/fonts/Elgato-Icons.ttf" ] || echo "warning: Elgato assets missing; pass wavelink-elgato-assets.tar.gz to this script"

icon="$ASSETS/Assets/StoreLogo.scale-400.png"
mkdir -p "$DATA/applications" "$CONFIG/autostart" "$CONFIG/systemd/user"
sed -e "s#@PREFIX@#$PREFIX#g" -e "s#@ICON@#$icon#g" "$HERE/desktop/wavelink-linux.desktop" > "$DATA/applications/wavelink-linux.desktop"
sed -e "s#@PREFIX@#$PREFIX#g" -e "s#@ICON@#$icon#g" -e "s#^Exec=.*#Exec=$PREFIX/bin/wavelink --hidden#" \
    "$HERE/desktop/wavelink-linux.desktop" > "$CONFIG/autostart/wavelink-linux.desktop"
sed -e "s#@PREFIX@#$PREFIX#g" "$HERE/wavelinkd.service" > "$CONFIG/systemd/user/wavelinkd.service"
systemctl --user daemon-reload
systemctl --user enable --now wavelinkd.service
systemctl --user restart wavelinkd.service

if [ ! -f /etc/udev/rules.d/70-elgato-wave3.rules ]; then
  echo "Installing the Wave:3 udev rule (needs sudo; on SteamOS set a password first with 'passwd')"
  if sudo install -m 0644 "$HERE/70-elgato-wave3.rules" /etc/udev/rules.d/70-elgato-wave3.rules; then
    sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=usb
    echo "Unplug and replug the Wave:3 once."
  else
    echo "Skipped the udev rule; Wave:3 settings stay unavailable until it is installed."
  fi
fi
echo "Done. Start 'Wave Link' from the application menu (engine status: systemctl --user status wavelinkd)."
