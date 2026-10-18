// Message and field layout of the Elgato Wave:3 vendor control protocol.
//
// The device exposes a small set of fixed-size "messages" (packed structs) that are read
// and written whole. Layouts differ per firmware API version; see docs/PROTOCOL.md for the
// evidence each table is derived from.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wave3 {

enum class Kind {
    Bool,      // one byte, 0 = false, anything else = true
    Unsigned,  // little-endian unsigned integer of `size` bytes
    Fixed,     // little-endian signed two's-complement integer scaled by 2^-frac_bits
    Chars,     // fixed-length ASCII, NUL padded
};

struct Limits {
    double min;
    double max;
    double step;
};

struct EnumEntry {
    std::string name;
    unsigned value;
};

struct FieldDef {
    std::string key;  // e.g. "input_gain"
    uint16_t offset;
    uint16_t size;
    Kind kind;
    uint8_t frac_bits = 0;
    std::optional<Limits> limits;
    std::vector<EnumEntry> enumeration;
    std::string unit;
    std::string description;
};

struct MessageDef {
    std::string name;  // e.g. "config"
    uint8_t id;        // UAC1 memory offset (wValue)
    uint16_t size;
    bool writable;
    std::vector<FieldDef> fields;

    const FieldDef* field(std::string_view key) const;
};

struct ApiSchema {
    uint8_t major;
    uint8_t minor;
    std::vector<MessageDef> messages;

    const MessageDef* message(std::string_view name) const;
};

// Message id of /version; its first two bytes are the API major/minor version and are
// readable before the schema is known.
constexpr uint8_t kVersionMessageId = 10;

// All Wave:3 schemas known to Wave Link 3.4 (API 5.2, 5.3, 5.4).
const std::vector<ApiSchema>& wave3_schemas();

// Same rule Wave Link uses: same major version, highest minor version not newer than the
// device's. Returns nullptr when nothing is compatible.
const ApiSchema* select_schema(uint8_t major, uint8_t minor);

// Resolves user-facing names ("gain", "config/input_gain", "/config/input_gain") to a
// message/field pair. Returns false when the key is unknown.
bool resolve_key(const ApiSchema& schema, std::string_view key, const MessageDef*& message,
                 const FieldDef*& field);

}  // namespace wave3
