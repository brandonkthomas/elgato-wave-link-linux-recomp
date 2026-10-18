# Elgato Wave Link for Linux

A Linux recreation of Elgato Wave Link 3.4 for the Elgato Wave:3, rebuilt from the Windows and macOS
releases: the PipeWire mixing engine, the Qt Quick app and tray icon, the Wave:3's USB controls, Voice
Focus (DeepFilterNet), Elgato Noise Removal (RNNoise) and a native Elgato EQ.

| Path | Contents |
|---|---|
| `wavelink-linux/` | engine (`wavelinkd`), app (`wavelink`), packaging and install script. Start with its [README](wavelink-linux/README.md) |
| `wave3-linux/` | Wave:3 USB control library and `wave3ctl`; [docs/PROTOCOL.md](wave3-linux/docs/PROTOCOL.md) has the protocol and the reverse-engineering evidence |
| `spec/` | behavior and architecture notes taken from the decompiled Wave Link |
| `tools/` | analysis helpers: headless Ghidra scripts, libusb probes, an AU host for the Elgato plugins, the XBF→XAML converter |

Not in this repository, and never to be committed: Elgato's installers, plugins, fonts, images and
strings, and the decompiled Wave Link code. The app reads the fonts, images and strings from your own
installer at runtime (`wavelink-linux/tools/import-elgato-assets.sh`).

## Build and test (from the repository root)

```sh
# Development image (Debian 13, Qt 6.8, PipeWire 1.4), then engine, unit and integration tests
docker build -t wavelink-dev:trixie -f wavelink-linux/docker/dev.Dockerfile wavelink-linux/docker
docker run --rm -v "$PWD:/w" -w /w/wavelink-linux wavelink-dev:trixie \
  bash -c 'cmake -S . -B build-linux -G Ninja && cmake --build build-linux && ctest --test-dir build-linux'
docker run --rm -v "$PWD:/w" -w /w/wavelink-linux -e WAVELINK_PLUGIN_DIR=/w/wavelink-linux/build-plugins/aarch64 \
  wavelink-dev:trixie bash engine/tests/integration.sh build-linux

# App on macOS for UI work (Homebrew Qt), against an engine in the dev image: engine/tests/demo.sh
cmake -S wavelink-linux -B wavelink-linux/build-mac -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build wavelink-linux/build-mac

# x86-64 release bundle: wavelink-linux/dist/wavelink-linux-x86_64.tar.gz
docker buildx build --platform linux/amd64 -f wavelink-linux/packaging/release.Dockerfile \
  --target export --output type=local,dest=wavelink-linux/dist .
```

The integration test's effect checks need the DeepFilterNet and RNNoise LADSPA plugins for the
container's architecture in `wavelink-linux/build-plugins/<arch>/` (the release build downloads the
x86-64 ones itself).
