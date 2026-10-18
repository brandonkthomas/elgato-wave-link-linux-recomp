// Conversion between raw message bytes and typed field values.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "wave3/schema.hpp"

namespace wave3 {

using Value = std::variant<bool, uint64_t, double, std::string>;

class ValueError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

Value decode(const FieldDef& field, const std::vector<uint8_t>& message);

// Writes `value` into `message` at the field's position. Throws ValueError when the value is
// out of range or not representable.
void encode(const FieldDef& field, const Value& value, std::vector<uint8_t>& message);

// Parses user text ("on", "22.5", "headphone", ...) for `field`. Fixed-point values are
// snapped to the field's step; `snapped` reports whether that changed the input.
Value parse_value(const FieldDef& field, const std::string& text, bool* snapped = nullptr);

// Human-readable rendering, e.g. "20 dB", "on", "headphone (2)".
std::string format_value(const FieldDef& field, const Value& value);

// JSON literal for the value (number, true/false or quoted string).
std::string json_value(const Value& value);

std::string json_escape(const std::string& text);

}  // namespace wave3
