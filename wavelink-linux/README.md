# Wave Link for Linux

A Linux reimplementation of Elgato Wave Link 3.4 for the Elgato Wave:3: the mixer (channels × mixes),
the Wave:3 device page (gain, Clipguard, Gain Lock, monitoring, LED direction, Sound Check), effects
(Voice Focus via DeepFilterNet, Elgato Noise Removal via RNNoise, a native Elgato EQ) and the tray icon.

- `wavelinkd`: the audio engine (PipeWire filter + virtual devices, Wave:3 USB control), a systemd user service.
- `wavelink`: the Qt Quick app, talking to the engine over a local WebSocket (`ws://127.0.0.1:1884`).

## Install on Arch Linux

Works the same on any x86-64 distro with glibc 2.36 or newer and PipeWire.

You also need Elgato's Wave Link **Windows** installer,
[`Elgato.WaveLink_3.4.0.4650_x64.msix`](https://edge.elgato.com/egc/windows/ewlw/3.4/Stable/Elgato.WaveLink_3.4.0.4650_x64.msix) (about 200 MB; the
version this project was built against), downloaded from Elgato. Keep the `.msix` as it is. The install
script reads Elgato's fonts, images and text from it, because this project can't distribute them.

1. Download the latest release and the Wave Link installer, and unpack the release:

   ```sh
   curl -LO https://github.com/brandonkthomas/elgato-wave-link-linux-recomp/releases/latest/download/wavelink-linux-x86_64.tar.gz
   curl -LO https://edge.elgato.com/egc/windows/ewlw/3.4/Stable/Elgato.WaveLink_3.4.0.4650_x64.msix
   tar -xzf wavelink-linux-x86_64.tar.gz
   ```

2. Install, pointing at the Wave Link installer:

   ```sh
   wavelink-linux/install.sh Elgato.WaveLink_3.4.0.4650_x64.msix
   ```

3. Unplug and replug the Wave:3, then start **Wave Link** from the application menu.

The script copies Elgato's assets into `~/.local/share/wavelink-linux/elgato`, installs the app to
`~/.local/opt/wavelink-linux`, enables the `wavelinkd` user service, adds the app to the menu and to
autostart, and installs the Wave:3 udev rule with `sudo`. Afterwards you can delete the downloaded files.
To update, repeat steps 1 and 2 with the new release; the `.msix` argument is optional once the assets
are in place.

### Build from source instead

Builds against Arch's own Qt (any distro with Qt 6.5 or newer works, using its package names):

```sh
sudo pacman -S --needed git base-devel cmake ninja qt6-base qt6-declarative qt6-websockets qt6-svg \
  qt6-wayland pipewire libusb dotnet-sdk unzip
git clone https://github.com/brandonkthomas/elgato-wave-link-linux-recomp.git
cd elgato-wave-link-linux-recomp
curl -LO https://edge.elgato.com/egc/windows/ewlw/3.4/Stable/Elgato.WaveLink_3.4.0.4650_x64.msix
wavelink-linux/packaging/build-install.sh Elgato.WaveLink_3.4.0.4650_x64.msix
```

`build-install.sh` builds into `wavelink-linux/build-release`, downloads the DeepFilterNet and RNNoise
plugins, then runs the same install script. Rerun it after `git pull`, and after Arch moves Qt to a new
minor version (6.x).

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
| App shows boxes or missing text | `~/.local/share/wavelink-linux/elgato` has `fonts/`, `Assets/` and `strings/en-US.json`; rerun `install.sh` with your `.msix` |
| Blank or garbled window on NVIDIA | start with `QT_QUICK_BACKEND=software wavelink` |
| No sound after install | Personal Mix output (top right of Mixes) points at your speakers or headphones |

## Development

Build, test and release commands are in the [repository README](../README.md).
