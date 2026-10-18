// Offline check: white noise through an effect type, RMS per 100 ms block.
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "effects/Registry.hpp"

int main(int argc, char** argv) {
    const std::string type = argc > 1 ? argv[1] : "VoiceFocus";
    const std::string dir = argc > 2 ? argv[2] : ".";
    const uint32_t block = argc > 3 ? std::stoul(argv[3]) : 1024;
    std::string error;
    auto fx = wl::createEffect(type, dir, 48000, 8192, &error);
    if (!fx) { std::fprintf(stderr, "error: %s\n", error.c_str()); return 1; }
    const bool threaded = argc > 4 && std::string(argv[4]) == "thread";
    std::thread worker([&] {
    std::mt19937 rng(1);
    std::normal_distribution<float> noise(0.0f, 0.1f);
    std::vector<float> l(block), r(block);
    double acc = 0; uint32_t n = 0, total = 0;
    for (int b = 0; b < int(48000 * 3 / block); b++) {
        for (uint32_t i = 0; i < block; i++) l[i] = r[i] = noise(rng);
        float* ch[2] = {l.data(), r.data()};
        fx->process(ch, 2, block);
        for (uint32_t i = 0; i < block; i++) { acc += double(l[i]) * l[i]; n++; }
        total += block;
        if (n >= 4800) { std::printf("t=%.1fs rms=%.1f dBFS\n", total / 48000.0, 10 * std::log10(acc / n + 1e-20)); acc = 0; n = 0; }
        if (threaded) std::this_thread::sleep_for(std::chrono::microseconds(1000000 * block / 48000));
    }
    });
    worker.join();
}
