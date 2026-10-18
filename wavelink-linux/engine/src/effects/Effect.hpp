// Audio effects applied per input before the mixing matrix (Wave Link's per-channel effect chain).
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wl {

struct ParamInfo {
    std::string id;      // stable key used in the configuration and API
    std::string name;    // display name
    float min = 0;
    float max = 1;
    float def = 0;
    std::string unit;
};

// An effect instance. prepare() and parameter writes happen on the control thread; process() runs on
// the PipeWire data thread and must not allocate, lock or block.
class Effect {
public:
    virtual ~Effect() = default;
    virtual std::string type() const = 0;
    virtual const std::vector<ParamInfo>& params() const = 0;
    virtual bool prepare(uint32_t sampleRate, uint32_t maxFrames, std::string* error) = 0;
    // Processes `channels` (1 or 2) buffers of `frames` samples in place.
    virtual void process(float* const* channels, uint32_t numChannels, uint32_t frames) = 0;
    virtual void setParam(int index, float value) = 0;
    virtual float param(int index) const = 0;
    virtual uint32_t latencyFrames() const { return 0; }
};

// One immutable snapshot of an input's chain, swapped atomically into the data thread.
struct Chain {
    struct Entry {
        std::shared_ptr<Effect> effect;
        std::shared_ptr<std::atomic<bool>> bypass;
    };
    std::vector<Entry> entries;
};

}  // namespace wl
