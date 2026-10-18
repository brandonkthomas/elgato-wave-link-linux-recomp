#include "wave3/device.hpp"

namespace wave3 {

Device::Device(std::unique_ptr<Transport> transport) : transport_(std::move(transport)) {
    std::vector<uint8_t> v = transport_->get_mem(kVersionMessageId, 2);
    api_major_ = v[0];
    api_minor_ = v[1];
    schema_ = select_schema(api_major_, api_minor_);
    if (!schema_)
        throw UsbError("unsupported Wave:3 control API " + std::to_string(api_major_) + "." +
                       std::to_string(api_minor_) + " (known: 5.2, 5.3, 5.4)");
}

std::vector<uint8_t> Device::read_raw(const MessageDef& message) {
    return transport_->get_mem(message.id, message.size);
}

std::map<std::string, Value> Device::read(const MessageDef& message) {
    std::vector<uint8_t> raw = read_raw(message);
    std::map<std::string, Value> out;
    for (const auto& f : message.fields) out.emplace(f.key, decode(f, raw));
    return out;
}

std::vector<FieldChange> Device::set(
    const std::vector<std::pair<std::string, std::string>>& assignments) {
    struct Pending {
        const FieldDef* field;
        Value value;
        bool snapped;
    };
    std::map<const MessageDef*, std::vector<Pending>> by_message;
    for (const auto& [key, text] : assignments) {
        const MessageDef* m = nullptr;
        const FieldDef* f = nullptr;
        if (!resolve_key(*schema_, key, m, f)) throw ValueError("unknown setting '" + key + "'");
        if (!m->writable) throw ValueError(key + " is read-only");
        bool snapped = false;
        Value v = parse_value(*f, text, &snapped);
        by_message[m].push_back({f, v, snapped});
    }

    std::vector<FieldChange> changes;
    for (const auto& [m, pending] : by_message) {
        std::vector<uint8_t> raw = read_raw(*m);
        std::vector<uint8_t> next = raw;
        for (const auto& p : pending) {
            changes.push_back({p.field, decode(*p.field, raw), p.value, {}, p.snapped});
            encode(*p.field, p.value, next);
        }
        if (next != raw) transport_->set_mem(m->id, next);
        std::vector<uint8_t> verify = read_raw(*m);
        for (auto& c : changes)
            if (m->field(c.field->key) == c.field) c.after = decode(*c.field, verify);
    }
    return changes;
}

}  // namespace wave3
