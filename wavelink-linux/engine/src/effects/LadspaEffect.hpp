// Hosts a LADSPA plugin as an Effect: one instance per channel for mono plugins, one shared instance
// for stereo plugins. Control ports are exposed as parameters.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Effect.hpp"

struct _LADSPA_Descriptor;

namespace wl {

class LadspaEffect : public Effect {
public:
    // `params` maps our parameter ids/names onto control ports by port name; empty exposes them all.
    struct PortMap {
        std::string id;
        std::string portName;
        std::string displayName;
        std::string unit;
    };

    LadspaEffect(std::string type, std::string library, std::string label, std::vector<PortMap> exposed = {});
    ~LadspaEffect() override;

    std::string type() const override { return type_; }
    const std::vector<ParamInfo>& params() const override { return params_; }
    bool prepare(uint32_t sampleRate, uint32_t maxFrames, std::string* error) override;
    void process(float* const* channels, uint32_t numChannels, uint32_t frames) override;
    void setParam(int index, float value) override;
    float param(int index) const override;

private:
    struct Instance;

    std::string type_, library_, label_;
    std::vector<PortMap> exposed_;
    void* handle_ = nullptr;
    const _LADSPA_Descriptor* desc_ = nullptr;
    std::vector<unsigned long> audioIn_, audioOut_, controlIn_, controlOut_;
    std::vector<int> paramPort_;           // our parameter index -> index into controlIn_
    std::vector<ParamInfo> params_;
    std::unique_ptr<float[]> controls_;    // shared control-port values (read by run())
    std::unique_ptr<float[]> controlOuts_;
    std::vector<std::unique_ptr<Instance>> instances_;
    std::vector<float> scratch_;           // output buffers (LADSPA in/out may not alias)
    uint32_t maxFrames_ = 0;
};

}  // namespace wl
