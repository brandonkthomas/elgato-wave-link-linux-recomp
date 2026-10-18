// Protocol tests against messages captured from a real Wave:3 (firmware 1.2.2, API 5.3).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "wave3/codec.hpp"
#include "wave3/schema.hpp"

using namespace wave3;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

template <typename F>
bool throws(F&& f) {
    try {
        f();
    } catch (const ValueError&) {
        return true;
    }
    return false;
}

// GET_MEM captures from a real Wave:3, its serial replaced with BS0000000000.
const std::vector<uint8_t> kConfig = {0x00, 0x14, 0x00, 0xec, 0x01, 0x01, 0x00, 0x80,
                                      0xea, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
const std::vector<uint8_t> kVersion = {
    0x05, 0x03, 0x4b, 0x03, 0x00, 0x0a, 0x00, 0x03, 0x07, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x03, 0x00,
    0x00, 0x00, 0x30, 0x62, 0x34, 0x32, 0x35, 0x31, 0x65, 0x65, 0x42, 0x53, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x84, 0x03, 0xdc, 0x05};

const FieldDef& field(const ApiSchema& s, const char* msg, const char* key) {
    const FieldDef* f = s.message(msg)->field(key);
    if (!f) {
        std::fprintf(stderr, "missing field %s/%s\n", msg, key);
        std::exit(1);
    }
    return *f;
}

void test_schema_selection() {
    CHECK(select_schema(5, 3)->minor == 3);
    CHECK(select_schema(5, 9)->minor == 4);  // newer minor falls back to the newest known
    CHECK(select_schema(5, 2)->message("config")->size == 14);
    CHECK(select_schema(5, 1) == nullptr);
    CHECK(select_schema(6, 0) == nullptr);
    for (const auto& s : wave3_schemas())
        for (const auto& m : s.messages)
            for (const auto& f : m.fields) CHECK(f.offset + f.size <= m.size);
}

void test_decode_capture() {
    const ApiSchema& s = *select_schema(5, 3);
    CHECK(s.message("version")->size == kVersion.size());
    CHECK(s.message("config")->size == kConfig.size());
    CHECK(std::get<double>(decode(field(s, "config", "input_gain"), kConfig)) == 20.0);
    CHECK(std::get<bool>(decode(field(s, "config", "input_mute"), kConfig)) == true);
    CHECK(std::get<bool>(decode(field(s, "config", "clipguard_enable"), kConfig)) == true);
    CHECK(std::get<bool>(decode(field(s, "config", "lowcut_enable"), kConfig)) == false);
    CHECK(std::get<double>(decode(field(s, "config", "headphone_volume"), kConfig)) == -21.5);
    CHECK(std::get<double>(decode(field(s, "config", "direct_monitor"), kConfig)) == 0.0);
    CHECK(std::get<uint64_t>(decode(field(s, "config", "volume_select"), kConfig)) == 2);
    CHECK(format_value(field(s, "config", "volume_select"), uint64_t(2)) == "headphone (2)");
    CHECK(std::get<std::string>(decode(field(s, "version", "serial"), kVersion)) == "BS0000000000");
    CHECK(std::get<std::string>(decode(field(s, "version", "fw/commit"), kVersion)) == "0b4251ee");
    CHECK(std::get<uint64_t>(decode(field(s, "version", "fw/major"), kVersion)) == 1);
    CHECK(std::get<uint64_t>(decode(field(s, "version", "fw/minor"), kVersion)) == 2);
    CHECK(std::get<uint64_t>(decode(field(s, "version", "fw/patch"), kVersion)) == 2);
    CHECK(std::get<uint64_t>(decode(field(s, "version", "touch_signal_thr/lower"), kVersion)) == 900);
    CHECK(std::get<uint64_t>(decode(field(s, "version", "touch_signal_thr/upper"), kVersion)) == 1500);
}

void test_encode_preserves_other_bytes() {
    const ApiSchema& s = *select_schema(5, 3);
    std::vector<uint8_t> m = kConfig;
    encode(field(s, "config", "input_gain"), 22.5, m);
    encode(field(s, "config", "headphone_volume"), -60.0, m);
    encode(field(s, "config", "lowcut_enable"), true, m);
    CHECK(m[0] == 0x80 && m[1] == 0x16);  // 22.5 * 256 = 0x1680
    CHECK(m[7] == 0x00 && m[8] == 0xc4);  // -60 * 256 = 0xc400
    CHECK(m[6] == 1);
    CHECK(m[2] == 0x00 && m[3] == 0xec);  // bytes outside any field are untouched
    for (size_t i : {4, 5, 9, 10, 11, 12, 13, 14, 15}) CHECK(m[i] == kConfig[i]);
    encode(field(s, "config", "input_gain"), 20.0, m);
    encode(field(s, "config", "headphone_volume"), -21.5, m);
    encode(field(s, "config", "lowcut_enable"), false, m);
    CHECK(m == kConfig);
}

void test_parse_and_limits() {
    const ApiSchema& s = *select_schema(5, 3);
    const FieldDef& gain = field(s, "config", "input_gain");
    const FieldDef& mix = field(s, "config", "direct_monitor");
    const FieldDef& knob = field(s, "config", "volume_select");
    bool snapped = false;
    CHECK(std::get<double>(parse_value(gain, "22.5", &snapped)) == 22.5 && !snapped);
    CHECK(std::get<double>(parse_value(gain, "22.7dB", &snapped)) == 22.5 && snapped);
    CHECK(std::get<double>(parse_value(mix, "52%", &snapped)) == 50.0 && snapped);
    CHECK(std::get<uint64_t>(parse_value(knob, "MIX")) == 3);
    CHECK(std::get<bool>(parse_value(field(s, "config", "gain_lock"), "on")) == true);
    CHECK(throws([&] { parse_value(gain, "41"); }));
    CHECK(throws([&] { parse_value(gain, "-1"); }));
    CHECK(throws([&] { parse_value(gain, "loud"); }));
    CHECK(throws([&] { parse_value(knob, "4"); }));
    CHECK(throws([&] { parse_value(knob, "0"); }));
    CHECK(throws([&] { parse_value(field(s, "config", "input_mute"), "maybe"); }));
}

void test_keys() {
    const ApiSchema& s = *select_schema(5, 3);
    const MessageDef* m = nullptr;
    const FieldDef* f = nullptr;
    CHECK(resolve_key(s, "gain", m, f) && f->key == "input_gain");
    CHECK(resolve_key(s, "/config/lowcut_enable", m, f) && f->key == "lowcut_enable");
    CHECK(resolve_key(s, "version/serial", m, f) && m->name == "version");
    CHECK(!resolve_key(s, "nonsense", m, f));
    CHECK(!resolve_key(*select_schema(5, 2), "gain_lock", m, f));  // not in API 5.2
}

}  // namespace

int main() {
    test_schema_selection();
    test_decode_capture();
    test_encode_preserves_other_bytes();
    test_parse_and_limits();
    test_keys();
    if (g_failures) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all protocol tests passed");
    return 0;
}
