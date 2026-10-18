// Model and mixer-core tests (no PipeWire needed).
#include <cmath>
#include <cstdio>

#include "MixerCore.hpp"
#include "Model.hpp"

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

bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

void testVolumeLaw() {
    CHECK(near(sliderToDb(1.0f), 0.0f));
    CHECK(near(sliderToDb(0.5f), -30.0f));
    CHECK(near(sliderToDb(0.0f), -60.0f));
    CHECK(near(sliderToGain(1.0f), 1.0f));
    CHECK(near(sliderToGain(0.5f), std::pow(10.0f, -1.5f)));
    CHECK(sliderToGain(0.0f) == 0.0f);
}

void testDefaultsAndJson() {
    Model m = Model::defaults();
    CHECK(m.mixes.size() == 3);
    CHECK(m.mixes[0].name == "Personal Mix" && m.mixes[1].name == "Stream Mix" && m.mixes[2].name == "Chat Mix");
    CHECK(m.inputs.size() == 1 && m.inputs[0].name == "System" && m.inputs[0].slot == 0);
    CHECK(m.freeSoftwareSlot() == 1 && m.freeHardwareSlot() == kSoftwareSlots && m.freeMixSlot() == 3);

    m.cells[0][1].volume = 0.25f;
    m.cells[0][2].muted = true;
    m.mixes[0].outputDevices = {"alsa_output.usb-Elgato_Wave_3"};
    Model r = Model::fromJson(m.toJson());
    CHECK(r.inputs.size() == 1 && r.mixes.size() == 3);
    CHECK(near(r.cells[0][1].volume, 0.25f));
    CHECK(r.cells[0][2].muted);
    CHECK(r.mixes[0].outputDevices == QStringList{"alsa_output.usb-Elgato_Wave_3"});
    CHECK(r.cellGain(0, 2) == 0.0f);
    CHECK(near(r.cellGain(0, 0), 1.0f));
    CHECK(r.cellGain(5, 0) == 0.0f);  // no input in slot 5
}

void testMixer() {
    MixerCore core;
    core.setTargetGain(0, 0, 1.0f);
    core.setTargetGain(0, 1, 0.5f);
    float l[64], r[64], o0l[64], o0r[64], o1l[64], o1r[64];
    for (int n = 0; n < 64; n++) l[n] = r[n] = 0.8f;
    const float* in[kInputSlots][kChannels] = {};
    float* out[kMixSlots][kChannels] = {};
    in[0][0] = l;
    in[0][1] = r;
    out[0][0] = o0l;
    out[0][1] = o0r;
    out[1][0] = o1l;
    out[1][1] = o1r;
    core.process(in, out, 64);  // ramps from 0 to the targets
    CHECK(near(o0l[63], 0.8f) && near(o1r[63], 0.4f));
    core.process(in, out, 64);  // steady state
    CHECK(near(o0l[0], 0.8f) && near(o1l[10], 0.4f));
    CHECK(near(core.takeInputPeak(0).left, 0.8f));
    CHECK(near(core.takeMixPeak(1).right, 0.4f));
    CHECK(core.takeMixPeak(1).right == 0.0f);  // drained
    core.setTargetGain(0, 0, 0.0f);
    core.process(in, out, 64);
    core.process(in, out, 64);
    CHECK(o0l[10] == 0.0f);
}

}  // namespace

int main() {
    testVolumeLaw();
    testDefaultsAndJson();
    testMixer();
    if (g_failures) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("model tests passed");
    return 0;
}
