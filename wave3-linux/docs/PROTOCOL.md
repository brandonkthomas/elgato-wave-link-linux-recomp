# Elgato Wave:3 control protocol

Reconstructed from Elgato Wave Link 3.4 (macOS 3.4.0.3281, Windows 3.4.0.4650) with Ghidra 12.1.4
and REA, then checked against a Wave:3 (firmware 1.2.2, control API 5.3).

## Inputs

| File | SHA-256 |
|---|---|
| `ElgatoWaveLink-3.4.0.3281.dmg` | `49f15d4a6565885631485b69f0c7f4bbdc8856d11af479a18555d30a1e548a1f` |
| `Elgato.WaveLink_3.4.0.4650_x64.msix` | `6f64d51867039cdf3354508c91db4e7c1bf899dd23a5c19ae970386080dbf615` |
| `WaveAPI.framework/Versions/A/WaveAPI` (universal) | `025307fe30ef4c0e963ed73dcf9b6e5fe32c934905eab43f69a367919b782386` |
| `WaveAPI` arm64 slice (analyzed) | `d01f288b4b2336a2a7368eb984d7e60709aff5ab615ae3416666fe5052f1b3d9` |
| `waveapi.dll` (Windows) | `3c236a0d7504067e4d070cb31657c0f09db9f8f5ebbd6f17d68950efa79a21f1` |
| `EWLWAudioEngine.dll` (Windows) | `2598f191546ef40e66e76b567bb8b72edd58dc189ca91b7390252b2e9bf4200d` |

Device control lives in the shared C++ library `WaveAPI` (namespace `LWT`, source tree
`ElgatoDeviceKit/lewittaudio-waveapi`). The macOS build keeps C++ symbols, so addresses below
refer to its arm64 slice. The Windows app (.NET/WinUI) reaches the same library through SWIG
bindings in `EWLWAudioEngine.dll`.

## Transport

| Claim | Evidence |
|---|---|
| Wave:3 is `0fd9:0070` (`0fd9:0071` in DFU mode) | `__GLOBAL__sub_I_Wave3Device.cpp` stores `0x700fd9`/`0x710fd9` into the Wave:3 `LewittDeviceConf` ([evidence/wave3_vidpid.c](evidence/wave3_vidpid.c)); `waveapi.dll` and `EWLWAudioEngine.dll` load the immediate `0x00700fd9` |
| Wave:3 uses the "LegacyUAC1" backend | `Wave3Device::open` @`0x7b7b4c` calls `LegacyWaveDevice::open` @`0x3e4ec0`, which calls `VendorUSBLewittDeviceBackend::setBackendType(2)` @`0x3e4f14`; type 2 logs `"LegacyUAC1 backend"` |
| Reads are UAC1 `GET_MEM` | `LegacyUAC1VendorUSBBackendStrategy::read` @`0x6644`: `inCtrlRequest(vendor=false, bRequest=0x85, wValue=msg.messageId(), wIndex=0x3300, msg.data(), msg.size())` |
| Writes are UAC1 `SET_MEM` | `LegacyUAC1VendorUSBBackendStrategy::write` @`0x66c8`: `outCtrlRequest(false, 0x05, msg.messageId(), 0x3300, ...)` |
| API version query | `LegacyUAC1VendorUSBBackendStrategy::protoVersion` @`0x65e0`: `GET_MEM` `wValue=10`, 2 bytes → major, minor |
| Class request, recipient interface, 500 ms | `LewittDeviceThesyconMacBackend::inCtrlRequest` @`0x809730` / `outCtrlRequest` @`0x8095b0` call `UsbDevice::ClassRequestIn/Out(recipient=1, ..., wIndex = (index & 0xff00) \| interfaceNumber, timeout=500)`. Windows does the same through `TUSBAUDIO_ClassVendorRequestOut` ([evidence/windows_EWLWLegacyThesyconBackend_outCtrlRequest.c](evidence/windows_EWLWLegacyThesyconBackend_outCtrlRequest.c)) |

So every message is a control transfer:

```
read : bmRequestType 0xA1, bRequest 0x85 (GET_MEM), wValue = message id, wIndex = 0x33<<8 | interface
write: bmRequestType 0x21, bRequest 0x05 (SET_MEM), wValue = message id, wIndex = 0x33<<8 | interface
```

Entity `0x33` appears nowhere in the USB descriptors (the audio function only declares IDs 1–6);
the firmware handles it as a private memory window.

Other strategies in the same library, not used by the Wave:3: `LegacyUAC2` (`bRequest 3`, UAC2
`MEM`) and `MK2` (vendor request `bRequest 1`, `wIndex = apiMajor << 8`) for newer devices.

## Messages and versions

Each message is a `PackedStructView`. Its 4-byte header is decoded by the accessors
`Message::messageId` @`0x42ffc`, `isReadable` @`0x43008`, `isWritable` @`0x43014` and
`apiMajorVersion` @`0x43020` ([evidence/imessage_accessors.s](evidence/imessage_accessors.s)):

| Message | Header bytes (@`0x8b8980..`) | id | Size (5.2 / 5.3 / 5.4) | Access |
|---|---|---|---|---|
| `/config` | `00 01 01 05` | 0 | 14 / 16 / 16 | read/write |
| `/status` | `01 01 00 05` | 1 | 8 | read |
| `/version` | `0a 01 00 05` | 10 | 52 / 52 / 54 | read |

`Wave3::known_apis` @`0x8dfd50` maps API 5.4 → table @`0x8fb670`, 5.3 → @`0x8fb730`,
5.2 → @`0x8fb7f0`. `BaseWaveDevice::Impl::lookupSessionAPI` @`0x3e3330` picks the entry with the
same major version and the highest minor version not above the device's, else falls back to 1.0
([evidence/lookupSessionAPI.c](evidence/lookupSessionAPI.c)).

Wave Link changes settings by reading the message, copying only the changed fields over it and
writing the whole message back (`SessionAPI::Impl::tick` @`0x41040`,
[evidence/SessionAPI_tick.c](evidence/SessionAPI_tick.c)). `wave3ctl` does the same.

### `/config` (id 0)

Field descriptors come from the static initializer `__GLOBAL__sub_I_Wave3Device.cpp` @`0x7b7c64`
([evidence/wave3_config_table.c](evidence/wave3_config_table.c)). Each field stores
`offset | size << 16`, a type code (`q` fixed-point, `b` bool, `u` unsigned, `c` chars), the
number of fraction bits, and optional min/max/step limits as floats.

| Offset | Size | Key | Type | Range | Description (from the binary) |
|---|---|---|---|---|---|
| 0 | 2 | `input_gain` | Q8.8 signed | 0..40 dB, step 0.5 | Microphone gain in dB |
| 2 | 2 | — | — | — | not described; observed `00 ec`; preserved |
| 4 | 1 | `input_mute` | bool | | Microphone mute |
| 5 | 1 | `clipguard_enable` | bool | | Clipguard enable |
| 6 | 1 | `lowcut_enable` | bool | | Low-cut filter enable |
| 7 | 2 | `headphone_volume` | Q8.8 signed | −60..0 dB, step 0.5 | Headphone output in dB |
| 9 | 1 | `headphone_mute` | bool | | Headphone output mute |
| 10 | 2 | `direct_monitor` | Q8.8 signed | 0..100 %, step 5 | Direct monitoring/playback mix in percent |
| 12 | 1 | `volume_select` | u8 enum | MIC=1, HEADPHONE=2, MIX=3 | Knob control select |
| 13 | 1 | `all_leds_off` | bool | | All LEDs are off |
| 14 | 1 | `leds_flip` | bool | | Reverse the direction of LEDs and knob (mic upside down) — 5.3+ |
| 15 | 1 | `gain_lock` | bool | | Ignore input volume change (SET_CUR) requests sent from the OS — 5.3+ |

### `/status` (id 1)

`touch_pressed_ms` u32 @0, `touch_signal` u32 @4.

### `/version` (id 10)

`api_version/major` @0, `/minor` @1, `vbus_voltage` Q8.8 @2, `mic_type` @4,
`hw_version_board` @5, `bl/major|minor|patch` @6–8, `bl/build` u32 @9, `bl/commit` char[8] @13,
`fw/major|minor|patch` @21–23, `fw/build` u32 @24, `fw/commit` char[8] @28, `serial` char[12]
@36 (char[14] in 5.4), then `touch_signal_thr/lower` and `/upper` (u16 each).

## Live verification (Wave:3 FW 1.2.2, API 5.3)

| Check | Result |
|---|---|
| `GET_MEM 10`, 2 bytes | `05 03` → API 5.3 |
| `/version` decode | FW `1.2.2` = USB `bcdDevice 0x0122`; serial (shown as `BS0000000000` in this repository) = USB iSerial; commit `0b4251ee` |
| `/config` decode | gain 20 dB, mute on, Clipguard on, low-cut off, headphones −21.5 dB, knob = headphone |
| Writes | gain 0/40/22.5, headphones −60/0, mix 0/50/100 %, knob mic/mix/headphone, every bool on/off; each read back as written; untouched bytes unchanged |
| Restore | final `/config` byte-identical to the starting capture |
| Recipient | only `bmRequestType 0xA1/0x21` (class, interface) is accepted; device/other/vendor recipients STALL |
| Interface number | `wIndex` low byte 0, 1, 2, 3 all work; 4 (DFU) times out |

### Standard USB-audio controls mirror part of `/config`

The audio function is UAC1: IT 1 (USB streaming) → FU 5 → OT 2 (headphones) and IT 4 (microphone)
→ FU 6 → OT 3 (USB streaming). Reading the feature units shows:

* FU 6 volume = `input_gain` (range 0..40 dB, res 0.5), FU 6 mute = `input_mute`
* FU 5 volume = `headphone_volume` (range −60..0 dB, res 0.5), FU 5 mute = `headphone_mute`

so ALSA's ordinary mixer controls already move gain and headphone level on Linux. With
`gain_lock` off, a `SET_CUR` of 10 dB on FU 6 set the gain to 10 dB; with `gain_lock` on, a
`SET_CUR` of 30 dB was ignored while `SET_MEM` still changed the gain. Clipguard, low-cut, monitor
mix, knob mode, LED options and gain lock have no standard control and need this protocol.

## Linux access

`snd-usb-audio` binds the audio-control interface (0) and the streaming interfaces (1, 2). usbfs
rejects class requests addressed to an interface the caller has not claimed
(`check_ctrlrecip` → `checkintf` in `drivers/usb/core/devio.c`), and the Wave:3 firmware rejects
any other recipient. Because the firmware accepts any audio-function interface number in
`wIndex`, `wave3ctl` claims the vendor-specific interface 3 (class `0xff`, no kernel driver) and
sends `wIndex = 0x3303`. Audio keeps running. `--detach-audio` remains as a last-resort fallback
that unbinds and resets the device.

## Not covered

* Bytes 2–3 of `/config` (constant `00 ec` = −20.0 in Q8.8 across all tests).
* Firmware update (Thesycon DFU through `tlusbdfuapi`); not reimplemented on purpose.
* Wave Link's software mixer and effects, which run on the computer through Elgato's virtual
  audio drivers, not in the microphone.
