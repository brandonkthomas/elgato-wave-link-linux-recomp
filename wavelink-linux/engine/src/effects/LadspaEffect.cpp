#include "LadspaEffect.hpp"

#include <dlfcn.h>
#include <ladspa.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wl {

struct LadspaEffect::Instance {
    const LADSPA_Descriptor* desc;
    LADSPA_Handle handle;
    ~Instance() {
        if (desc->deactivate) desc->deactivate(handle);
        desc->cleanup(handle);
    }
};

LadspaEffect::LadspaEffect(std::string type, std::string library, std::string label, std::vector<PortMap> exposed)
    : type_(std::move(type)), library_(std::move(library)), label_(std::move(label)), exposed_(std::move(exposed)) {
    handle_ = dlopen(library_.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_) return;
    auto fn = reinterpret_cast<LADSPA_Descriptor_Function>(dlsym(handle_, "ladspa_descriptor"));
    for (unsigned long i = 0; fn; i++) {
        const LADSPA_Descriptor* d = fn(i);
        if (!d) break;
        if (label_ == d->Label) {
            desc_ = d;
            break;
        }
    }
    if (!desc_) return;
    for (unsigned long p = 0; p < desc_->PortCount; p++) {
        const auto pd = desc_->PortDescriptors[p];
        if (LADSPA_IS_PORT_AUDIO(pd)) (LADSPA_IS_PORT_INPUT(pd) ? audioIn_ : audioOut_).push_back(p);
        else (LADSPA_IS_PORT_INPUT(pd) ? controlIn_ : controlOut_).push_back(p);
    }
    controls_ = std::make_unique<float[]>(controlIn_.size());
    controlOuts_ = std::make_unique<float[]>(controlOut_.size() + 1);
    for (size_t c = 0; c < controlIn_.size(); c++) {
        const unsigned long p = controlIn_[c];
        const LADSPA_PortRangeHint& h = desc_->PortRangeHints[p];
        float lo = LADSPA_IS_HINT_BOUNDED_BELOW(h.HintDescriptor) ? h.LowerBound : 0.0f;
        float hi = LADSPA_IS_HINT_BOUNDED_ABOVE(h.HintDescriptor) ? h.UpperBound : 1.0f;
        float def = lo;
        const auto d = h.HintDescriptor & LADSPA_HINT_DEFAULT_MASK;
        if (d == LADSPA_HINT_DEFAULT_MAXIMUM) def = hi;
        else if (d == LADSPA_HINT_DEFAULT_LOW) def = lo * 0.75f + hi * 0.25f;
        else if (d == LADSPA_HINT_DEFAULT_MIDDLE) def = (lo + hi) / 2;
        else if (d == LADSPA_HINT_DEFAULT_HIGH) def = lo * 0.25f + hi * 0.75f;
        else if (d == LADSPA_HINT_DEFAULT_0) def = 0;
        else if (d == LADSPA_HINT_DEFAULT_1) def = 1;
        else if (d == LADSPA_HINT_DEFAULT_100) def = 100;
        else if (d == LADSPA_HINT_DEFAULT_440) def = 440;
        controls_[c] = def;
        const std::string portName = desc_->PortNames[p];
        if (exposed_.empty()) {
            params_.push_back({portName, portName, lo, hi, def, {}});
            paramPort_.push_back(int(c));
            continue;
        }
        for (const PortMap& m : exposed_)
            if (m.portName == portName) {
                params_.push_back({m.id, m.displayName.empty() ? portName : m.displayName, lo, hi, def, m.unit});
                paramPort_.push_back(int(c));
            }
    }
}

LadspaEffect::~LadspaEffect() {
    instances_.clear();
    if (handle_) dlclose(handle_);
}

bool LadspaEffect::prepare(uint32_t sampleRate, uint32_t maxFrames, std::string* error) {
    if (!desc_) {
        *error = "cannot load " + label_ + " from " + library_ + (handle_ ? "" : std::string(": ") + dlerror());
        return false;
    }
    if (audioIn_.empty() || audioOut_.empty() || audioIn_.size() != audioOut_.size() || audioIn_.size() > 2) {
        *error = label_ + " is not a mono or stereo effect";
        return false;
    }
    instances_.clear();
    maxFrames_ = maxFrames;
    scratch_.assign(size_t(maxFrames) * 2, 0.0f);
    const size_t count = audioIn_.size() == 1 ? 2 : 1;  // mono plugins run once per channel
    for (size_t i = 0; i < count; i++) {
        auto inst = std::make_unique<Instance>();
        inst->desc = desc_;
        inst->handle = desc_->instantiate(desc_, sampleRate);
        if (!inst->handle) {
            *error = "cannot instantiate " + label_;
            return false;
        }
        for (size_t c = 0; c < controlIn_.size(); c++) desc_->connect_port(inst->handle, controlIn_[c], &controls_[c]);
        for (size_t c = 0; c < controlOut_.size(); c++) desc_->connect_port(inst->handle, controlOut_[c], &controlOuts_[c]);
        if (desc_->activate) desc_->activate(inst->handle);
        instances_.push_back(std::move(inst));
    }
    return true;
}

void LadspaEffect::process(float* const* channels, uint32_t numChannels, uint32_t frames) {
    if (instances_.empty() || frames > maxFrames_) return;
    if (audioIn_.size() == 1) {
        for (uint32_t c = 0; c < numChannels && c < instances_.size(); c++) {
            float* out = scratch_.data() + size_t(c) * maxFrames_;
            desc_->connect_port(instances_[c]->handle, audioIn_[0], channels[c]);
            desc_->connect_port(instances_[c]->handle, audioOut_[0], out);
            desc_->run(instances_[c]->handle, frames);
            std::memcpy(channels[c], out, frames * sizeof(float));
        }
        return;
    }
    float* outs[2] = {scratch_.data(), scratch_.data() + maxFrames_};
    for (int c = 0; c < 2; c++) {
        desc_->connect_port(instances_[0]->handle, audioIn_[c], channels[numChannels > 1 ? c : 0]);
        desc_->connect_port(instances_[0]->handle, audioOut_[c], outs[c]);
    }
    desc_->run(instances_[0]->handle, frames);
    for (uint32_t c = 0; c < numChannels; c++) std::memcpy(channels[c], outs[c], frames * sizeof(float));
}

void LadspaEffect::setParam(int index, float value) {
    if (index < 0 || size_t(index) >= params_.size()) return;
    // A single aligned float store; LADSPA plugins read control ports at the start of run().
    controls_[paramPort_[index]] = std::clamp(value, params_[index].min, params_[index].max);
}

float LadspaEffect::param(int index) const {
    if (index < 0 || size_t(index) >= params_.size()) return 0;
    return controls_[paramPort_[index]];
}

}  // namespace wl
