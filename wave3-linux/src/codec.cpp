#include "wave3/codec.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace wave3 {

namespace {

void check_bounds(const FieldDef& field, const std::vector<uint8_t>& message) {
    if (size_t(field.offset) + field.size > message.size())
        throw ValueError("field " + field.key + " lies outside the message");
}

uint64_t read_le(const std::vector<uint8_t>& m, size_t off, size_t size) {
    uint64_t v = 0;
    for (size_t i = 0; i < size; i++) v |= uint64_t(m[off + i]) << (8 * i);
    return v;
}

void write_le(std::vector<uint8_t>& m, size_t off, size_t size, uint64_t v) {
    for (size_t i = 0; i < size; i++) m[off + i] = uint8_t(v >> (8 * i));
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trim_number(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4f", v);
    std::string s = buf;
    s.erase(s.find_last_not_of('0') + 1);
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    return s;
}

void check_limits(const FieldDef& field, double v) {
    if (field.limits && (v < field.limits->min - 1e-9 || v > field.limits->max + 1e-9))
        throw ValueError(field.key + " must be between " + trim_number(field.limits->min) + " and " +
                         trim_number(field.limits->max) + (field.unit.empty() ? "" : " " + field.unit));
}

}  // namespace

Value decode(const FieldDef& field, const std::vector<uint8_t>& message) {
    check_bounds(field, message);
    switch (field.kind) {
    case Kind::Bool:
        return message[field.offset] != 0;
    case Kind::Unsigned:
        return read_le(message, field.offset, field.size);
    case Kind::Fixed: {
        uint64_t raw = read_le(message, field.offset, field.size);
        int bits = field.size * 8;
        int64_t sraw = bits < 64 && (raw >> (bits - 1)) ? int64_t(raw) - (int64_t(1) << bits)
                                                        : int64_t(raw);
        return double(sraw) / double(1u << field.frac_bits);
    }
    case Kind::Chars: {
        std::string s(message.begin() + field.offset, message.begin() + field.offset + field.size);
        s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
        return s;
    }
    }
    throw ValueError("unknown field kind");
}

void encode(const FieldDef& field, const Value& value, std::vector<uint8_t>& message) {
    check_bounds(field, message);
    switch (field.kind) {
    case Kind::Bool:
        message[field.offset] = std::get<bool>(value) ? 1 : 0;
        return;
    case Kind::Unsigned: {
        uint64_t v = std::get<uint64_t>(value);
        check_limits(field, double(v));
        if (field.size < 8 && v >> (8 * field.size))
            throw ValueError(field.key + " does not fit in " + std::to_string(field.size) + " bytes");
        write_le(message, field.offset, field.size, v);
        return;
    }
    case Kind::Fixed: {
        double v = std::get<double>(value);
        check_limits(field, v);
        int64_t raw = std::llround(v * double(1u << field.frac_bits));
        int bits = field.size * 8;
        int64_t lo = -(int64_t(1) << (bits - 1)), hi = (int64_t(1) << (bits - 1)) - 1;
        if (raw < lo || raw > hi) throw ValueError(field.key + " is not representable");
        write_le(message, field.offset, field.size, uint64_t(raw));
        return;
    }
    case Kind::Chars: {
        const std::string& s = std::get<std::string>(value);
        if (s.size() > field.size) throw ValueError(field.key + " is too long");
        for (size_t i = 0; i < field.size; i++)
            message[field.offset + i] = i < s.size() ? uint8_t(s[i]) : 0;
        return;
    }
    }
}

Value parse_value(const FieldDef& field, const std::string& text, bool* snapped) {
    if (snapped) *snapped = false;
    const std::string t = lower(text);
    switch (field.kind) {
    case Kind::Bool:
        if (t == "1" || t == "on" || t == "true" || t == "yes" || t == "enable" || t == "enabled")
            return true;
        if (t == "0" || t == "off" || t == "false" || t == "no" || t == "disable" || t == "disabled")
            return false;
        throw ValueError(field.key + " expects on/off");
    case Kind::Unsigned: {
        for (const auto& e : field.enumeration)
            if (t == e.name) return uint64_t(e.value);
        size_t used = 0;
        unsigned long long v = 0;
        try {
            v = std::stoull(t, &used, 0);
        } catch (const std::exception&) {
            used = 0;
        }
        if (used == 0 || used != t.size() || t[0] == '-') {
            std::string names;
            for (const auto& e : field.enumeration) names += (names.empty() ? "" : "/") + e.name;
            throw ValueError(field.key + " expects a non-negative integer" +
                             (names.empty() ? "" : " or one of " + names));
        }
        check_limits(field, double(v));
        return uint64_t(v);
    }
    case Kind::Fixed: {
        std::string n = t;
        if (!field.unit.empty()) {
            const std::string u = lower(field.unit);
            if (n.size() > u.size() && n.compare(n.size() - u.size(), u.size(), u) == 0)
                n.erase(n.size() - u.size());
        }
        size_t used = 0;
        double v = 0;
        try {
            v = std::stod(n, &used);
        } catch (const std::exception&) {
            used = 0;
        }
        while (used < n.size() && std::isspace(static_cast<unsigned char>(n[used]))) used++;
        if (used == 0 || used != n.size() || !std::isfinite(v))
            throw ValueError(field.key + " expects a number");
        check_limits(field, v);
        if (field.limits && field.limits->step > 0) {
            double s = field.limits->min +
                       std::round((v - field.limits->min) / field.limits->step) * field.limits->step;
            s = std::clamp(s, field.limits->min, field.limits->max);
            if (snapped && std::fabs(s - v) > 1e-9) *snapped = true;
            v = s;
        }
        return v;
    }
    case Kind::Chars:
        return text;
    }
    throw ValueError("unknown field kind");
}

std::string format_value(const FieldDef& field, const Value& value) {
    if (auto b = std::get_if<bool>(&value)) return *b ? "on" : "off";
    if (auto u = std::get_if<uint64_t>(&value)) {
        for (const auto& e : field.enumeration)
            if (e.value == *u) return e.name + " (" + std::to_string(*u) + ")";
        return std::to_string(*u) + (field.unit.empty() ? "" : " " + field.unit);
    }
    if (auto d = std::get_if<double>(&value))
        return trim_number(*d) + (field.unit.empty() ? "" : " " + field.unit);
    return std::get<std::string>(value);
}

std::string json_escape(const std::string& text) {
    std::ostringstream o;
    o << '"';
    for (unsigned char c : text) {
        switch (c) {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\n': o << "\\n"; break;
        case '\t': o << "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                o << buf;
            } else {
                o << c;
            }
        }
    }
    o << '"';
    return o.str();
}

std::string json_value(const Value& value) {
    if (auto b = std::get_if<bool>(&value)) return *b ? "true" : "false";
    if (auto u = std::get_if<uint64_t>(&value)) return std::to_string(*u);
    if (auto d = std::get_if<double>(&value)) return trim_number(*d);
    return json_escape(std::get<std::string>(value));
}

}  // namespace wave3
