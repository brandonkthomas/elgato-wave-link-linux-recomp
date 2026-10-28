# Third-party components in this bundle

| Component | Where | License | Source |
| --- | --- | --- | --- |
| Qt 6.8.3 (Core, Gui, Widgets, Qml, Quick, QuickControls2, Network, WebSockets, Svg, Wayland, ...) | `lib/`, `plugins/`, `qml/` (shared libraries, unmodified) | LGPL-3.0 (`LGPL-3.0.txt`, `GPL-3.0.txt`) | https://download.qt.io/archive/qt/6.8/6.8.3/single/ |
| DeepFilterNet LADSPA plugin 0.5.6 | `lib/wavelink/plugins/libdeep_filter_ladspa.so` (unmodified) | MIT or Apache-2.0 (`DeepFilterNet-LICENSE-*.txt`) | https://github.com/Rikorose/DeepFilterNet/tree/v0.5.6 |
| noise-suppression-for-voice (RNNoise LADSPA) 1.10 | `lib/wavelink/plugins/librnnoise_ladspa.so` (unmodified) | GPL-3.0 (`GPL-3.0.txt`) | https://github.com/werman/noise-suppression-for-voice/tree/v1.10 |
| LADSPA header 1.1 | compiled into `bin/wavelinkd` | LGPL-2.1 (`LGPL-2.1.txt`) | https://www.ladspa.org |
| PriTools (PriFormat) | compiled into `tools/pridump` | Apache-2.0 (`PriTools-LICENSE.txt`) | https://github.com/chausner/PriTools/tree/765d4544fd48aaaebf848b5b1b24ade2cf1770a0 |
| .NET runtime 10 | self-contained in `tools/pridump` | MIT (`dotnet-runtime-LICENSE.txt`) | https://github.com/dotnet/runtime |
| WinUI theme values | compiled into `bin/wavelink` | MIT (`microsoft-ui-xaml-LICENSE.txt`) | https://github.com/microsoft/microsoft-ui-xaml |

The Qt libraries are dynamically linked; replace them in `lib/` to use a modified Qt. The bundle contains
no Elgato code, fonts, images or strings: `tools/import-elgato-assets.sh` copies those from your own
Wave Link installer at install time.
