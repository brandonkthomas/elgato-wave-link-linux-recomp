#include "Model.hpp"

#include <QJsonArray>
#include <algorithm>
#include <cmath>

namespace wl {

QJsonArray effectsToJson(const QList<EffectConfig>& effects) {
    QJsonArray out;
    for (const EffectConfig& e : effects) {
        QJsonObject params;
        for (auto it = e.params.begin(); it != e.params.end(); ++it) params[it.key()] = it.value();
        QJsonObject o{{"id", e.id}, {"type", e.type}, {"bypass", e.bypass}, {"params", params}};
        if (!e.name.isEmpty()) o["name"] = e.name;
        out.append(o);
    }
    return out;
}

const std::array<SlotDef, kSoftwareSlots> kSoftwareSlotDefs = {{
    {"PCM_OUT_00_V_00_SD1", "System", "wavelink.input.1"},
    {"PCM_OUT_00_V_02_SD2", "Music", "wavelink.input.2"},
    {"PCM_OUT_00_V_04_SD3", "Browser", "wavelink.input.3"},
    {"PCM_OUT_00_V_06_SD4", "Voice", "wavelink.input.4"},
    {"PCM_OUT_00_V_08_SD5", "SFX", "wavelink.input.5"},
    {"PCM_OUT_00_V_10_SD6", "Game", "wavelink.input.6"},
    {"PCM_OUT_00_V_12_SD7", "Aux 1", "wavelink.input.7"},
    {"PCM_OUT_00_V_14_SD8", "Aux 2", "wavelink.input.8"},
}};

const std::array<SlotDef, kMixSlots> kMixSlotDefs = {{
    {"PCM_IN_01_V_00_SD1", "Personal Mix", "wavelink.mix.1"},
    {"PCM_IN_01_V_02_SD2", "Chat Mix", "wavelink.mix.2"},
    {"PCM_IN_01_V_04_SD3", "Stream Mix", "wavelink.mix.3"},
    {"PCM_IN_01_V_06_SD4", "Record Mix", "wavelink.mix.4"},
    {"PCM_IN_01_V_08_SD5", "Aux Mix", "wavelink.mix.5"},
}};

float sliderToDb(float v) {
    return 60.0f * std::clamp(v, 0.0f, 1.0f) - 60.0f;
}

float sliderToGain(float v) {
    if (v <= 0.0f) return 0.0f;
    return std::pow(10.0f, sliderToDb(v) / 20.0f);
}

Model Model::defaults() {
    Model m;
    // AddAudioMix hands out mix ids in slot order, so the DefaultMixChannelNames land in slots 0-2.
    const char* names[] = {"Personal Mix", "Stream Mix", "Chat Mix"};
    for (int slot = 0; slot < 3; slot++) m.mixes.push_back({kMixSlotDefs[slot].id, slot, names[slot], {}});
    m.inputs.push_back({kSoftwareSlotDefs[0].id, 0, InputKind::Software, kSoftwareSlotDefs[0].name, {}, false});
    // Configuration.InitializeDefaultConfiguration: System plays in the Personal and Stream mixes.
    m.cells[0][0].active = true;
    m.cells[0][1].active = true;
    return m;
}

Input* Model::input(const QString& id) {
    auto it = std::find_if(inputs.begin(), inputs.end(), [&](const Input& i) { return i.id == id; });
    return it == inputs.end() ? nullptr : &*it;
}

const Input* Model::input(const QString& id) const {
    return const_cast<Model*>(this)->input(id);
}

Mix* Model::mix(const QString& id) {
    auto it = std::find_if(mixes.begin(), mixes.end(), [&](const Mix& m) { return m.id == id; });
    return it == mixes.end() ? nullptr : &*it;
}

const Mix* Model::mix(const QString& id) const {
    return const_cast<Model*>(this)->mix(id);
}

namespace {
int freeSlot(int first, int count, auto used) {
    for (int s = first; s < first + count; s++)
        if (!used(s)) return s;
    return -1;
}
}  // namespace

int Model::freeSoftwareSlot() const {
    return freeSlot(0, kSoftwareSlots, [&](int s) {
        return std::any_of(inputs.begin(), inputs.end(), [&](const Input& i) { return i.slot == s; });
    });
}

int Model::freeHardwareSlot() const {
    return freeSlot(kSoftwareSlots, kHardwareSlots, [&](int s) {
        return std::any_of(inputs.begin(), inputs.end(), [&](const Input& i) { return i.slot == s; });
    });
}

int Model::freeMixSlot() const {
    return freeSlot(0, kMixSlots, [&](int s) {
        return std::any_of(mixes.begin(), mixes.end(), [&](const Mix& m) { return m.slot == s; });
    });
}

float Model::cellGain(int inputSlot, int mixSlot) const {
    auto in = std::find_if(inputs.begin(), inputs.end(), [&](const Input& i) { return i.slot == inputSlot; });
    auto mx = std::find_if(mixes.begin(), mixes.end(), [&](const Mix& m) { return m.slot == mixSlot; });
    if (in == inputs.end() || mx == mixes.end() || in->hidden) return 0.0f;
    const Cell& c = cells[inputSlot][mixSlot];
    if (!c.active || c.muted) return 0.0f;
    float gain = sliderToGain(c.volume);
    // Software channels carry their own volume on their PipeWire sink; hardware inputs apply it here,
    // with the desktop's cubic volume curve.
    if (in->kind == InputKind::Hardware) {
        if (in->masterMuted) return 0.0f;
        gain *= in->masterVolume * in->masterVolume * in->masterVolume;
    }
    return gain;
}

QJsonObject Model::toJson() const {
    QJsonArray ins;
    for (const auto& i : inputs) {
        QJsonObject o{{"id", i.id}, {"slot", i.slot}, {"name", i.name}, {"hidden", i.hidden},
                      {"volume", i.masterVolume}, {"muted", i.masterMuted},
                      {"effects", effectsToJson(i.effects)},
                      {"kind", i.kind == InputKind::Software ? "software" : "hardware"}};
        if (i.kind == InputKind::Hardware) o["device"] = i.deviceNodeName;
        ins.append(o);
    }
    QJsonArray mxs;
    for (const auto& m : mixes)
        mxs.append(QJsonObject{{"id", m.id}, {"slot", m.slot}, {"name", m.name},
                               {"outputs", QJsonArray::fromStringList(m.outputDevices)}});
    QJsonArray cellArr;
    for (const auto& i : inputs)
        for (const auto& m : mixes) {
            const Cell& c = cells[i.slot][m.slot];
            cellArr.append(QJsonObject{{"input", i.id}, {"mix", m.id}, {"volume", c.volume},
                                       {"muted", c.muted}, {"active", c.active}});
        }
    QJsonObject routes;
    for (auto it = appRoutes.begin(); it != appRoutes.end(); ++it) routes[it.key()] = it.value();
    return {{"version", 1}, {"inputs", ins}, {"mixes", mxs}, {"cells", cellArr},
            {"manageDefaultSink", manageDefaultSink}, {"appRoutes", routes},
            {"soundCheckOutput", soundCheckOutput}, {"monitorOutputChosen", monitorOutputChosen}};
}

Model Model::fromJson(const QJsonObject& json) {
    Model m;
    for (const auto& v : json["inputs"].toArray()) {
        QJsonObject o = v.toObject();
        Input i;
        i.id = o["id"].toString();
        i.slot = o["slot"].toInt(-1);
        i.name = o["name"].toString();
        i.hidden = o["hidden"].toBool();
        i.kind = o["kind"].toString() == "hardware" ? InputKind::Hardware : InputKind::Software;
        i.deviceNodeName = o["device"].toString();
        i.masterVolume = std::clamp(float(o["volume"].toDouble(1.0)), 0.0f, 1.0f);
        i.masterMuted = o["muted"].toBool();
        for (const auto& e : o["effects"].toArray()) {
            const QJsonObject fo = e.toObject();
            EffectConfig fx{fo["id"].toString(), fo["type"].toString(), fo["bypass"].toBool(), {}, fo["name"].toString()};
            const QJsonObject params = fo["params"].toObject();
            for (auto it = params.begin(); it != params.end(); ++it) fx.params[it.key()] = it.value().toDouble();
            if (!fx.id.isEmpty() && !fx.type.isEmpty()) i.effects.append(fx);
        }
        bool validSlot = i.kind == InputKind::Software ? (i.slot >= 0 && i.slot < kSoftwareSlots)
                                                       : (i.slot >= kSoftwareSlots && i.slot < kInputSlots);
        if (!i.id.isEmpty() && validSlot && !m.input(i.id)) m.inputs.push_back(i);
    }
    for (const auto& v : json["mixes"].toArray()) {
        QJsonObject o = v.toObject();
        Mix x;
        x.id = o["id"].toString();
        x.slot = o["slot"].toInt(-1);
        x.name = o["name"].toString();
        for (const auto& d : o["outputs"].toArray()) x.outputDevices.append(d.toString());
        if (!x.id.isEmpty() && x.slot >= 0 && x.slot < kMixSlots && !m.mix(x.id)) m.mixes.push_back(x);
    }
    for (const auto& v : json["cells"].toArray()) {
        QJsonObject o = v.toObject();
        const Input* i = m.input(o["input"].toString());
        const Mix* x = m.mix(o["mix"].toString());
        if (!i || !x) continue;
        Cell& c = m.cells[i->slot][x->slot];
        c.volume = std::clamp(float(o["volume"].toDouble(1.0)), 0.0f, 1.0f);
        c.muted = o["muted"].toBool();
        c.active = o["active"].toBool(true);
    }
    m.manageDefaultSink = json["manageDefaultSink"].toBool(true);
    m.soundCheckOutput = json["soundCheckOutput"].toString();
    m.monitorOutputChosen = json["monitorOutputChosen"].toBool(false);
    const QJsonObject routes = json["appRoutes"].toObject();
    for (auto it = routes.begin(); it != routes.end(); ++it)
        if (m.input(it.value().toString())) m.appRoutes[it.key()] = it.value().toString();
    return m;
}

}  // namespace wl
