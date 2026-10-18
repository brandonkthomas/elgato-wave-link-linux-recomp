#include "MixerCore.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wl {

MixerCore::MixerCore()
    : scratch_(size_t(kInputSlots) * 2 * kMaxFrames, 0.0f), record_(size_t(kRecordSeconds) * 96000, 0.0f) {
    for (auto& c : chains_) c.store(nullptr, std::memory_order_relaxed);
    for (auto& m : mono_) m.store(false, std::memory_order_relaxed);
    for (auto& row : target_)
        for (auto& g : row) g.store(0.0f, std::memory_order_relaxed);
    for (auto& p : inputPeak_)
        for (auto& c : p) c.store(0.0f, std::memory_order_relaxed);
    for (auto& p : mixPeak_)
        for (auto& c : p) c.store(0.0f, std::memory_order_relaxed);
    for (auto& row : cellPeak_)
        for (auto& p : row)
            for (auto& c : p) c.store(0.0f, std::memory_order_relaxed);
}

void MixerCore::setTargetGain(int inputSlot, int mixSlot, float gain) {
    target_[inputSlot][mixSlot].store(gain, std::memory_order_relaxed);
}

void MixerCore::raise(std::atomic<float>& slot, float value) {
    float prev = slot.load(std::memory_order_relaxed);
    while (value > prev && !slot.compare_exchange_weak(prev, value, std::memory_order_relaxed)) {
    }
}

Peak MixerCore::drain(AtomicPeak& p) {
    return {p[0].exchange(0.0f, std::memory_order_relaxed), p[1].exchange(0.0f, std::memory_order_relaxed)};
}

void MixerCore::setChain(int inputSlot, const Chain* chain) {
    chains_[inputSlot].store(chain, std::memory_order_release);
}

void MixerCore::setMono(int inputSlot, bool mono) {
    mono_[inputSlot].store(mono, std::memory_order_relaxed);
}

void MixerCore::startRecording(int inputSlot) {
    recordSlot_.store(-1, std::memory_order_release);
    const uint32_t rate = rate_.load(std::memory_order_relaxed);
    recordRate_.store(rate, std::memory_order_relaxed);
    recordCap_.store(std::min(record_.size(), size_t(rate) * kRecordSeconds), std::memory_order_relaxed);
    recordPos_.store(0, std::memory_order_release);
    recordSlot_.store(inputSlot, std::memory_order_release);
}

void MixerCore::stopRecording() { recordSlot_.store(-1, std::memory_order_release); }

void MixerCore::record(int inputSlot, const float* left, const float* right, uint32_t frames) {
    size_t pos = recordPos_.load(std::memory_order_acquire);
    const size_t cap = recordCap_.load(std::memory_order_relaxed);
    const size_t n = std::min(size_t(frames), cap > pos ? cap - pos : 0);
    for (size_t i = 0; i < n; i++) record_[pos + i] = 0.5f * (left[i] + right[i]);
    // A restart may have reset the position meanwhile; then this cycle's samples are simply dropped.
    if (!recordPos_.compare_exchange_strong(pos, pos + n, std::memory_order_acq_rel)) return;
    if (pos + n >= cap) recordSlot_.compare_exchange_strong(inputSlot, -1, std::memory_order_acq_rel);
}

Peak MixerCore::takeInputPeak(int inputSlot) { return drain(inputPeak_[inputSlot]); }
Peak MixerCore::takeMixPeak(int mixSlot) { return drain(mixPeak_[mixSlot]); }
Peak MixerCore::takeCellPeak(int inputSlot, int mixSlot) { return drain(cellPeak_[inputSlot][mixSlot]); }

void MixerCore::process(const float* (&in)[kInputSlots][kChannels], float* (&out)[kMixSlots][kChannels],
                        uint32_t frames) {
    for (int m = 0; m < kMixSlots; m++)
        for (int c = 0; c < kChannels; c++)
            if (out[m][c]) std::memset(out[m][c], 0, frames * sizeof(float));
    if (frames == 0) return;
    const float invFrames = 1.0f / float(frames);

    for (int i = 0; i < kInputSlots; i++) {
        if (!in[i][0] && !in[i][1]) {
            for (int m = 0; m < kMixSlots; m++) current_[i][m] = target_[i][m].load(std::memory_order_relaxed);
            continue;
        }
        const float* src[kChannels] = {in[i][0] ? in[i][0] : in[i][1], in[i][1] ? in[i][1] : in[i][0]};
        const Chain* chain = chains_[i].load(std::memory_order_acquire);
        if (chain && !chain->entries.empty() && frames <= kMaxFrames) {
            float* buf[kChannels] = {&scratch_[(size_t(i) * 2) * kMaxFrames], &scratch_[(size_t(i) * 2 + 1) * kMaxFrames]};
            const bool mono = mono_[i].load(std::memory_order_relaxed);
            std::memcpy(buf[0], src[0], frames * sizeof(float));
            if (!mono) std::memcpy(buf[1], src[1], frames * sizeof(float));
            for (const Chain::Entry& e : chain->entries)
                if (!e.bypass->load(std::memory_order_relaxed)) e.effect->process(buf, mono ? 1 : 2, frames);
            if (mono) std::memcpy(buf[1], buf[0], frames * sizeof(float));
            src[0] = buf[0];
            src[1] = buf[1];
        }
        for (int c = 0; c < kChannels; c++) {
            float peak = 0.0f;
            for (uint32_t n = 0; n < frames; n++) peak = std::max(peak, std::fabs(src[c][n]));
            raise(inputPeak_[i][c], peak);
        }
        if (recordSlot_.load(std::memory_order_acquire) == i) record(i, src[0], src[1], frames);
        for (int m = 0; m < kMixSlots; m++) {
            const float from = current_[i][m];
            const float to = target_[i][m].load(std::memory_order_relaxed);
            current_[i][m] = to;
            if (from == 0.0f && to == 0.0f) continue;
            const float step = (to - from) * invFrames;
            for (int c = 0; c < kChannels; c++) {
                const float* s = src[c];
                float* dst = out[m][c];
                float g = from;
                float peak = 0.0f;
                for (uint32_t n = 0; n < frames; n++) {
                    g += step;  // linear ramp avoids zipper noise on gain changes
                    const float v = s[n] * g;
                    peak = std::max(peak, std::fabs(v));
                    if (dst) dst[n] += v;
                }
                raise(cellPeak_[i][m][c], peak);
            }
        }
    }
    for (int m = 0; m < kMixSlots; m++)
        for (int c = 0; c < kChannels; c++) {
            if (!out[m][c]) continue;
            float peak = 0.0f;
            for (uint32_t n = 0; n < frames; n++) peak = std::max(peak, std::fabs(out[m][c][n]));
            raise(mixPeak_[m][c], peak);
        }
}

}  // namespace wl
