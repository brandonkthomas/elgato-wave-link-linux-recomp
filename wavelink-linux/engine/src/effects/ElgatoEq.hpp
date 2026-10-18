// Elgato EQ 1.3 (Elgato Marketplace), rebuilt natively. The plugin is a JUCE processor with eight bands
// in series, each a juce::dsp::IIR::Filter whose coefficients come from JUCE's (RBJ cookbook)
// factories: highPass and lowPass use frequency and Q; lowShelf, peak and highShelf also take a linear
// gain (0.25..4, ±12 dB). A bypassed band keeps filtering (its state stays warm) but passes its input
// through, as JUCE's ProcessorChain does; "Visible" only matters to the editor. Parameter ids are the
// plugin's ("Band 1 Filter Frequency", ...), so its presets map onto them directly.
#pragma once

#include <array>
#include <atomic>

#include "Effect.hpp"

namespace wl {

class ElgatoEq : public Effect {
public:
    static constexpr int kBands = 8;
    enum FilterType { HighPass, LowShelf, Peak, HighShelf, LowPass };
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;  // normalised by a0
    };
    static Biquad coefficients(int type, double sampleRate, double frequency, double quality, double gain);

    ElgatoEq();
    std::string type() const override { return "ElgatoEQ"; }
    const std::vector<ParamInfo>& params() const override { return params_; }
    bool prepare(uint32_t sampleRate, uint32_t maxFrames, std::string* error) override;
    void process(float* const* channels, uint32_t numChannels, uint32_t frames) override;
    void setParam(int index, float value) override;
    float param(int index) const override;

private:
    enum Field { Type, Frequency, Quality, Gain, Bypass, Visible, kFields };
    struct Settings {
        std::array<Biquad, kBands> coeffs{};
        std::array<bool, kBands> bypass{};
    };
    Settings compute() const;
    void publish();  // control thread: hand freshly computed settings to the data thread

    std::vector<ParamInfo> params_;
    std::array<float, kBands * kFields> values_{};
    uint32_t rate_ = 48000;

    // Triple buffer: the control thread fills back_ and swaps it into middle_ marked fresh; the data
    // thread swaps front_ with middle_ when it sees the mark. Neither side waits for the other.
    static constexpr int kFresh = 4;
    std::array<Settings, 3> settings_{};
    int back_ = 0;
    int front_ = 1;
    std::atomic<int> middle_{2};

    std::array<std::array<std::array<float, 2>, kBands>, 2> state_{};  // TDF-II, per channel and band
};

}  // namespace wl
