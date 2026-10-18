# Wave Link for Linux

A Linux reimplementation of Elgato Wave Link 3.4 for the Elgato Wave:3: the mixer (channels × mixes),
the Wave:3 device page (gain, Clipguard, Gain Lock, monitoring, LED direction, Sound Check), effects
(Voice Focus via DeepFilterNet, Elgato Noise Removal via RNNoise, a native Elgato EQ) and the tray icon.

- `wavelinkd`: the audio engine (PipeWire filter + virtual devices, Wave:3 USB control), a systemd user service.
- `wavelink`: the Qt Quick app, talking to the engine over a local WebSocket (`ws://127.0.0.1:1884`).

## Install (SteamOS 3 desktop mode, or any x86-64 distro with glibc ≥ 2.36 and PipeWire)

You need two files: `wavelink-linux-x86_64.tar.gz` (the build) and `wavelink-elgato-assets.tar.gz`
(fonts, images and strings taken from your own Wave Link installer; never redistribute it).

```sh
tar -xzf wavelink-linux-x86_64.tar.gz
cd wavelink-linux
./install.sh ../wavelink-elgato-assets.tar.gz
```

The script installs to `~/.local/opt/wavelink-linux`, enables the `wavelinkd` user service, adds the
app to the menu and to autostart, and installs the Wave:3 udev rule with `sudo` (on SteamOS set a
password first with `passwd`). Unplug and replug the Wave:3 once afterwards.

On first start the engine makes **Wave Link System** the default output and plays the Personal Mix on
the device you were using before. Turn that off in Settings → Audio devices → Lock system devices.

## Uninstall

```sh
systemctl --user disable --now wavelinkd.service
rm -rf ~/.local/opt/wavelink-linux ~/.local/bin/wavelink ~/.local/bin/wavelinkd \
  ~/.config/systemd/user/wavelinkd.service ~/.config/autostart/wavelink-linux.desktop \
  ~/.local/share/applications/wavelink-linux.desktop
sudo rm /etc/udev/rules.d/70-elgato-wave3.rules
```

App settings live in `~/.config/wavelink-linux/wavelink-linux.conf`, the engine's channels, mixes and
effects in `~/.config/wavelink-linux/config.json`.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| "Waiting for the Wave Link engine" | `systemctl --user status wavelinkd`, `journalctl --user -u wavelinkd` |
| No Wave:3 page | udev rule installed and the mic replugged; the engine log shows "Wave:3 … connected via" |
| Blank or garbled window on NVIDIA | start with `QT_QUICK_BACKEND=software wavelink` |
| No sound after install | Personal Mix output (top right of Mixes) points at your speakers or headphones |

## Development

Build, test and release commands are in the [repository README](../README.md).
