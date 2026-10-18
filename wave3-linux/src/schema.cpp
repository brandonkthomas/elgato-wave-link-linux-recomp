#include "wave3/schema.hpp"

#include <algorithm>
#include <map>

namespace wave3 {

namespace {

std::vector<FieldDef> config_fields(bool with_flip_and_lock) {
    const std::vector<EnumEntry> knob = {{"mic", 1}, {"headphone", 2}, {"mix", 3}};
    std::vector<FieldDef> f = {
        {"input_gain", 0, 2, Kind::Fixed, 8, Limits{0, 40, 0.5}, {}, "dB", "Microphone gain"},
        {"input_mute", 4, 1, Kind::Bool, 0, {}, {}, "", "Microphone mute"},
        {"clipguard_enable", 5, 1, Kind::Bool, 0, {}, {}, "", "Clipguard enable"},
        {"lowcut_enable", 6, 1, Kind::Bool, 0, {}, {}, "", "Low-cut filter enable"},
        {"headphone_volume", 7, 2, Kind::Fixed, 8, Limits{-60, 0, 0.5}, {}, "dB",
         "Headphone output level"},
        {"headphone_mute", 9, 1, Kind::Bool, 0, {}, {}, "", "Headphone output mute"},
        {"direct_monitor", 10, 2, Kind::Fixed, 8, Limits{0, 100, 5}, {}, "%",
         "Direct monitoring/playback mix"},
        {"volume_select", 12, 1, Kind::Unsigned, 0, Limits{1, 3, 1}, knob, "",
         "What the front knob controls"},
        {"all_leds_off", 13, 1, Kind::Bool, 0, {}, {}, "", "All LEDs off"},
    };
    if (with_flip_and_lock) {
        f.push_back({"leds_flip", 14, 1, Kind::Bool, 0, {}, {}, "",
                     "Reverse LED and knob direction (mic mounted upside down)"});
        f.push_back({"gain_lock", 15, 1, Kind::Bool, 0, {}, {}, "",
                     "Ignore input-volume (SET_CUR) requests sent by the OS"});
    }
    return f;
}

MessageDef status_message() {
    return {"status", 1, 8, false,
            {{"touch_pressed_ms", 0, 4, Kind::Unsigned, 0, {}, {}, "ms", "Touch key press time"},
             {"touch_signal", 4, 4, Kind::Unsigned, 0, {}, {}, "", "Touch key raw signal"}}};
}

MessageDef version_message(uint16_t serial_len) {
    const uint16_t thr = static_cast<uint16_t>(0x24 + serial_len);
    return {"version",
            kVersionMessageId,
            static_cast<uint16_t>(thr + 4),
            false,
            {{"api_version/major", 0, 1, Kind::Unsigned, 0, {}, {}, "", "API version"},
             {"api_version/minor", 1, 1, Kind::Unsigned, 0, {}, {}, "", "API version"},
             {"vbus_voltage", 2, 2, Kind::Fixed, 8, {}, {}, "V", "VBUS voltage (factory test)"},
             {"mic_type", 4, 1, Kind::Unsigned, 0, {}, {}, "", "PCB type: Schubert=1, Strauss=2"},
             {"hw_version_board", 5, 1, Kind::Unsigned, 0, {}, {}, "", "Board hardware version"},
             {"bl/major", 6, 1, Kind::Unsigned, 0, {}, {}, "", "Bootloader version"},
             {"bl/minor", 7, 1, Kind::Unsigned, 0, {}, {}, "", "Bootloader version"},
             {"bl/patch", 8, 1, Kind::Unsigned, 0, {}, {}, "", "Bootloader version"},
             {"bl/build", 9, 4, Kind::Unsigned, 0, {}, {}, "", "Bootloader build"},
             {"bl/commit", 13, 8, Kind::Chars, 0, {}, {}, "", "Bootloader commit"},
             {"fw/major", 21, 1, Kind::Unsigned, 0, {}, {}, "", "Firmware version"},
             {"fw/minor", 22, 1, Kind::Unsigned, 0, {}, {}, "", "Firmware version"},
             {"fw/patch", 23, 1, Kind::Unsigned, 0, {}, {}, "", "Firmware version"},
             {"fw/build", 24, 4, Kind::Unsigned, 0, {}, {}, "", "Firmware build"},
             {"fw/commit", 28, 8, Kind::Chars, 0, {}, {}, "", "Firmware commit"},
             {"serial", 36, serial_len, Kind::Chars, 0, {}, {}, "", "Serial number"},
             {"touch_signal_thr/lower", thr, 2, Kind::Unsigned, 0, {}, {}, "",
              "Touch key lower threshold"},
             {"touch_signal_thr/upper", static_cast<uint16_t>(thr + 2), 2, Kind::Unsigned, 0, {},
              {}, "", "Touch key upper threshold"}}};
}

std::vector<ApiSchema> build() {
    return {
        {5, 4, {{"config", 0, 16, true, config_fields(true)}, status_message(), version_message(14)}},
        {5, 3, {{"config", 0, 16, true, config_fields(true)}, status_message(), version_message(12)}},
        {5, 2, {{"config", 0, 14, true, config_fields(false)}, status_message(), version_message(12)}},
    };
}

std::string strip(std::string_view key) {
    while (!key.empty() && key.front() == '/') key.remove_prefix(1);
    return std::string(key);
}

}  // namespace

const FieldDef* MessageDef::field(std::string_view key) const {
    for (const auto& f : fields)
        if (f.key == key) return &f;
    return nullptr;
}

const MessageDef* ApiSchema::message(std::string_view name) const {
    for (const auto& m : messages)
        if (m.name == name) return &m;
    return nullptr;
}

const std::vector<ApiSchema>& wave3_schemas() {
    static const std::vector<ApiSchema> schemas = build();
    return schemas;
}

const ApiSchema* select_schema(uint8_t major, uint8_t minor) {
    const ApiSchema* best = nullptr;
    for (const auto& s : wave3_schemas()) {
        if (s.major != major || s.minor > minor) continue;
        if (!best || s.minor > best->minor) best = &s;
    }
    return best;
}

bool resolve_key(const ApiSchema& schema, std::string_view key, const MessageDef*& message,
                 const FieldDef*& field) {
    static const std::map<std::string, std::string> aliases = {
        {"gain", "input_gain"},          {"mute", "input_mute"},
        {"clipguard", "clipguard_enable"}, {"lowcut", "lowcut_enable"},
        {"headphone", "headphone_volume"}, {"headphones", "headphone_volume"},
        {"mix", "direct_monitor"},       {"knob", "volume_select"},
        {"leds_off", "all_leds_off"},
    };
    std::string k = strip(key);
    std::string msg_name = "config";
    if (auto slash = k.find('/'); slash != std::string::npos) {
        std::string head = k.substr(0, slash);
        if (schema.message(head)) {
            msg_name = head;
            k = k.substr(slash + 1);
        }
    }
    if (auto it = aliases.find(k); it != aliases.end() && msg_name == "config") k = it->second;
    message = schema.message(msg_name);
    if (!message) return false;
    field = message->field(k);
    return field != nullptr;
}

}  // namespace wave3
