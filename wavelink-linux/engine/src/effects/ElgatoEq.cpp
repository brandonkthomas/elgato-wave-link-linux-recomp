#include "ElgatoEq.hpp"

#include <algorithm>
#include <cmath>

namespace wl {

namespace {

// createDefaultBands(): {type, frequency, quality, visible}; gain 1 and bypass off for all.
struct DefaultBand {
    int type;
    float frequency, quality;
    bool visible;
};
constexpr DefaultBand kDefaults[ElgatoEq::kBands] = {
    {ElgatoEq::HighPass, 20.0f, 0.707f, true},   {ElgatoEq::Peak, 1000.0f, 1.0f, true},
    {ElgatoEq::Peak, 200.0f, 0.707f, false},     {ElgatoEq::Peak, 2000.0f, 0.707f, false},
    {ElgatoEq::Peak, 7000.0f, 0.707f, false},    {ElgatoEq::Peak, 20.0f, 0.707f, false},
    {ElgatoEq::Peak, 20.0f, 0.707f, false},      {ElgatoEq::LowPass, 20000.0f, 0.707f, true},
};

}  // namespace

ElgatoEq::Biquad ElgatoEq::coefficients(int type, double fs, double f, double q, double gain) {
    const double pi = 3.14159265358979323846;
    f = std::clamp(f, 2.0, fs * 0.499);
    q = std::max(q, 0.01);
    double b0, b1, b2, a0, a1, a2;
    if (type == HighPass || type == LowPass) {
        const double n = type == HighPass ? std::tan(pi * f / fs) : 1.0 / std::tan(pi * f / fs);
        const double n2 = n * n, invQ = 1.0 / q, c1 = 1.0 / (1.0 + invQ * n + n2);
        b0 = c1;
        b1 = type == HighPass ? -2.0 * c1 : 2.0 * c1;
        b2 = c1;
        a0 = 1.0;
        a1 = type == HighPass ? c1 * 2.0 * (n2 - 1.0) : c1 * 2.0 * (1.0 - n2);
        a2 = c1 * (1.0 - invQ * n + n2);
    } else if (type == Peak) {
        const double A = std::sqrt(std::max(gain, 1e-15)), omega = 2.0 * pi * f / fs;
        const double alpha = std::sin(omega) / (2.0 * q), c2 = -2.0 * std::cos(omega);
        b0 = 1.0 + alpha * A;
        b1 = c2;
        b2 = 1.0 - alpha * A;
        a0 = 1.0 + alpha / A;
        a1 = c2;
        a2 = 1.0 - alpha / A;
    } else {  // shelves
        const double A = std::sqrt(std::max(gain, 1e-15)), am1 = A - 1.0, ap1 = A + 1.0;
        const double omega = 2.0 * pi * f / fs, coso = std::cos(omega);
        const double beta = std::sin(omega) * std::sqrt(A) / q, am1Coso = am1 * coso;
        if (type == LowShelf) {
            b0 = A * (ap1 - am1Coso + beta);
            b1 = A * 2.0 * (am1 - ap1 * coso);
            b2 = A * (ap1 - am1Coso - beta);
            a0 = ap1 + am1Coso + beta;
            a1 = -2.0 * (am1 + ap1 * coso);
            a2 = ap1 + am1Coso - beta;
        } else {
            b0 = A * (ap1 + am1Coso + beta);
            b1 = A * -2.0 * (am1 + ap1 * coso);
            b2 = A * (ap1 + am1Coso - beta);
            a0 = ap1 - am1Coso + beta;
            a1 = 2.0 * (am1 - ap1 * coso);
            a2 = ap1 - am1Coso - beta;
        }
    }
    return {float(b0 / a0), float(b1 / a0), float(b2 / a0), float(a1 / a0), float(a2 / a0)};
}

ElgatoEq::ElgatoEq() {
    static const char* fieldNames[kFields] = {"Type", "Frequency", "Quality", "Gain", "Bypass", "Visible"};
    for (int b = 0; b < kBands; b++) {
        const DefaultBand& d = kDefaults[b];
        const std::string prefix = "Band " + std::to_string(b + 1) + " Filter ";
        const float defs[kFields] = {float(d.type), d.frequency, d.quality, 1.0f, 0.0f, d.visible ? 1.0f : 0.0f};
        const float mins[kFields] = {0, 20, 0.1f, 0.25f, 0, 0};
        const float maxs[kFields] = {4, 20000, 10, 4, 1, 1};
        const char* units[kFields] = {"", "Hz", "", "", "", ""};
        for (int f = 0; f < kFields; f++) {
            params_.push_back({prefix + fieldNames[f], prefix + fieldNames[f], mins[f], maxs[f], defs[f], units[f]});
            values_[b * kFields + f] = defs[f];
        }
    }
}

ElgatoEq::Settings ElgatoEq::compute() const {
    Settings s;
    for (int b = 0; b < kBands; b++) {
        const float* v = &values_[b * kFields];
        s.coeffs[b] = coefficients(int(std::lround(v[Type])), rate_, v[Frequency], v[Quality], v[Gain]);
        s.bypass[b] = v[Bypass] >= 0.5f;
    }
    return s;
}

bool ElgatoEq::prepare(uint32_t sampleRate, uint32_t, std::string*) {
    rate_ = sampleRate ? sampleRate : 48000;
    const Settings s = compute();
    settings_.fill(s);  // not yet running: all three slots may be written
    for (auto& channel : state_)
        for (auto& band : channel) band = {0.0f, 0.0f};
    return true;
}

void ElgatoEq::publish() {
    settings_[back_] = compute();
    back_ = middle_.exchange(back_ | kFresh, std::memory_order_acq_rel) & 3;
}

void ElgatoEq::process(float* const* channels, uint32_t numChannels, uint32_t frames) {
    if (middle_.load(std::memory_order_acquire) & kFresh)
        front_ = middle_.exchange(front_, std::memory_order_acq_rel) & 3;
    const Settings& s = settings_[front_];
    for (uint32_t c = 0; c < numChannels && c < 2; c++) {
        float* x = channels[c];
        for (int b = 0; b < kBands; b++) {
            const Biquad& q = s.coeffs[b];
            float s1 = state_[c][b][0], s2 = state_[c][b][1];
            const bool bypass = s.bypass[b];
            for (uint32_t i = 0; i < frames; i++) {
                const float in = x[i];
                const float out = q.b0 * in + s1;
                s1 = q.b1 * in - q.a1 * out + s2;
                s2 = q.b2 * in - q.a2 * out;
                if (!bypass) x[i] = out;
            }
            // JUCE_SNAP_TO_ZERO, plus a reset should a state ever stop being finite.
            if (!std::isfinite(s1) || !std::isfinite(s2)) s1 = s2 = 0.0f;
            if (std::fabs(s1) < 1e-8f) s1 = 0.0f;
            if (std::fabs(s2) < 1e-8f) s2 = 0.0f;
            state_[c][b] = {s1, s2};
        }
    }
}

void ElgatoEq::setParam(int index, float value) {
    if (index < 0 || index >= int(params_.size())) return;
    float v = std::clamp(value, params_[index].min, params_[index].max);
    const int field = index % kFields;
    if (field == Type || field == Bypass || field == Visible) v = std::round(v);
    if (values_[index] == v) return;
    values_[index] = v;
    if (field != Visible) publish();
}

float ElgatoEq::param(int index) const {
    return index >= 0 && index < int(params_.size()) ? values_[index] : 0.0f;
}

}  // namespace wl
