// Built-in effect types:
//   VoiceFocus          Wave Link's Voice Focus (ai-coustics there), here DeepFilterNet 3. One parameter,
//                       "Enhancement" 0..1 (DefaultPluginConstants.VoiceFocus, default 0.9).
//   ElgatoNoiseRemoval  The Elgato Marketplace "Noise Removal" plugin: RNNoise with a VAD noise gate
//                       (werman noise-suppression-for-voice, which that plugin wraps).
//   ElgatoEQ            The Elgato Marketplace "EQ" plugin, eight JUCE biquad bands (native).
//   ladspa:<library>:<label>  any LADSPA plugin.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Effect.hpp"

namespace wl {

struct EffectType {
    std::string type;
    std::string name;
    std::vector<ParamInfo> params;
};

// Effects whose libraries are present in pluginDir.
std::vector<EffectType> availableEffects(const std::string& pluginDir);

// Creates and prepares an effect; nullptr with *error set on failure.
std::shared_ptr<Effect> createEffect(const std::string& type, const std::string& pluginDir, uint32_t sampleRate,
                                     uint32_t maxFrames, std::string* error);

}  // namespace wl
