#!/usr/bin/env bash
# Remove Wave Link for Linux installed by install.sh. Settings, channels and the imported Elgato assets
# are kept unless --purge is given.
#   ~/.local/opt/wavelink-linux/uninstall.sh [--purge]
set -euo pipefail
PREFIX=${PREFIX:-$HOME/.local/opt/wavelink-linux}
DATA=${XDG_DATA_HOME:-$HOME/.local/share}
CONFIG=${XDG_CONFIG_HOME:-$HOME/.config}
PURGE=0
case "${1:-}" in
  "") ;;
  --purge) PURGE=1 ;;
  *) echo "usage: uninstall.sh [--purge]" >&2; exit 2 ;;
esac

echo "Stopping Wave Link"
systemctl --user disable --now wavelinkd.service 2>/dev/null || true
pkill -x wavelink 2>/dev/null || true

echo "Removing $PREFIX"
rm -rf "${PREFIX:?}"
for bin in wavelink wavelinkd; do
  # Only the links install.sh made, not some other wavelink on PATH
  if [ -L "$HOME/.local/bin/$bin" ] && [ "$(readlink "$HOME/.local/bin/$bin")" = "$PREFIX/bin/$bin" ]; then
    rm "$HOME/.local/bin/$bin"
  fi
done
rm -f "$CONFIG/systemd/user/wavelinkd.service" "$CONFIG/autostart/wavelink-linux.desktop" \
  "$DATA/applications/wavelink-linux.desktop"
systemctl --user daemon-reload 2>/dev/null || true

if [ "$PURGE" = 1 ]; then
  echo "Removing settings and Elgato assets"
  rm -rf "$CONFIG/wavelink-linux" "$DATA/wavelink-linux"
else
  echo "Kept settings in $CONFIG/wavelink-linux and Elgato assets in $DATA/wavelink-linux (--purge removes them)"
fi

if [ -f /etc/udev/rules.d/70-elgato-wave3.rules ]; then
  echo "Removing the Wave:3 udev rule (needs sudo)"
  if sudo rm /etc/udev/rules.d/70-elgato-wave3.rules; then
    sudo udevadm control --reload
  else
    echo "Skipped; remove /etc/udev/rules.d/70-elgato-wave3.rules yourself."
  fi
fi
echo "Wave Link for Linux is uninstalled."
