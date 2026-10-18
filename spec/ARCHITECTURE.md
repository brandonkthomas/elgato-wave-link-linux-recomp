# Wave Link for Linux — architecture

Target: SteamOS 3 Desktop Mode (KDE Plasma 6, Wayland), x86-64, PipeWire >= 1.0. Reference behavior and
UI: Wave Link 3.4.0.4650 for Windows (decompiled C# in `work/ilspy`, XAML in `work/xaml/xaml`, strings in
`work/xaml/strings`). Facts used by the engine are collected in `spec/engine/NOTES.md`.

## Processes
| Process | Role |
|---|---|
| `wavelinkd` | Audio engine. Qt Core + libpipewire. Runs as a systemd user service so mixing also works in Game Mode. Owns the PipeWire graph, DSP, Wave:3 control (libwave3), configuration. |
| `wavelink` | Qt Quick app recreating the Wave Link window, compact views and tray (StatusNotifierItem + quick-mix flyout). Pure client of the engine API. |

## IPC
JSON over a localhost WebSocket (`ws://127.0.0.1:1884`, configurable). Requests
`{"id", "method", "params"}`, responses `{"id", "result"|"error"}`, events `{"event", "data"}` (`state`,
`meters` at 30 Hz). QML and a future Plasma applet can use it without compiled plugins. A Wave Link–
compatible API for Stream Deck tools can be layered on later.

## PipeWire graph
```
app streams ──► [null sink "Wave Link <Channel>"  Audio/Sink] ─monitor─┐
hardware mic ──────────────────────────────────────────────────────────┤
                                                                       ▼
                                  [pw_filter "wavelink.engine": 12 stereo inputs, 5 stereo mix outputs]
                                                                       │
            ┌─────────────────────────────── per mix ─────────────────┤
            ▼                                                          ▼
 [null sink "Wave Link <Mix>" Audio/Source/Virtual]        hardware outputs chosen for the mix
 (OBS, Discord, recorders capture this)                    (e.g. Personal Mix → Wave:3 headphones)
```
- Input slots 0–7 are the software channels (System, Music, Browser, Voice, SFX, Game, Aux 1, Aux 2),
  slots 8–11 hardware inputs; mix slots 0–4 are Personal, Chat, Stream, Record, Aux — matching Wave
  Link's limits (`MaxSoftwareInputs 8`, `MaxHardwareInputs 4`, `MaxOutputMixes 5`).
- All ports are static (preallocated), so the real-time callback never reconfigures; unused slots are
  simply unlinked. The engine creates and reconciles every link itself; the filter has no media.class,
  so WirePlumber leaves it alone. Virtual devices are client-owned (`object.linger=false`) and vanish
  if the engine stops, letting apps fall back to real devices.
- Gains: control thread writes per-cell target gains into atomics; the RT callback ramps linearly to
  them over each block. Meters: RT keeps per-block peaks in atomics (fetch-max); the control thread
  drains them at 30 Hz.
- The System channel becomes the default output (`default.configured.audio.sink` metadata), like Wave
  Link's first run; this is a setting.

## Later phases
Effects per input (Elgato EQ clone, RNNoise Noise Removal, DeepFilterNet Voice Focus, LV2/VST3/CLAP
host), per-mix maximizer, ducking, channel boost, app→channel routing (stream `target.object`
metadata), processed-mic virtual devices (Wave Mic 1–4), Wave Link–compatible WebSocket API, AppImage
packaging with a systemd user unit and udev rule.
