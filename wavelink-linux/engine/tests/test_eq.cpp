// Elgato EQ clone: filter responses measured on sine tones, bypass, and parameter publishing.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "effects/ElgatoEq.hpp"

using namespace wl;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                          \
        }                                                                          \
    } while (0)

constexpr uint32_t kRate = 48000;

int paramIndex(const Effect& fx, const std::string& id) {
    for (size_t i = 0; i < fx.params().size(); i++)
        if (fx.params()[i].id == id) return int(i);
    return -1;
}

// Steady-state gain in dB of the EQ for a sine at `hz` (stereo, processed in 256-frame blocks).
double gainDb(ElgatoEq& eq, double hz) {
    std::vector<float> left(256), right(256);
    float* ch[2] = {left.data(), right.data()};
    double inPow = 0, outPow = 0;
    for (int block = 0; block < 400; block++) {
        for (int i = 0; i < 256; i++) left[i] = right[i] = float(0.25 * std::sin(2 * M_PI * hz * (block * 256 + i) / kRate));
        std::vector<float> in = left;
        eq.process(ch, 2, 256);
        if (block < 200) continue;  // let the filters settle
        for (int i = 0; i < 256; i++) {
            inPow += double(in[i]) * in[i];
            outPow += double(left[i]) * left[i];
        }
    }
    return 10 * std::log10(outPow / inPow);
}

bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

}  // namespace

int main() {
    ElgatoEq eq;
    std::string error;
    CHECK(eq.prepare(kRate, 8192, &error));
    CHECK(eq.params().size() == 48);
    CHECK(paramIndex(eq, "Band 2 Filter Gain") >= 0);

    // Defaults: high-pass 20 Hz, unity peak at 1 kHz, low-pass 20 kHz -> flat in the voice range.
    CHECK(near(gainDb(eq, 1000), 0.0, 0.1));
    CHECK(near(gainDb(eq, 200), 0.0, 0.1));

    // Band 2 peak at 1 kHz, gain 4 (+12.04 dB) at its centre, little effect two octaves away.
    eq.setParam(paramIndex(eq, "Band 2 Filter Gain"), 4.0f);
    const double peak = gainDb(eq, 1000);
    std::printf("peak +12 dB at 1 kHz: %.2f dB, at 250 Hz: %.2f dB\n", peak, gainDb(eq, 250));
    CHECK(near(peak, 12.04, 0.15));
    CHECK(gainDb(eq, 250) < 2.0);

    // Bypass passes the input through.
    eq.setParam(paramIndex(eq, "Band 2 Filter Bypass"), 1.0f);
    CHECK(near(gainDb(eq, 1000), 0.0, 0.1));
    eq.setParam(paramIndex(eq, "Band 2 Filter Bypass"), 0.0f);
    eq.setParam(paramIndex(eq, "Band 2 Filter Gain"), 1.0f);

    // High-pass moved to 1 kHz (Q 0.707): -3 dB at the corner, about -18 dB at 350 Hz (12 dB/octave).
    eq.setParam(paramIndex(eq, "Band 1 Filter Frequency"), 1000.0f);
    const double corner = gainDb(eq, 1000), below = gainDb(eq, 350);
    std::printf("high-pass 1 kHz: %.2f dB at 1 kHz, %.2f dB at 350 Hz\n", corner, below);
    CHECK(near(corner, -3.01, 0.2));
    CHECK(below < -17.0);
    eq.setParam(paramIndex(eq, "Band 1 Filter Frequency"), 20.0f);

    // Low shelf at 300 Hz, +6 dB: full boost well below, nothing well above.
    eq.setParam(paramIndex(eq, "Band 3 Filter Type"), float(ElgatoEq::LowShelf));
    eq.setParam(paramIndex(eq, "Band 3 Filter Frequency"), 300.0f);
    eq.setParam(paramIndex(eq, "Band 3 Filter Gain"), 2.0f);
    std::printf("low shelf +6 dB at 300 Hz: %.2f dB at 50 Hz, %.2f dB at 5 kHz\n", gainDb(eq, 50), gainDb(eq, 5000));
    CHECK(near(gainDb(eq, 50), 6.02, 0.3));
    CHECK(near(gainDb(eq, 5000), 0.0, 0.3));

    if (g_failures) std::fprintf(stderr, "%d failure(s)\n", g_failures);
    else std::printf("all EQ tests passed\n");
    return g_failures ? 1 : 0;
}
