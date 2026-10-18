#include "Registry.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "ElgatoEq.hpp"
#include "LadspaEffect.hpp"

namespace wl {

namespace {

const char* kDeepFilter = "libdeep_filter_ladspa.so";
const char* kRnNoise = "librnnoise_ladspa.so";

// Voice Focus: Wave Link's single "Enhancement" control mapped onto DeepFilterNet's attenuation limit.
// Wave Link's slider spans 0.7 (Weak) to 1.0 (Strong) (SliderMinimumValue="0.7"); that range maps to a
// 12..100 dB limit so the whole slider is audible (100 = unlimited). Lower values, reachable only
// through the API, fade to 0 dB (untouched).
float attenuationLimit(float enhancement) {
    if (enhancement >= 0.7f) return 12.0f + (enhancement - 0.7f) / 0.3f * 88.0f;
    return enhancement / 0.7f * 12.0f;
}

class VoiceFocus : public Effect {
public:
    explicit VoiceFocus(const std::string& dir)
        : inner_("VoiceFocus", dir + "/" + kDeepFilter, "deep_filter_mono",
                 {{"atten", "Attenuation Limit (dB)", "", "dB"}}),
          params_{{"Enhancement", "Enhancement", 0.0f, 1.0f, 0.9f, ""}} {}

    std::string type() const override { return "VoiceFocus"; }
    const std::vector<ParamInfo>& params() const override { return params_; }
    bool prepare(uint32_t rate, uint32_t maxFrames, std::string* error) override {
        if (!inner_.prepare(rate, maxFrames, error)) return false;
        setParam(0, enhancement_);
        return true;
    }
    void process(float* const* ch, uint32_t n, uint32_t frames) override { inner_.process(ch, n, frames); }
    void setParam(int index, float value) override {
        if (index != 0) return;
        enhancement_ = std::clamp(value, 0.0f, 1.0f);
        inner_.setParam(0, attenuationLimit(enhancement_));
    }
    float param(int index) const override { return index == 0 ? enhancement_ : 0.0f; }

private:
    LadspaEffect inner_;
    std::vector<ParamInfo> params_;
    float enhancement_ = 0.9f;
};

// Elgato Noise Removal 1.1.2 (JUCE createParameterLayout): "VAD Threshold" float 0..1 default 0.85,
// "VAD Release" int 0..1000 ms default 250, bypass. processBlock calls RnNoiseCommonPlugin::process with
// the threshold, the release converted to blocks and a retroactive grace of 0.
class NoiseRemoval : public Effect {
public:
    explicit NoiseRemoval(const std::string& dir)
        : inner_("ElgatoNoiseRemoval", dir + "/" + kRnNoise, "noise_suppressor_mono",
                 {{"threshold", "VAD Threshold (%)", "", "%"},
                  {"grace", "VAD Grace Period (ms)", "", "ms"},
                  {"retro", "Retroactive VAD Grace (ms)", "", "ms"}}),
          params_{{"VAD Threshold", "Threshold", 0.0f, 1.0f, 0.85f, ""},
                   {"VAD Release", "Release", 0.0f, 1000.0f, 250.0f, "ms"}} {}

    std::string type() const override { return "ElgatoNoiseRemoval"; }
    const std::vector<ParamInfo>& params() const override { return params_; }
    bool prepare(uint32_t rate, uint32_t maxFrames, std::string* error) override {
        if (!inner_.prepare(rate, maxFrames, error)) return false;
        inner_.setParam(2, 0.0f);
        setParam(0, values_[0]);
        setParam(1, values_[1]);
        return true;
    }
    void process(float* const* ch, uint32_t n, uint32_t frames) override { inner_.process(ch, n, frames); }
    void setParam(int index, float value) override {
        if (index < 0 || index > 1) return;
        values_[index] = std::clamp(value, params_[index].min, params_[index].max);
        // The LADSPA port takes percent and tops out at 99.
        inner_.setParam(index, index == 0 ? std::min(values_[0] * 100.0f, 99.0f) : std::round(values_[1]));
    }
    float param(int index) const override { return index >= 0 && index < 2 ? values_[index] : 0.0f; }

private:
    LadspaEffect inner_;
    std::vector<ParamInfo> params_;
    float values_[2] = {0.85f, 250.0f};
};

std::shared_ptr<Effect> make(const std::string& type, const std::string& dir) {
    if (type == "VoiceFocus") return std::make_shared<VoiceFocus>(dir);
    if (type == "ElgatoNoiseRemoval") return std::make_shared<NoiseRemoval>(dir);
    if (type == "ElgatoEQ") return std::make_shared<ElgatoEq>();
    if (type.rfind("ladspa:", 0) == 0) {
        const auto sep = type.rfind(':');
        if (sep > 7) return std::make_shared<LadspaEffect>(type, type.substr(7, sep - 7), type.substr(sep + 1));
    }
    return nullptr;
}

}  // namespace

std::vector<EffectType> availableEffects(const std::string& dir) {
    std::vector<EffectType> out;
    for (const auto& [type, name, lib] : {std::tuple{"VoiceFocus", "Voice Focus", kDeepFilter},
                                          std::tuple{"ElgatoNoiseRemoval", "Elgato Noise Removal", kRnNoise}}) {
        if (!std::filesystem::exists(dir + "/" + lib)) continue;
        auto fx = make(type, dir);
        out.push_back({type, name, fx ? fx->params() : std::vector<ParamInfo>{}});
    }
    out.push_back({"ElgatoEQ", "Elgato EQ", ElgatoEq().params()});  // native, always available
    return out;
}

std::shared_ptr<Effect> createEffect(const std::string& type, const std::string& dir, uint32_t sampleRate,
                                     uint32_t maxFrames, std::string* error) {
    auto fx = make(type, dir);
    if (!fx) {
        *error = "unknown effect type " + type;
        return nullptr;
    }
    if (!fx->prepare(sampleRate, maxFrames, error)) return nullptr;
    return fx;
}

}  // namespace wl
