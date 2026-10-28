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

# Noise suppression plugins behind Voice Focus (DeepFilterNet, MIT/Apache-2.0) and Elgato Noise Removal
# (noise-suppression-for-voice's RNNoise LADSPA, GPL-3.0), the same releases the Docker bundle uses.
fetch() { # url file sha256
  if ! echo "$3  $2" | sha256sum -c --status 2>/dev/null; then
    curl -sSfL -o "$2" "$1"
    echo "$3  $2" | sha256sum -c --quiet
  fi
}
mkdir -p "$BUILD/downloads" "$STAGE/lib/wavelink/plugins"
fetch https://github.com/Rikorose/DeepFilterNet/releases/download/v0.5.6/libdeep_filter_ladspa-0.5.6-x86_64-unknown-linux-gnu.so \
  "$BUILD/downloads/libdeep_filter_ladspa.so" 2ca3205c2911d389604a826a240e745597d50252b5cab81c8248252b335e2236
fetch https://github.com/werman/noise-suppression-for-voice/releases/download/v1.10/linux-rnnoise.zip \
  "$BUILD/downloads/rnnoise.zip" 811390b6eb6e28dde023c70590c74d26e74ebb2e595bcf4b95af2341db160e99
cp "$BUILD/downloads/libdeep_filter_ladspa.so" "$STAGE/lib/wavelink/plugins/"
bsdtar -xOf "$BUILD/downloads/rnnoise.zip" linux-rnnoise/ladspa/librnnoise_ladspa.so > "$STAGE/lib/wavelink/plugins/librnnoise_ladspa.so"

cp "$SRC/README.md" "$HERE/install.sh" "$HERE/wavelinkd.service" "$SRC/../wave3-linux/packaging/70-elgato-wave3.rules" "$STAGE/"
cp -r "$HERE/desktop" "$STAGE/desktop"
IMPORT="$SRC/tools/import-elgato-assets.sh" exec "$STAGE/install.sh" "$@"
