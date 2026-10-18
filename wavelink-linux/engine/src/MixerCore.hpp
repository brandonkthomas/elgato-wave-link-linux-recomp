// Real-time mixing matrix. The control thread sets target gains; the PipeWire data thread mixes and
// records peaks. Only atomics are shared, so neither side ever blocks the other.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "Model.hpp"
#include "effects/Effect.hpp"

namespace wl {

struct Peak {
    float left = 0.0f;
    float right = 0.0f;
};

class MixerCore {
public:
    MixerCore();

    static constexpr uint32_t kMaxFrames = 8192;  // largest PipeWire quantum we process effects for

    // Control thread.
    void setTargetGain(int inputSlot, int mixSlot, float gain);
    // The chain stays owned by the caller, which must keep it alive until the data thread has
    // finished any cycle that may still use it (see Engine's retire list).
    void setChain(int inputSlot, const Chain* chain);
    // Mono sources feed both input channels the same signal; effects then run once.
    void setMono(int inputSlot, bool mono);
    uint32_t sampleRate() const { return rate_.load(std::memory_order_relaxed); }
    void setSampleRate(uint32_t rate) { rate_.store(rate, std::memory_order_relaxed); }
    Peak takeInputPeak(int inputSlot);
    Peak takeMixPeak(int mixSlot);
    Peak takeCellPeak(int inputSlot, int mixSlot);

    // Sound Check recorder: captures one input's signal after its effects, mixed to mono, for up to
    // kRecordSeconds, then stops by itself. Samples [0, recordedFrames()) are stable once recording
    // has stopped.
    static constexpr uint32_t kRecordSeconds = 10;
    void startRecording(int inputSlot);
    void stopRecording();
    bool recording() const { return recordSlot_.load(std::memory_order_acquire) >= 0; }
    size_t recordedFrames() const { return recordPos_.load(std::memory_order_acquire); }
    size_t recordCapacity() const { return recordCap_.load(std::memory_order_relaxed); }
    uint32_t recordRate() const { return recordRate_.load(std::memory_order_relaxed); }
    const float* recordData() const { return record_.data(); }

    // Data thread. in[slot][ch] may be null (unlinked port); out[mix][ch] may be null.
    void process(const float* (&in)[kInputSlots][kChannels], float* (&out)[kMixSlots][kChannels],
                 uint32_t frames);

private:
    using AtomicPeak = std::array<std::atomic<float>, kChannels>;

    static void raise(std::atomic<float>& slot, float value);
    static Peak drain(AtomicPeak& p);

    std::array<std::array<std::atomic<float>, kMixSlots>, kInputSlots> target_;
    std::array<std::array<float, kMixSlots>, kInputSlots> current_{};  // data thread only
    std::array<AtomicPeak, kInputSlots> inputPeak_;
    std::array<AtomicPeak, kMixSlots> mixPeak_;
    std::array<std::array<AtomicPeak, kMixSlots>, kInputSlots> cellPeak_;
    std::array<std::atomic<const Chain*>, kInputSlots> chains_{};
    std::array<std::atomic<bool>, kInputSlots> mono_{};
    std::atomic<uint32_t> rate_{48000};
    std::vector<float> scratch_;  // kInputSlots × 2 channels × kMaxFrames

    void record(int inputSlot, const float* left, const float* right, uint32_t frames);
    std::vector<float> record_;  // kRecordSeconds at up to 96 kHz, allocated once
    std::atomic<int> recordSlot_{-1};
    std::atomic<size_t> recordPos_{0};
    std::atomic<size_t> recordCap_{0};
    std::atomic<uint32_t> recordRate_{48000};
};

}  // namespace wl
