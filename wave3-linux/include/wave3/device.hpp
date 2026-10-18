// High-level Wave:3 access: API detection, typed reads and verified read-modify-write updates.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "wave3/codec.hpp"
#include "wave3/schema.hpp"
#include "wave3/transport.hpp"

namespace wave3 {

struct FieldChange {
    const FieldDef* field;
    Value before;
    Value requested;
    Value after;  // read back from the device after the write
    bool snapped = false;
};

class Device {
public:
    explicit Device(std::unique_ptr<Transport> transport);

    const ApiSchema& schema() const { return *schema_; }
    uint8_t api_major() const { return api_major_; }
    uint8_t api_minor() const { return api_minor_; }
    Transport& transport() { return *transport_; }

    std::vector<uint8_t> read_raw(const MessageDef& message);
    std::map<std::string, Value> read(const MessageDef& message);

    // Applies key=value assignments in one write per message. Each message is read, only the
    // requested fields are changed (unknown bytes are preserved) and the result is read back.
    std::vector<FieldChange> set(const std::vector<std::pair<std::string, std::string>>& assignments);

private:
    std::unique_ptr<Transport> transport_;
    const ApiSchema* schema_ = nullptr;
    uint8_t api_major_ = 0;
    uint8_t api_minor_ = 0;
};

}  // namespace wave3
