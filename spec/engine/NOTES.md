# Wave Link 3.4 engine facts (verified from decompiled C#)

Paths are relative to `work/ilspy/Elgato.WaveLink.AppLogic/`.

## Limits — `Elgato.WaveLink.AppLogic.Services.AudioMixing/AudioMixer.cs`
- `MaxOutputMixes => 5` (l.71), `MaxHardwareInputs => 4` (l.75), `MaxSoftwareInputs => 8` (l.77).

## Model — interfaces in `Services.AudioMixing/IAudioMixer.cs`, `Models.Configuration.*`
- Inputs (`IAudioInputModel`): hardware (Wave mics, external devices) or software (virtual app channels).
  Can be hidden from mixes. Each has a plugin chain (`EWLWFilterConfiguration`: predefined + user,
  position, bypass, parameter-state blob, custom name), a level meter (also an "unducked" meter) and an
  optional sample-recorder output device.
- Mixes (`IAudioMixModel`): name, own virtual device, list of extra output devices, level meter,
  maximizer (bypass flag + integer amount, attenuation readout).
- Input×mix cells (`IAudioInputMixModel`): `Volume` float 0..1, `IsMuted`, active/inactive
  (`ToggleInputOnAudioMix`). Effective volume = cell volume × input volume where applicable
  (`AudioInputMixModel.cs` l.121–127); effective mute = cell mute || input mute.
- Ducking (`IDuckingWrapper`): key channels; per target input "ducks against" set of key inputs;
  per (input, mix) ducking enabled; `DuckingSensitivity {Low, Medium, High}` used for both sensitivity
  and amount; `DuckingSpeed {Slow, Medium, Fast}`.
- Channel boost (`IChannelBoostService`): per input enabled + boost dB.
- Main output device (settable; "Wave Link default").

## Volume law — `Utility/AudioMixUtility.cs`
```
MIN_VOLUME_DB = -60, MAX_VOLUME_DB = 0
MapToMixerRange(v)   = 60 * clamp(v, 0, 1) - 60                 // v = slider 0..1
ApplyMixerSkewToDb(d) = n = ((d + 70) / 70)^5; n < 1e-5 ? -60 : max(20*log10(n), -60)
engine gain (dB)     = ApplyMixerSkewToDb(MapToMixerRange(v))   // v=1 -> 0 dB, v=0.5 -> -24.3 dB, v=0 -> -60 dB
meter               = MapLinearToNormalized(peak, minDb) = (clamp(20*log10(peak), minDb, 0) - minDb) / -minDb, 0 if peak <= 1e-6
ApplyVolumeScaling(x, curve) = x ^ (ln(curve.OutputPoint) / ln(curve.InputPoint))
```
Muting sends `EnableInput(mix, input) = false` to the engine; volume 0 alone is -60 dB, not silence.

Which formula reaches the engine depends on the mixer (`AudioInputMixModel.SendVolumeToEngine`, l.130–148):
- Software mixer (Wave:3 and every non-XLR-Pro setup): `SetInputVolume(mix, input) = MapToMixerRange(cell)`,
  i.e. linear-in-dB `60*v - 60`, no skew, no boost.
- Hardware mixer (XLR Pro): `ApplyMixerSkewToDb(MapToMixerRange(effective)) + boostDb (if enabled and > 0) + 100`.
- Deactivating a cell (input removed from a mix) sends -60 dB and `EnableInput = false`.

## Defaults and virtual devices — `Definitions/WaveDeviceDefinitions.cs`, `Services.Configuration/Configuration.cs`
- First run creates mixes `DefaultMixChannelNames = { "Personal Mix", "Stream Mix", "Chat Mix" }` (l.315);
  "Record Mix" and "Aux Mix" exist as the 4th/5th slots.
- First run adds only one software input, "System" (`AddDefaultInputs`, Configuration.cs l.433), with
  `AppLogicSettings.SystemIconId`; the OS default output is switched to the System virtual device.
- The pre-startup default output device is attached to the Personal Mix (`PCM_IN_01_V_00_SD1`).
- Software input slots (virtual playback devices, apps output to them), in slot order:
  System `PCM_OUT_00_V_00_SD1`, Music `_02_SD2`, Browser `_04_SD3`, Voice (Chat) `_06_SD4`, SFX `_08_SD5`,
  Game `_10_SD6`, Aux 1 `_12_SD7`, Aux 2 `_14_SD8`.
- Mic pipeline outputs (processed mic as a device): Wave Mic 1–4 `PCM_OUT_00_V_16_SD9` … `_22_SD12`.
- Mix devices (virtual capture devices, apps record from them): Personal `PCM_IN_01_V_00_SD1`,
  Chat `_02_SD2`, Stream `_04_SD3`, Record `_06_SD4`, Aux `_08_SD5`.
