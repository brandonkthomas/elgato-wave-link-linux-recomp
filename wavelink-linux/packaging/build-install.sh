#!/usr/bin/env bash
# Build Wave Link for Linux from this checkout against the system's Qt 6, PipeWire and libusb, then
# install it for the current user with install.sh. For distros that ship Qt 6.5 or newer (Arch); the
# Docker release bundle in release.Dockerfile is for the rest.
#   wavelink-linux/packaging/build-install.sh [Elgato.WaveLink_*.msix, or wavelink-elgato-assets.tar.gz]
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/.." && pwd)
BUILD=${BUILD:-$SRC/build-release}
STAGE="$BUILD/stage"

cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/
cmake --build "$BUILD"
rm -rf "$STAGE"
DESTDIR="$STAGE" cmake --install "$BUILD"

# Voice Focus and Noise Removal plugins, cached in build-plugins/x86_64 (also used by the engine tests)
"$HERE/fetch-plugins.sh" "$SRC/build-plugins/x86_64"
mkdir -p "$STAGE/lib/wavelink/plugins"
cp "$SRC"/build-plugins/x86_64/*.so "$STAGE/lib/wavelink/plugins/"

cp "$SRC/README.md" "$HERE/install.sh" "$HERE/uninstall.sh" "$HERE/wavelinkd.service" "$SRC/../wave3-linux/packaging/70-elgato-wave3.rules" "$STAGE/"
cp -r "$HERE/desktop" "$STAGE/desktop"
IMPORT="$SRC/tools/import-elgato-assets.sh" exec "$STAGE/install.sh" "$@"
