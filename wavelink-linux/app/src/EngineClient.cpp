#include "EngineClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>

namespace {

QString key(const QString& input, const QString& mix) {
    return input + QLatin1Char('|') + mix;
}

QPointF peak(const QJsonValue& v) {
    const QJsonArray a = v.toArray();
    return {a.at(0).toDouble(), a.at(1).toDouble()};
}

}  // namespace

EngineClient::EngineClient(QUrl url, QObject* parent) : QObject(parent), url_(std::move(url)) {
    reconnectTimer_.setInterval(1000);
    connect(&reconnectTimer_, &QTimer::timeout, this, [this] { socket_.open(url_); });
    // Slider drags produce many moves; send at most ~60 updates per second per cell.
    volumeThrottle_.setSingleShot(true);
    volumeThrottle_.setInterval(16);
    connect(&volumeThrottle_, &QTimer::timeout, this, &EngineClient::flushPendingVolumes);
    wave3Settle_.setSingleShot(true);
    wave3Settle_.setInterval(400);
    clock_.start();

    connect(&socket_, &QWebSocket::connected, this, [this] {
        reconnectTimer_.stop();
        connected_ = true;
        emit connectedChanged();
        call(QStringLiteral("getState"), {});
        call(QStringLiteral("subscribeMeters"), {{"enabled", true}});
        call(QStringLiteral("getEffectTypes"), {}, [this](const QJsonValue& types) {
            effectTypes_ = types.toArray().toVariantList();
            emit effectTypesChanged();
        });
    });
    connect(&socket_, &QWebSocket::disconnected, this, [this] {
        if (connected_) {
            connected_ = false;
            emit connectedChanged();
        }
        pending_.clear();
        pendingParams_.clear();
        reconnectTimer_.start();
    });
    connect(&socket_, &QWebSocket::textMessageReceived, this, &EngineClient::onMessage);
    socket_.open(url_);
    reconnectTimer_.start();
}

void EngineClient::call(const QString& method, const QJsonObject& params,
                        std::function<void(const QJsonValue&)> onResult) {
    if (!connected_) return;
    const int id = nextId_++;
    if (onResult) pending_[id] = std::move(onResult);
    const QJsonObject request{{"id", id}, {"method", method}, {"params", params}};
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
}

void EngineClient::onMessage(const QString& text) {
    const QJsonObject msg = QJsonDocument::fromJson(text.toUtf8()).object();
    if (msg.contains("id")) {
        if (msg.contains("error")) emit error(msg["error"].toObject()["message"].toString());
        if (auto callback = pending_.take(msg["id"].toInt())) callback(msg["result"]);
        const QJsonObject result = msg["result"].toObject();
        if (result.contains("model")) applyState(result);
        return;
    }
    const QString event = msg["event"].toString();
    const QJsonValue data = msg["data"];
    if (event == "meters") {
        const QJsonObject d = data.toObject();
        const QJsonObject inputs = d["inputs"].toObject();
        for (auto it = inputs.begin(); it != inputs.end(); ++it) inputPeaks_[it.key()] = peak(it.value());
        const QJsonObject mixes = d["mixes"].toObject();
        for (auto it = mixes.begin(); it != mixes.end(); ++it) mixPeaks_[it.key()] = peak(it.value());
        const QJsonObject cells = d["cells"].toObject();
        for (auto it = cells.begin(); it != cells.end(); ++it) cellPeaks_[it.key()] = peak(it.value());
        meterRevision_++;
        emit metersChanged();
    } else if (event == "state") {
        applyState(data.toObject());
    } else if (event == "devices") {
        applyDevices(data.toObject());
    } else if (event == "soundCheck") {
        soundCheck_ = data.toObject().toVariantMap();
        emit soundCheckChanged();
    } else if (event == "effect") {
        const QJsonObject e = data.toObject();
        storeEffects(e["input"].toString(), e["effects"].toArray());
    } else if (event == "wave3") {
        if (wave3Settle_.isActive()) return;
        wave3_ = data.toObject().toVariantMap();
        emit wave3Changed();
    } else if (event == "input") {
        const QJsonObject i = data.toObject();
        const QString id = i["id"].toString();
        if (pendingInputVolumes_.contains(id)) return;
        inputLevels_[id] = QVariantMap{{"volume", i["volume"].toDouble(1.0)}, {"muted", i["muted"].toBool()}};
        inputRevision_++;
        emit inputLevelsChanged();
    } else if (event == "cell") {
        const QJsonObject c = data.toObject();
        // Ignore echoes of a volume we are still dragging.
        if (pendingVolumes_.contains(key(c["input"].toString(), c["mix"].toString()))) return;
        storeCell(c);
        cellRevision_++;
        emit cellsChanged();
    }
}

void EngineClient::applyState(const QJsonObject& state) {
    const QJsonObject model = state["model"].toObject();
    inputs_ = model["inputs"].toArray().toVariantList();
    inputLevels_.clear();
    for (const auto& v : model["inputs"].toArray()) {
        const QJsonObject i = v.toObject();
        inputLevels_[i["id"].toString()] = QVariantMap{{"volume", i["volume"].toDouble(1.0)}, {"muted", i["muted"].toBool()}};
    }
    inputRevision_++;
    emit inputLevelsChanged();
    effects_.clear();
    for (const auto& v : model["inputs"].toArray())
        effects_[v.toObject()["id"].toString()] = v.toObject()["effects"].toArray().toVariantList();
    effectRevision_++;
    emit effectsChanged();
    mixes_ = model["mixes"].toArray().toVariantList();
    appRoutes_ = model["appRoutes"].toObject().toVariantMap();
    manageDefaultSink_ = model["manageDefaultSink"].toBool(true);
    cells_.clear();
    for (const auto& c : model["cells"].toArray()) storeCell(c.toObject());
    cellRevision_++;
    emit stateChanged();
    emit cellsChanged();
    if (state.contains("devices")) applyDevices(state["devices"].toObject());
    if (state.contains("soundCheck")) {
        soundCheck_ = state["soundCheck"].toObject().toVariantMap();
        emit soundCheckChanged();
    }
    if (state.contains("wave3")) {
        wave3_ = state["wave3"].toObject().toVariantMap();
        emit wave3Changed();
    }
}

void EngineClient::applyDevices(const QJsonObject& devices) {
    outputDevices_ = devices["outputs"].toArray().toVariantList();
    inputDevices_ = devices["inputs"].toArray().toVariantList();
    apps_ = devices["apps"].toArray().toVariantList();
    emit devicesChanged();
}

void EngineClient::storeCell(const QJsonObject& c) {
    cells_[key(c["input"].toString(), c["mix"].toString())] =
        QVariantMap{{"volume", c["volume"].toDouble(1.0)}, {"muted", c["muted"].toBool()},
                    {"active", c["active"].toBool(true)}};
}

QVariantMap EngineClient::cell(const QString& input, const QString& mix) const {
    return cells_.value(key(input, mix), QVariantMap{{"volume", 1.0}, {"muted", false}, {"active", false}});
}

void EngineClient::setCellVolume(const QString& input, const QString& mix, double volume) {
    const QString k = key(input, mix);
    cells_[k]["volume"] = volume;
    pendingVolumes_[k] = volume;
    cellRevision_++;
    emit cellsChanged();
    if (!volumeThrottle_.isActive()) volumeThrottle_.start();
}

void EngineClient::flushPendingVolumes() {
    for (auto it = pendingVolumes_.begin(); it != pendingVolumes_.end(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('|'));
        call(QStringLiteral("setCell"), {{"input", parts.value(0)}, {"mix", parts.value(1)}, {"volume", it.value()}});
    }
    pendingVolumes_.clear();
    for (auto it = pendingInputVolumes_.begin(); it != pendingInputVolumes_.end(); ++it)
        call(QStringLiteral("setInput"), {{"input", it.key()}, {"volume", it.value()}});
    pendingInputVolumes_.clear();
    for (auto it = pendingParams_.begin(); it != pendingParams_.end(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('|'));
        call(QStringLiteral("setEffect"), {{"input", parts.value(0)}, {"effect", parts.value(1)}, {"params", it.value()}});
    }
    pendingParams_.clear();
}

QVariantMap EngineClient::inputLevel(const QString& input) const {
    return inputLevels_.value(input, QVariantMap{{"volume", 1.0}, {"muted", false}});
}

void EngineClient::setInputVolume(const QString& input, double volume) {
    inputLevels_[input]["volume"] = volume;
    pendingInputVolumes_[input] = volume;
    inputRevision_++;
    emit inputLevelsChanged();
    if (!volumeThrottle_.isActive()) volumeThrottle_.start();
}

void EngineClient::setInputMuted(const QString& input, bool muted) {
    inputLevels_[input]["muted"] = muted;
    inputRevision_++;
    emit inputLevelsChanged();
    call(QStringLiteral("setInput"), {{"input", input}, {"muted", muted}});
}

void EngineClient::setWave3(const QString& key, const QVariant& value) {
    QVariantMap config = wave3_.value(QStringLiteral("config")).toMap();
    config[key] = value;
    wave3_[QStringLiteral("config")] = config;
    emit wave3Changed();
    wave3Settle_.start();
    call(QStringLiteral("setWave3"), QJsonObject{{key, QJsonValue::fromVariant(value)}});
}

void EngineClient::storeEffects(const QString& input, const QJsonArray& effects) {
    QVariantList list = effects.toVariantList();
    // Echoes of values sent while the user is still dragging would make the slider jump back.
    const QVariantList local = effects_.value(input);
    for (QVariant& v : list) {
        QVariantMap e = v.toMap();
        const qint64 editedAt = paramEditedAt_.value(input + '|' + e["id"].toString(), -1);
        if (editedAt < 0 || clock_.elapsed() - editedAt > 500) continue;
        for (const QVariant& l : local)
            if (l.toMap()["id"] == e["id"]) e["params"] = l.toMap()["params"];
        v = e;
    }
    effects_[input] = list;
    effectRevision_++;
    emit effectsChanged();
}

QVariantList EngineClient::effects(const QString& input) const { return effects_.value(input); }

void EngineClient::addEffect(const QString& input, const QString& type, int index) {
    QJsonObject params{{"input", input}, {"type", type}};
    if (index >= 0) params["index"] = index;
    call(QStringLiteral("addEffect"), params);
}

void EngineClient::editEffect(const QString& input, const QString& effect,
                              const std::function<void(QVariantMap&)>& change) {
    QVariantList& list = effects_[input];
    for (QVariant& v : list) {
        QVariantMap e = v.toMap();
        if (e["id"] != effect) continue;
        change(e);
        v = e;
    }
    effectRevision_++;
    emit effectsChanged();
}

void EngineClient::renameEffect(const QString& input, const QString& effect, const QString& name) {
    editEffect(input, effect, [&](QVariantMap& e) { e["name"] = name.trimmed(); });
    call(QStringLiteral("setEffect"), {{"input", input}, {"effect", effect}, {"name", name.trimmed()}});
}

void EngineClient::removeEffect(const QString& input, const QString& effect) {
    call(QStringLiteral("removeEffect"), {{"input", input}, {"effect", effect}});
}

void EngineClient::setEffectBypass(const QString& input, const QString& effect, bool bypass) {
    editEffect(input, effect, [&](QVariantMap& e) { e["bypass"] = bypass; });
    call(QStringLiteral("setEffect"), {{"input", input}, {"effect", effect}, {"bypass", bypass}});
}

void EngineClient::setEffectParam(const QString& input, const QString& effect, const QString& param, double value) {
    editEffect(input, effect, [&](QVariantMap& e) {
        QVariantMap params = e["params"].toMap();
        params[param] = value;
        e["params"] = params;
    });
    pendingParams_[input + '|' + effect][param] = value;
    paramEditedAt_[input + '|' + effect] = clock_.elapsed();
    if (!volumeThrottle_.isActive()) volumeThrottle_.start();
}

void EngineClient::setManageDefaultSink(bool manage) {
    manageDefaultSink_ = manage;
    emit stateChanged();
    call(QStringLiteral("setSettings"), {{"manageDefaultSink", manage}});
}

void EngineClient::soundCheckAction(const QString& action, const QString& input) {
    QJsonObject params{{"action", action}};
    if (!input.isEmpty()) params["input"] = input;
    call(QStringLiteral("soundCheck"), params);
}

void EngineClient::setSoundCheckOutput(const QString& output) {
    soundCheck_["output"] = output;
    emit soundCheckChanged();
    call(QStringLiteral("soundCheck"), {{"action", "output"}, {"output", output}});
}

void EngineClient::routeApp(const QString& appKey, const QString& input) {
    call(QStringLiteral("routeApp"), {{"app", appKey}, {"input", input}});
}

void EngineClient::setCellMuted(const QString& input, const QString& mix, bool muted) {
    cells_[key(input, mix)]["muted"] = muted;
    cellRevision_++;
    emit cellsChanged();
    call(QStringLiteral("setCell"), {{"input", input}, {"mix", mix}, {"muted", muted}});
}

void EngineClient::setCellActive(const QString& input, const QString& mix, bool active) {
    cells_[key(input, mix)]["active"] = active;
    cellRevision_++;
    emit cellsChanged();
    call(QStringLiteral("setCell"), {{"input", input}, {"mix", mix}, {"active", active}});
}

QPointF EngineClient::inputPeak(const QString& input) const { return inputPeaks_.value(input); }
QPointF EngineClient::mixPeak(const QString& mix) const { return mixPeaks_.value(mix); }
QPointF EngineClient::cellPeak(const QString& input, const QString& mix) const {
    return cellPeaks_.value(key(input, mix));
}

void EngineClient::addSoftwareInput(const QString& name) {
    call(QStringLiteral("addInput"), {{"kind", "software"}, {"name", name}});
}

void EngineClient::addHardwareInput(const QString& device, const QString& name) {
    call(QStringLiteral("addInput"), {{"kind", "hardware"}, {"device", device}, {"name", name}});
}

void EngineClient::addAppInput(const QString& appKey, const QString& name) {
    call(QStringLiteral("addInput"), {{"kind", "software"}, {"name", name}, {"app", appKey}});
}

void EngineClient::removeInput(const QString& input) { call(QStringLiteral("removeInput"), {{"input", input}}); }

void EngineClient::renameInput(const QString& input, const QString& name) {
    call(QStringLiteral("setInput"), {{"input", input}, {"name", name}});
}

void EngineClient::addMix(const QString& name) { call(QStringLiteral("addMix"), {{"name", name}}); }

void EngineClient::removeMix(const QString& mix) { call(QStringLiteral("removeMix"), {{"mix", mix}}); }

void EngineClient::renameMix(const QString& mix, const QString& name) {
    call(QStringLiteral("setMix"), {{"mix", mix}, {"name", name}});
}

void EngineClient::setMixOutputs(const QString& mix, const QStringList& devices) {
    call(QStringLiteral("setMix"), {{"mix", mix}, {"outputs", QJsonArray::fromStringList(devices)}});
}
