#!/usr/bin/env bash
# Download the x86-64 noise suppression plugins behind Voice Focus (DeepFilterNet, MIT/Apache-2.0) and
# Elgato Noise Removal (noise-suppression-for-voice's RNNoise LADSPA, GPL-3.0) into <dir>, checked against
# known hashes; the same releases the Docker bundle uses. Neither project publishes other architectures.
#   wavelink-linux/packaging/fetch-plugins.sh <dir>
set -euo pipefail
DIR=${1:?usage: fetch-plugins.sh <dir>}
[ "$(uname -m)" = x86_64 ] || echo "warning: these plugins are x86-64 builds; on $(uname -m) build them from source" >&2
mkdir -p "$DIR/.downloads"

fetch() { # url file sha256
  if ! echo "$3  $2" | sha256sum -c --status 2>/dev/null; then
    curl -sSfL -o "$2" "$1"
    echo "$3  $2" | sha256sum -c --quiet
  fi
}
fetch https://github.com/Rikorose/DeepFilterNet/releases/download/v0.5.6/libdeep_filter_ladspa-0.5.6-x86_64-unknown-linux-gnu.so \
  "$DIR/.downloads/libdeep_filter_ladspa.so" 2ca3205c2911d389604a826a240e745597d50252b5cab81c8248252b335e2236
fetch https://github.com/werman/noise-suppression-for-voice/releases/download/v1.10/linux-rnnoise.zip \
  "$DIR/.downloads/rnnoise.zip" 811390b6eb6e28dde023c70590c74d26e74ebb2e595bcf4b95af2341db160e99
cp "$DIR/.downloads/libdeep_filter_ladspa.so" "$DIR/"
bsdtar -xOf "$DIR/.downloads/rnnoise.zip" linux-rnnoise/ladspa/librnnoise_ladspa.so > "$DIR/librnnoise_ladspa.so"
