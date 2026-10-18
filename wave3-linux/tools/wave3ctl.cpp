// wave3ctl: command-line control for the Elgato Wave:3 on Linux (and macOS).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "wave3/device.hpp"

using namespace wave3;

namespace {

const char* kUsage =
    "usage: wave3ctl [options] <command> [args]\n"
    "\n"
    "commands:\n"
    "  list                     list connected Wave:3 microphones\n"
    "  info                     firmware, serial and protocol details\n"
    "  get [key...]             show settings (all when no key is given)\n"
    "  set key=value...         change settings, e.g. set gain=22.5 lowcut=on knob=mix\n"
    "  watch                    print setting changes made on the mic (knob, touch mute)\n"
    "  dump                     hex dump of every message\n"
    "  schema                   list keys, ranges and descriptions for the connected mic\n"
    "\n"
    "options:\n"
    "  --serial SN              pick a mic by serial number\n"
    "  --json                   machine-readable output for get/info\n"
    "  --interval MS            poll interval for watch (default 200)\n"
    "  --interface N            force the interface number used in wIndex\n"
    "  --detach-audio           Linux fallback: detach snd-usb-audio while running\n"
    "  -v, --verbose            show how the device is reached\n"
    "\n"
    "keys: gain, mute, clipguard, lowcut, headphone, headphone_mute, mix, knob\n"
    "      (mic|headphone|mix), all_leds_off, leds_flip, gain_lock\n";

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

struct Args {
    OpenOptions open;
    bool json = false;
    bool verbose = false;
    int interval_ms = 200;
    std::string command;
    std::vector<std::string> rest;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; i++) {
        std::string s = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw ValueError(s + " needs a value");
            return argv[++i];
        };
        if (s == "--serial") a.open.serial = next();
        else if (s == "--interface") a.open.interface_number = std::stoi(next());
        else if (s == "--detach-audio") a.open.detach_audio = true;
        else if (s == "--json") a.json = true;
        else if (s == "--interval") a.interval_ms = std::stoi(next());
        else if (s == "-v" || s == "--verbose") a.verbose = true;
        else if (s == "-h" || s == "--help") a.command = "help";
        else if (a.command.empty()) a.command = s;
        else a.rest.push_back(s);
    }
    return a;
}

std::unique_ptr<Device> open_device(const Args& a, UsbTransport** usb_out = nullptr) {
    auto usb = std::make_unique<UsbTransport>(a.open);
    UsbTransport* raw = usb.get();
    if (usb_out) *usb_out = raw;
    if (a.verbose)
        std::fprintf(stderr, "Wave:3 %s via %s\n", raw->serial().c_str(), raw->route().c_str());
    return std::make_unique<Device>(std::move(usb));
}

std::string hex(const std::vector<uint8_t>& bytes) {
    std::string out;
    char buf[4];
    for (size_t i = 0; i < bytes.size(); i++) {
        std::snprintf(buf, sizeof buf, "%02x", bytes[i]);
        out += (i ? " " : "") + std::string(buf);
    }
    return out;
}

void print_message(const MessageDef& m, const std::map<std::string, Value>& values,
                   const std::vector<std::string>& only) {
    for (const auto& f : m.fields) {
        if (!only.empty() && std::find(only.begin(), only.end(), f.key) == only.end()) continue;
        std::printf("%-18s %s\n", f.key.c_str(), format_value(f, values.at(f.key)).c_str());
    }
}

std::string json_object(const MessageDef& m, const std::map<std::string, Value>& values,
                        const std::vector<std::string>& only) {
    std::string out = "{";
    bool first = true;
    for (const auto& f : m.fields) {
        if (!only.empty() && std::find(only.begin(), only.end(), f.key) == only.end()) continue;
        out += (first ? "" : ",") + json_escape(f.key) + ":" + json_value(values.at(f.key));
        first = false;
    }
    return out + "}";
}

int cmd_list() {
    auto devices = UsbTransport::list();
    if (devices.empty()) {
        std::puts("no Wave:3 found");
        return 1;
    }
    for (const auto& d : devices) {
        std::printf("bus %03u device %03u  %04x:%04x  %s%s\n", d.bus, d.address, kElgatoVid, d.pid,
                    d.serial.empty() ? "(serial unavailable)" : d.serial.c_str(),
                    d.pid == kWave3DfuPid ? "  [firmware-update mode]" : "");
        if (!d.error.empty()) std::printf("  cannot open: %s\n", d.error.c_str());
    }
    return 0;
}

int cmd_info(const Args& a) {
    UsbTransport* usb = nullptr;
    auto dev = open_device(a, &usb);
    const MessageDef& vm = *dev->schema().message("version");
    auto v = dev->read(vm);
    auto num = [&](const char* k) { return std::to_string(std::get<uint64_t>(v.at(k))); };
    std::string fw = num("fw/major") + "." + num("fw/minor") + "." + num("fw/patch");
    std::string bl = num("bl/major") + "." + num("bl/minor") + "." + num("bl/patch");
    if (a.json) {
        std::printf("{\"serial\":%s,\"api\":\"%u.%u\",\"schema\":\"%u.%u\",\"firmware\":\"%s\","
                    "\"firmware_commit\":%s,\"bootloader\":\"%s\",\"route\":%s,\"version\":%s}\n",
                    json_escape(std::get<std::string>(v.at("serial"))).c_str(), dev->api_major(),
                    dev->api_minor(), dev->schema().major, dev->schema().minor, fw.c_str(),
                    json_escape(std::get<std::string>(v.at("fw/commit"))).c_str(), bl.c_str(),
                    json_escape(usb->route()).c_str(), json_object(vm, v, {}).c_str());
        return 0;
    }
    std::printf("serial       %s\n", std::get<std::string>(v.at("serial")).c_str());
    std::printf("firmware     %s (build %s, commit %s)\n", fw.c_str(), num("fw/build").c_str(),
                std::get<std::string>(v.at("fw/commit")).c_str());
    std::printf("bootloader   %s (build %s)\n", bl.c_str(), num("bl/build").c_str());
    std::printf("board        hw %s, type %s\n", num("hw_version_board").c_str(), num("mic_type").c_str());
    std::printf("control API  %u.%u (layout %u.%u)\n", dev->api_major(), dev->api_minor(),
                dev->schema().major, dev->schema().minor);
    std::printf("route        %s\n", usb->route().c_str());
    return 0;
}

int cmd_get(const Args& a) {
    auto dev = open_device(a);
    std::vector<std::string> only;
    const MessageDef& config = *dev->schema().message("config");
    for (const auto& k : a.rest) {
        const MessageDef* m = nullptr;
        const FieldDef* f = nullptr;
        if (!resolve_key(dev->schema(), k, m, f) || m != &config)
            throw ValueError("unknown setting '" + k + "'");
        only.push_back(f->key);
    }
    auto values = dev->read(config);
    if (a.json) std::printf("%s\n", json_object(config, values, only).c_str());
    else print_message(config, values, only);
    return 0;
}

int cmd_set(const Args& a) {
    if (a.rest.empty()) throw ValueError("set needs key=value arguments");
    std::vector<std::pair<std::string, std::string>> assignments;
    for (const auto& s : a.rest) {
        auto eq = s.find('=');
        if (eq == std::string::npos || eq == 0) throw ValueError("expected key=value, got '" + s + "'");
        assignments.emplace_back(s.substr(0, eq), s.substr(eq + 1));
    }
    auto dev = open_device(a);
    int status = 0;
    for (const auto& c : dev->set(assignments)) {
        bool ok = format_value(*c.field, c.after) == format_value(*c.field, c.requested);
        std::printf("%-18s %s -> %s%s%s\n", c.field->key.c_str(),
                    format_value(*c.field, c.before).c_str(), format_value(*c.field, c.after).c_str(),
                    c.snapped ? "  (rounded to step)" : "",
                    ok ? "" : ("  (requested " + format_value(*c.field, c.requested) + ")").c_str());
        if (!ok) status = 1;
    }
    return status;
}

int cmd_watch(const Args& a) {
    auto dev = open_device(a);
    const MessageDef& config = *dev->schema().message("config");
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    auto last = dev->read(config);
    print_message(config, last, {});
    std::fflush(stdout);
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(a.interval_ms));
        auto now = dev->read(config);
        for (const auto& f : config.fields) {
            if (format_value(f, now.at(f.key)) == format_value(f, last.at(f.key))) continue;
            if (a.json)
                std::printf("{\"key\":%s,\"value\":%s}\n", json_escape(f.key).c_str(),
                            json_value(now.at(f.key)).c_str());
            else
                std::printf("%-18s %s -> %s\n", f.key.c_str(), format_value(f, last.at(f.key)).c_str(),
                            format_value(f, now.at(f.key)).c_str());
        }
        std::fflush(stdout);
        last = std::move(now);
    }
    return 0;
}

int cmd_dump(const Args& a) {
    auto dev = open_device(a);
    for (const auto& m : dev->schema().messages)
        std::printf("%-8s id %2u  %s\n", m.name.c_str(), m.id, hex(dev->read_raw(m)).c_str());
    return 0;
}

int cmd_schema(const Args& a) {
    auto dev = open_device(a);
    for (const auto& m : dev->schema().messages) {
        std::printf("%s (id %u, %u bytes, %s)\n", m.name.c_str(), m.id, m.size,
                    m.writable ? "read/write" : "read-only");
        for (const auto& f : m.fields) {
            std::string range;
            if (!f.enumeration.empty()) {
                for (const auto& e : f.enumeration) range += (range.empty() ? "" : "|") + e.name;
            } else if (f.limits) {
                char buf[64];
                std::snprintf(buf, sizeof buf, "%g..%g step %g", f.limits->min, f.limits->max,
                              f.limits->step);
                range = buf;
            } else if (f.kind == Kind::Bool) {
                range = "on|off";
            }
            std::printf("  %-24s @%-2u %-20s %s%s\n", f.key.c_str(), f.offset, range.c_str(),
                        f.description.c_str(), f.unit.empty() ? "" : (" [" + f.unit + "]").c_str());
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        Args a = parse_args(argc, argv);
        if (a.command.empty() || a.command == "help") {
            std::fputs(kUsage, a.command.empty() ? stderr : stdout);
            return a.command.empty() ? 2 : 0;
        }
        if (a.command == "list") return cmd_list();
        if (a.command == "info") return cmd_info(a);
        if (a.command == "get") return cmd_get(a);
        if (a.command == "set") return cmd_set(a);
        if (a.command == "watch") return cmd_watch(a);
        if (a.command == "dump") return cmd_dump(a);
        if (a.command == "schema") return cmd_schema(a);
        std::fprintf(stderr, "unknown command '%s'\n\n%s", a.command.c_str(), kUsage);
        return 2;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "wave3ctl: %s\n", e.what());
        return 1;
    }
}
