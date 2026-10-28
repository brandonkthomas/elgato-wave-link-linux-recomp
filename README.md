<div align="center">
  <a href="https://github.com/brandonkthomas/elgato-wave-link-linux-recomp">
    <img src="Public/Screenshots/wavelink-linux.png" alt="Wave Link for Linux running on Arch Linux" width="720">
  </a>

  <h3 align="center">Elgato Wave Link for Linux</h3>

  <p align="center">
    Elgato Wave Link recreated for Linux w/ the mixer, Wave:3 controls, Voice Focus, Elgato EQ & Noise Removal.
    <br />
    <br />
    <a href="https://isocpp.org"><img alt="C++" src="https://img.shields.io/badge/C%2B%2B-00599C?logo=cplusplus&logoColor=white"></a>
    <a href="https://www.qt.io"><img alt="Qt" src="https://img.shields.io/badge/Qt-41CD52?logo=qt&logoColor=white"></a>
    <a href="https://pipewire.org"><img alt="PipeWire" src="https://img.shields.io/badge/PipeWire-4A86CF"></a>
  </p>
  
  <p>
    <b>Unofficial</b> and not affiliated with Elgato or Corsair. Elgato, Wave Link and Wave:3 are their trademarks.
  </p>
</div>

## About the Project

Elgato's Wave Link runs only on Windows and macOS. This project recreates Wave Link 3.4 for Linux for the
Elgato Wave:3 using [morluto/rea](https://github.com/morluto/rea) and Claude Opus 5.5. No Wave Link source 
exists, so the project was rebuilt from the decompiled Windows and macOS releases and checked against them.

Tested on Arch w/ KDE Plasma on Wayland, and should run on any x86-64 distribution
with glibc 2.36 or newer and PipeWire.

> [!WARNING]
> Only the **Wave:3** microphone is supported at this time.

### Feature Highlights

- **Mixer**: up to 8 software and 4 hardware channels across 5 mixes (Personal, Stream, Chat and your own), with Wave Link's volume law, mutes, level meters and per-app routing.
- **Virtual devices**: the channels appear as PipeWire outputs apps play into, and each mix as an input apps like OBS or Discord can record.
- **Wave:3 controls**: gain, Gain Lock, Clipguard, Enhanced lowcut, headphone volume, Mic/PC mix and LED direction, sent over USB exactly as Wave Link sends them.
- **Effects**: Voice Focus (DeepFilterNet in place of ai-coustics), Elgato Noise Removal (RNNoise with the plugin's own settings) and a native Elgato EQ with its eight-band graph editor.
- **Sound Check**: record up to 10 seconds of your mic after its effects and play it back.
- **App and tray**: Wave Link's Windows interface rebuilt in Qt Quick, with the tray icon, settings and the per-channel dialog.

## Installation

### Install on Arch, Ubuntu/Pop!_OS 24.04+, Fedora 37+, Mint 22+, Debian 12+

1. Download the [latest release](https://github.com/brandonkthomas/elgato-wave-link-linux-recomp/releases/latest/download/wavelink-linux-x86_64.tar.gz).
2. Download Elgato's [Wave Link installer](https://edge.elgato.com/egc/windows/ewlw/3.4/Stable/Elgato.WaveLink_3.4.0.4650_x64.msix) (`.msix` Windows build v3.4.0 -- leave compressed for now).
3. Unpack and install:
   ```sh
   tar -xzf wavelink-linux-x86_64.tar.gz
   wavelink-linux/install.sh Elgato.WaveLink_3.4.0.4650_x64.msix
   ```
4. Replug the Wave:3 and open **Wave Link** from the app menu.

Run the commands in the folder you downloaded to. Details, updating and building from source:
[wavelink-linux/README.md](wavelink-linux/README.md#install-on-arch-linux). Elgato's fonts, images and
strings are not in this repository or its releases; `install.sh` copies them from your installer.

> [!NOTE]
> Only Arch has been tested at this time. Please open an issue / pull request if your distribution has any issues installing.

### Uninstall

To uninstall: `~/.local/opt/wavelink-linux/uninstall.sh` (add `--purge` to also remove settings and Elgato assets).

### Troubleshooting

If you run into any issues with input / system audio, try restarting the Wave Link service:

```sh
systemctl --user restart wavelinkd
```

More fixes are in the [troubleshooting table](wavelink-linux/README.md#troubleshooting).

## Project Technicals

### Repository Layout

| Path | Contents |
|---|---|
| `wavelink-linux/` | engine (`wavelinkd`), app (`wavelink`), packaging and install script |
| `wave3-linux/` | Wave:3 USB control library and `wave3ctl`; [docs/PROTOCOL.md](wave3-linux/docs/PROTOCOL.md) has the protocol and the reverse-engineering evidence |
| `spec/` | behavior and architecture notes taken from the decompiled Wave Link |
| `tools/` | analysis helpers: headless Ghidra scripts, libusb probes, an AU host for the Elgato plugins, the XBF→XAML converter |

Not in this repository, and never to be committed: Elgato's installers, plugins, fonts, images and
strings, and the decompiled Wave Link code (`wavelink-linux/tools/import-elgato-assets.sh` copies the
assets from your installer).

### Build and Test

Run these from the repository root.

#### Linux (native)

```sh
# Arch packages; other distros need the same libraries under their own names
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-declarative qt6-websockets qt6-svg \
  qt6-wayland pipewire pipewire-audio wireplumber libusb python
cmake -S wavelink-linux -B wavelink-linux/build-linux -G Ninja
cmake --build wavelink-linux/build-linux
ctest --test-dir wavelink-linux/build-linux

# Integration test: starts its own PipeWire session; the plugins enable its effect checks (x86-64 only)
wavelink-linux/packaging/fetch-plugins.sh wavelink-linux/build-plugins/x86_64
wavelink-linux/engine/tests/integration.sh wavelink-linux/build-linux
```

#### Docker (any host)

```sh
# Development image (Debian 13, Qt 6.8, PipeWire 1.4), then unit and integration tests
docker build -t wavelink-dev:trixie -f wavelink-linux/docker/dev.Dockerfile wavelink-linux/docker
docker run --rm -v "$PWD:/w" -w /w/wavelink-linux wavelink-dev:trixie bash -c \
  'cmake -S . -B build-docker -G Ninja && cmake --build build-docker && ctest --test-dir build-docker && engine/tests/integration.sh build-docker'

# x86-64 release bundle: wavelink-linux/dist/wavelink-linux-x86_64.tar.gz
# (.github/workflows/release.yml builds and publishes it when a v* tag is pushed)
docker buildx build --platform linux/amd64 -f wavelink-linux/packaging/release.Dockerfile \
  --target export --output type=local,dest=wavelink-linux/dist .
```

#### macOS (UI work only)

The engine needs PipeWire, so on macOS only the app builds. It connects to an engine playing test tones
in the dev image.

```sh
brew install qt cmake ninja
cmake -S wavelink-linux -B wavelink-linux/build-mac -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build wavelink-linux/build-mac

docker run -d --name wavelink-demo -p 127.0.0.1:1884:1884 -v "$PWD:/w" -w /w/wavelink-linux wavelink-dev:trixie \
  bash -c 'cmake -S . -B build-docker -G Ninja && cmake --build build-docker && engine/tests/demo.sh build-docker'
wavelink-linux/build-mac/app/wavelink
```

The integration test and demo load the effect plugins from `wavelink-linux/build-plugins/<arch>/`, matching
the machine or container they run on (`WAVELINK_PLUGIN_DIR` overrides), and skip the effect checks without
them. `fetch-plugins.sh` provides the x86-64 builds. DeepFilterNet and RNNoise publish no other Linux
builds, so on Apple Silicon (aarch64 containers) build them from their sources.

## Disclaimer

An independent reimplementation, not affiliated with or endorsed by Elgato or Corsair. Elgato, Wave Link
and Wave:3 are their trademarks, and the screenshot shows Wave Link's design, which belongs to Elgato.
This repository contains no Elgato code, installers, fonts or images: the app loads those from your own
Wave Link installer. Third-party parts keep their licenses: DeepFilterNet (MIT/Apache-2.0), the RNNoise
LADSPA plugin (GPL-3.0, downloaded at build time and shipped unmodified in releases), Qt (LGPL-3.0,
bundled in releases), the LADSPA header (LGPL), PriTools (Apache-2.0) and Microsoft's XBF parser and WinUI
theme values (MIT). Release bundles list them in
[licenses/THIRD-PARTY.md](wavelink-linux/packaging/licenses/THIRD-PARTY.md).
