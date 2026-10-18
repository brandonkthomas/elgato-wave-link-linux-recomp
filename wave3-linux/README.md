# wave3-linux

Linux (and macOS) control for the Elgato Wave:3 microphone: gain, mute, Clipguard, low-cut,
headphone level, monitor mix, knob function, LED options and gain lock. It is a clean
reimplementation of the protocol Elgato Wave Link uses; see [docs/PROTOCOL.md](docs/PROTOCOL.md)
for the reverse-engineering evidence.

This library is the device-control part of the full Wave Link reimplementation in
[`../wavelink-linux`](../wavelink-linux) (no Wave Link source ships, so the app, mixer and effects
were rebuilt from scratch rather than recompiled). On its own, `wave3ctl` reaches the settings that
only Wave Link could change; the microphone's audio already works as a standard USB audio device.

## Build

Needs a C++17 compiler, CMake ≥ 3.16, pkg-config and libusb-1.0.

```sh
sudo apt install g++ cmake pkg-config libusb-1.0-0-dev     # Debian/Ubuntu
sudo dnf install gcc-c++ cmake pkgconf libusb1-devel       # Fedora
sudo pacman -S base-devel cmake libusb                     # Arch

cmake -S . -B build && cmake --build build -j && ctest --test-dir build
sudo cmake --install build        # wave3ctl + udev rule
sudo udevadm control --reload && sudo udevadm trigger
```

Without installing, copy `packaging/70-elgato-wave3.rules` to `/etc/udev/rules.d/` so your user
can open the device, then replug the mic.

A prebuilt x86-64 binary is reproducible with Docker:

```sh
docker buildx build --platform linux/amd64 -f packaging/Dockerfile \
  --target export --output type=local,dest=dist/linux-x86_64 .
```

## Use

```sh
wave3ctl info                      # firmware, serial, API version, how the device is reached
wave3ctl get                       # all settings
wave3ctl set gain=22.5 lowcut=on   # several settings, one write, read back and checked
wave3ctl set clipguard=on mix=50 knob=mix
wave3ctl set gain_lock=on          # stop ALSA/PipeWire from changing the gain
wave3ctl watch                     # print changes made with the knob or touch-mute
wave3ctl --json get                # for scripts
wave3ctl schema                    # every key with its range
```

Keys: `gain` (0–40 dB), `mute`, `clipguard`, `lowcut`, `headphone` (−60–0 dB),
`headphone_mute`, `mix` (0–100 %), `knob` (`mic`/`headphone`/`mix`), `all_leds_off`,
`leds_flip`, `gain_lock`. Booleans take `on`/`off`.

## Linux notes

* Audio is not interrupted. `snd-usb-audio` keeps the audio interfaces; `wave3ctl` talks through
  the mic's unused vendor interface (`wave3ctl -v info` shows the route).
* Gain resetting after reboot: the mic's gain is also the ALSA "Mic" capture volume, and ALSA or
  PipeWire restore that volume at boot. `wave3ctl set gain_lock=on` makes the mic ignore it.
* Headphone level and mic mute are also plain ALSA controls (`alsamixer`, your desktop's sound
  settings).
* If `wave3ctl` reports no free interface, `--detach-audio` unbinds the audio driver for the
  duration of the command and resets the mic afterwards.

## Layout

| Path | Contents |
|---|---|
| `include/wave3/`, `src/` | protocol tables, codec, libusb transport, device API |
| `tools/wave3ctl.cpp` | CLI |
| `tests/test_protocol.cpp` | tests against messages captured from a real Wave:3 |
| `docs/PROTOCOL.md`, `docs/evidence/` | protocol description, decompiler excerpts, REA evidence bundle |
| `packaging/` | udev rule, Linux x86-64 Dockerfile |
