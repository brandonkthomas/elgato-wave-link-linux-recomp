#include "Engine.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <set>

namespace wl {

namespace {

QString req(const QJsonObject& params, const char* key) {
    const QString v = params.value(key).toString();
    if (v.isEmpty()) throw ApiError(std::string("missing parameter: ") + key);
    return v;
}

bool isOurs(const PwNode& n) {
    return n.name.rfind("wavelink.", 0) == 0;
}

QJsonArray peakJson(const Peak& p) {
    return {double(p.left), double(p.right)};
}

}  // namespace

Engine::Engine(QString configPath, QHostAddress host, quint16 port, QObject* parent)
    : QObject(parent),
      configPath_(std::move(configPath)),
      host_(std::move(host)),
      port_(port),
      graph_(mixer_),
      api_([this](const QString& m, const QJsonObject& p, QWebSocket* c) { return handle(m, p, c); }) {
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(500);
    connect(&saveTimer_, &QTimer::timeout, this, &Engine::save);
    meterTimer_.setInterval(33);  // ~30 Hz
    connect(&meterTimer_, &QTimer::timeout, this, &Engine::publishMeters);
    connect(&meterTimer_, &QTimer::timeout, this, &Engine::purgeRetiredChains);
    clock_.start();
    pluginDir_ = qEnvironmentVariableIsSet("WAVELINK_PLUGIN_DIR")
                     ? qEnvironmentVariable("WAVELINK_PLUGIN_DIR").toStdString()
                     : (QCoreApplication::applicationDirPath() + "/../lib/wavelink/plugins").toStdString();
    devicesTimer_.setSingleShot(true);
    devicesTimer_.setInterval(100);
    connect(&devicesTimer_, &QTimer::timeout, this, [this] {
        autoAddWave3();
        firstRun_ = false;  // the default configuration covers the devices present at startup only
        ensureMonitorOutput();
        applyAppRoutes();
        api_.broadcast(QStringLiteral("devices"), devices());
    });
    connect(&graph_, &PwGraph::nodeVolumeChanged, this, [this](const QString& node, float slider, bool mute) {
        for (Input& in : model_.inputs) {
            if (in.kind != InputKind::Software || node != kSoftwareSlotDefs[in.slot].nodeName) continue;
            in.masterVolume = slider;
            in.masterMuted = mute;
            saveTimer_.start();
            api_.broadcast(QStringLiteral("input"),
                           QJsonObject{{"id", in.id}, {"volume", slider}, {"muted", mute}});
        }
    });
    connect(&graph_, &PwGraph::nodesChanged, this, [this] { devicesTimer_.start(); });
    soundCheckTimer_.setInterval(100);
    connect(&soundCheckTimer_, &QTimer::timeout, this, &Engine::soundCheckTick);
}

QString Engine::soundCheckTarget() const {
    if (!model_.soundCheckOutput.isEmpty()) return model_.soundCheckOutput;
    for (const PwNode& n : graph_.nodes())
        if (n.mediaClass == "Audio/Sink" && n.name.find("Elgato_Wave_3") != std::string::npos)
            return QString::fromStdString(n.name);
    return {};
}

QJsonObject Engine::soundCheckState() const {
    // Wave Link draws 26 bars over the 10 s maximum, each the peak of its stretch on the -60..0 dB scale.
    constexpr int kBars = 26;
    const size_t frames = soundCheckHasRecording_ || soundCheck_ == "recording" ? mixer_.recordedFrames() : 0;
    const size_t capacity = std::max<size_t>(1, mixer_.recordCapacity());
    const float* data = mixer_.recordData();
    QJsonArray bars;
    for (int b = 0; b < kBars; b++) {
        const size_t from = capacity * b / kBars, to = std::min(frames, capacity * (b + 1) / kBars);
        float peak = 0.0f;
        for (size_t i = from; i < to; i++) peak = std::max(peak, std::fabs(data[i]));
        bars.append(peak <= 1e-6f ? 0.0 : std::clamp((20.0 * std::log10(peak) + 60.0) / 60.0, 0.0, 1.0));
    }
    const double duration = double(frames) / double(capacity);  // share of the 26 bars recorded
    return {{"state", soundCheck_},
            {"hasRecording", soundCheckHasRecording_},
            {"input", soundCheckInput_},
            {"output", soundCheckTarget()},
            {"length", duration},
            {"position", soundCheck_ == "playing" ? graph_.sampleProgress() * duration : 0.0},
            {"bars", bars}};
}

void Engine::setSoundCheck(const QString& state) {
    soundCheck_ = state;
    if (state == "idle") soundCheckTimer_.stop();
    else soundCheckTimer_.start();
    api_.broadcast(QStringLiteral("soundCheck"), soundCheckState());
}

void Engine::soundCheckTick() {
    if (soundCheck_ == "recording" && !mixer_.recording()) {  // reached the 10 s limit
        soundCheckHasRecording_ = mixer_.recordedFrames() > 0;
        setSoundCheck(QStringLiteral("idle"));
        return;
    }
    if (soundCheck_ == "playing" && !graph_.samplePlaying()) {
        graph_.stopSample();
        setSoundCheck(QStringLiteral("idle"));
        return;
    }
    api_.broadcast(QStringLiteral("soundCheck"), soundCheckState());
}

Engine::~Engine() {
    wave3Thread_.quit();
    wave3Thread_.wait();
}

bool Engine::start(QString* error) {
    load();
    wave3_ = new Wave3Service;
    wave3_->moveToThread(&wave3Thread_);
    connect(&wave3Thread_, &QThread::finished, wave3_, &QObject::deleteLater);
    connect(&wave3Thread_, &QThread::started, wave3_, &Wave3Service::start);
    connect(wave3_, &Wave3Service::stateChanged, this, [this](const QJsonObject& state) {
        wave3State_ = state;
        api_.broadcast(QStringLiteral("wave3"), state);
    });
    wave3Thread_.start();
    if (!graph_.start(error)) return false;
    if (!api_.listen(host_, port_, error)) return false;
    apply();
    pushMasterVolumes();
    meterTimer_.start();
    return true;
}

void Engine::load() {
    QFile f(configPath_);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            model_ = Model::fromJson(doc.object());
            return;
        }
        qWarning() << "ignoring unreadable configuration" << configPath_;
    }
    model_ = Model::defaults();
    firstRun_ = true;
    saveTimer_.start();
}

void Engine::save() {
    QDir().mkpath(QFileInfo(configPath_).absolutePath());
    QSaveFile f(configPath_);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(model_.toJson()).toJson());
        if (!f.commit()) qWarning() << "cannot save configuration" << configPath_;
    }
}

void Engine::applyGains() {
    for (int i = 0; i < kInputSlots; i++)
        for (int m = 0; m < kMixSlots; m++) mixer_.setTargetGain(i, m, model_.cellGain(i, m));
}

void Engine::rebuildGraph() {
    const std::string engine = PwGraph::engineNodeName();
    std::vector<std::string> watched;
    for (const Input& in : model_.inputs)
        if (in.kind == InputKind::Software) watched.push_back(kSoftwareSlotDefs[in.slot].nodeName);
    graph_.setVolumeWatch(watched);
    std::vector<VirtualDevice> devices;
    std::vector<LinkSpec> links;
    for (const Input& in : model_.inputs) {
        std::string source;
        if (in.kind == InputKind::Software) {
            source = kSoftwareSlotDefs[in.slot].nodeName;
            devices.push_back({source, "Wave Link " + in.name.toStdString(), false});
        } else {
            source = in.deviceNodeName.toStdString();
        }
        for (int c = 0; c < kChannels; c++)
            links.push_back({source, "", c ? "FR" : "FL", engine, PwGraph::inputPortName(in.slot, c), ""});
    }
    for (const Mix& mix : model_.mixes) {
        const std::string node = kMixSlotDefs[mix.slot].nodeName;
        devices.push_back({node, "Wave Link " + mix.name.toStdString(), true});
        for (int c = 0; c < kChannels; c++) {
            const char* ch = c ? "FR" : "FL";
            links.push_back({engine, PwGraph::outputPortName(mix.slot, c), "", node, "", ch});
            for (const QString& out : mix.outputDevices)
                links.push_back({engine, PwGraph::outputPortName(mix.slot, c), "", out.toStdString(), "", ch});
        }
    }
    graph_.setVirtualDevices(devices);
    graph_.setLinks(std::move(links));
    const bool hasSystem = std::any_of(model_.inputs.begin(), model_.inputs.end(),
                                       [](const Input& i) { return i.kind == InputKind::Software && i.slot == 0; });
    graph_.setDefaultSink(model_.manageDefaultSink && hasSystem ? kSoftwareSlotDefs[0].nodeName : "");
}

void Engine::pushMasterVolumes() {
    for (const Input& in : model_.inputs)
        if (in.kind == InputKind::Software)
            graph_.setNodeVolume(kSoftwareSlotDefs[in.slot].nodeName, in.masterVolume, in.masterMuted);
}

static QString appKey(const AppStream& s) {
    return QString::fromStdString(!s.binary.empty() ? s.binary : s.appName);
}

// Like Wave Link's quick-add, the Wave:3 becomes a mixer input the first time its capture device appears.
void Engine::autoAddWave3() {
    // AudioMixer.UpdateWaveMicPipelineOrWaveHardwareInputDevices: when a Wave mic's capture device
    // appears (at startup or when plugged in) and no channel uses it, it becomes a channel in no mix.
    // A channel the user removes stays removed until the device appears again.
    std::set<std::string> present;
    bool added = false;
    for (const PwNode& n : graph_.nodes()) {
        if (n.mediaClass != "Audio/Source" || n.name.find("Elgato_Wave_3") == std::string::npos) continue;
        present.insert(n.name);
        const QString device = QString::fromStdString(n.name);
        if (wave3Seen_.count(n.name) || model_.input("hw:" + device)) continue;
        const int slot = model_.freeHardwareSlot();
        if (slot < 0) continue;
        Input in;
        in.kind = InputKind::Hardware;
        in.slot = slot;
        in.id = "hw:" + device;
        in.name = QStringLiteral("Elgato Wave:3");
        in.deviceNodeName = device;
        for (int m = 0; m < kMixSlots; m++) model_.cells[slot][m] = Cell{};
        // First run only (InitializeDefaultConfiguration): the Stream Mix takes every input and the Chat
        // Mix the Wave mics. A mic never joins the Personal Mix by itself.
        if (firstRun_)
            for (const Mix& mix : model_.mixes)
                if (mix.name == "Stream Mix" || mix.name == "Chat Mix") model_.cells[slot][mix.slot].active = true;
        model_.inputs.insert(model_.inputs.begin(), in);
        added = true;
    }
    wave3Seen_ = std::move(present);
    if (added) changed();
}

void Engine::ensureMonitorOutput() {
    // First run: the Personal Mix (what you hear, mix slot 0) plays where system audio went before the
    // engine made its own sink the default; otherwise taking over the default would go silent.
    if (model_.monitorOutputChosen) return;
    auto personal = std::find_if(model_.mixes.begin(), model_.mixes.end(), [](const Mix& m) { return m.slot == 0; });
    if (personal == model_.mixes.end()) return;
    if (!personal->outputDevices.isEmpty()) {
        model_.monitorOutputChosen = true;
        saveTimer_.start();
        return;
    }
    const std::string sink = graph_.userDefaultSink();
    const auto nodes = graph_.nodes();
    if (sink.empty() || std::none_of(nodes.begin(), nodes.end(), [&](const PwNode& n) {
            return n.name == sink && n.mediaClass == "Audio/Sink";
        }))
        return;
    qInfo() << "Personal Mix plays on" << QString::fromStdString(sink);
    personal->outputDevices = {QString::fromStdString(sink)};
    model_.monitorOutputChosen = true;
    changed();
}

void Engine::applyAppRoutes() {
    std::map<uint32_t, QString> seen;
    for (const AppStream& s : graph_.appStreams()) {
        const QString inputId = model_.appRoutes.value(appKey(s));
        const Input* in = inputId.isEmpty() ? nullptr : model_.input(inputId);
        if (!in || in->kind != InputKind::Software) continue;
        const QString node = kSoftwareSlotDefs[in->slot].nodeName;
        seen[s.id] = node;
        auto sent = routedStreams_.find(s.id);
        if (QString::fromStdString(s.target) != node && (sent == routedStreams_.end() || sent->second != node))
            graph_.setStreamTarget(s.id, node.toStdString());
    }
    routedStreams_ = std::move(seen);
}

void Engine::apply() {
    applyGains();
    rebuildGraph();
    rebuildChains();
}

void Engine::rebuildChains() {
    std::set<QString> live;
    for (int slot = 0; slot < kInputSlots; slot++) {
        auto in = std::find_if(model_.inputs.begin(), model_.inputs.end(), [&](const Input& i) { return i.slot == slot; });
        std::shared_ptr<Chain> next;
        if (in != model_.inputs.end() && !in->effects.isEmpty()) {
            next = std::make_shared<Chain>();
            for (const EffectConfig& cfg : in->effects) {
                live.insert(cfg.id);
                auto& fx = effects_[cfg.id];
                if (!fx) {
                    std::string error;
                    fx = createEffect(cfg.type.toStdString(), pluginDir_, mixer_.sampleRate(), MixerCore::kMaxFrames, &error);
                    if (!fx) {
                        qWarning() << "effect" << cfg.type << "unavailable:" << QString::fromStdString(error);
                        effects_.erase(cfg.id);
                        continue;
                    }
                    const auto& params = fx->params();
                    for (int p = 0; p < int(params.size()); p++)
                        fx->setParam(p, float(cfg.params.value(QString::fromStdString(params[p].id), params[p].def)));
                }
                auto& flag = bypass_[cfg.id];
                if (!flag) flag = std::make_shared<std::atomic<bool>>(false);
                flag->store(cfg.bypass);
                next->entries.push_back({fx, flag});
            }
        }
        if (chains_[slot] == next) continue;
        qInfo() << "input slot" << slot << "effects:" << (next ? int(next->entries.size()) : 0);
        mixer_.setChain(slot, next.get());
        if (chains_[slot]) retired_.emplace_back(chains_[slot], clock_.elapsed());
        chains_[slot] = next;
    }
    for (auto it = effects_.begin(); it != effects_.end();)  // retired chains keep them alive meanwhile
        it = live.count(it->first) ? std::next(it) : effects_.erase(it);
    for (auto it = bypass_.begin(); it != bypass_.end();)
        it = live.count(it->first) ? std::next(it) : bypass_.erase(it);
}

void Engine::purgeRetiredChains() {
    // A data-thread cycle is far shorter than a second; after that nobody can still hold the pointer.
    const qint64 now = clock_.elapsed();
    retired_.erase(std::remove_if(retired_.begin(), retired_.end(), [&](const auto& r) { return now - r.second > 1000; }),
                   retired_.end());
}

QJsonArray Engine::effectTypes() const {
    QJsonArray out;
    for (const EffectType& t : availableEffects(pluginDir_)) {
        QJsonArray params;
        for (const ParamInfo& p : t.params)
            params.append(QJsonObject{{"id", QString::fromStdString(p.id)}, {"name", QString::fromStdString(p.name)},
                                      {"min", p.min}, {"max", p.max}, {"default", p.def},
                                      {"unit", QString::fromStdString(p.unit)}});
        out.append(QJsonObject{{"type", QString::fromStdString(t.type)}, {"name", QString::fromStdString(t.name)},
                               {"params", params}});
    }
    return out;
}

void Engine::changed() {
    apply();
    saveTimer_.start();
    api_.broadcast(QStringLiteral("state"), state());
}

QJsonObject Engine::devices() const {
    QJsonArray outputs, inputs, apps;
    for (const PwNode& n : graph_.nodes()) {
        if (isOurs(n)) continue;
        QJsonObject o{{"name", QString::fromStdString(n.name)},
                      {"description", QString::fromStdString(n.description.empty() ? n.name : n.description)}};
        if (n.mediaClass == "Audio/Sink") outputs.append(o);
        else if (n.mediaClass == "Audio/Source" || n.mediaClass == "Audio/Source/Virtual") inputs.append(o);
    }
    for (const AppStream& s : graph_.appStreams()) {
        QString route;
        for (const Input& in : model_.inputs)
            if (in.kind == InputKind::Software && s.target == kSoftwareSlotDefs[in.slot].nodeName) route = in.id;
        apps.append(QJsonObject{{"id", int(s.id)}, {"app", QString::fromStdString(s.appName)},
                                {"binary", QString::fromStdString(s.binary)}, {"key", appKey(s)},
                                {"target", QString::fromStdString(s.target)}, {"input", route},
                                {"rule", model_.appRoutes.value(appKey(s))}});
    }
    return {{"outputs", outputs}, {"inputs", inputs}, {"apps", apps}};
}

QJsonObject Engine::state() const {
    return {{"model", model_.toJson()}, {"devices", devices()}, {"wave3", wave3State_},
            {"soundCheck", soundCheckState()}};
}

QJsonValue Engine::handle(const QString& method, const QJsonObject& p, QWebSocket* client) {
    if (method == "getState") return state();

    if (method == "subscribeMeters") {
        api_.setMeterSubscriber(client, p.value("enabled").toBool(true));
        return true;
    }

    if (method == "getEffectTypes") return effectTypes();

    if (method == "addEffect") {
        Input* in = model_.input(req(p, "input"));
        if (!in) throw ApiError("unknown input");
        EffectConfig fx{QUuid::createUuid().toString(QUuid::WithoutBraces), req(p, "type"), false, {}, {}};
        std::string error;
        auto probe = createEffect(fx.type.toStdString(), pluginDir_, mixer_.sampleRate(), MixerCore::kMaxFrames, &error);
        if (!probe) throw ApiError(error);
        for (const ParamInfo& info : probe->params()) fx.params[QString::fromStdString(info.id)] = info.def;
        effects_[fx.id] = probe;
        // Optional insert position (Voice Focus goes first, as Wave Link runs it before user effects).
        const int index = p.contains("index") ? std::clamp(p.value("index").toInt(), 0, int(in->effects.size()))
                                              : int(in->effects.size());
        in->effects.insert(index, fx);
        changed();
        return QJsonObject{{"id", fx.id}};
    }

    if (method == "removeEffect" || method == "setEffect" || method == "moveEffect") {
        Input* in = model_.input(req(p, "input"));
        if (!in) throw ApiError("unknown input");
        const QString id = req(p, "effect");
        auto it = std::find_if(in->effects.begin(), in->effects.end(), [&](const EffectConfig& e) { return e.id == id; });
        if (it == in->effects.end()) throw ApiError("unknown effect");
        if (method == "removeEffect") {
            in->effects.erase(it);
            changed();
            return true;
        }
        if (method == "moveEffect") {
            const int to = std::clamp(p.value("index").toInt(), 0, int(in->effects.size()) - 1);
            in->effects.move(int(it - in->effects.begin()), to);
            changed();
            return true;
        }
        if (p.contains("name")) it->name = p.value("name").toString().trimmed();
        if (p.contains("bypass")) {
            it->bypass = p.value("bypass").toBool();
            if (auto flag = bypass_.find(id); flag != bypass_.end()) flag->second->store(it->bypass);
        }
        const QJsonObject params = p.value("params").toObject();
        if (auto fx = effects_.find(id); fx != effects_.end()) {
            const auto& infos = fx->second->params();
            for (int i = 0; i < int(infos.size()); i++) {
                const QString key = QString::fromStdString(infos[i].id);
                if (!params.contains(key)) continue;
                fx->second->setParam(i, float(params.value(key).toDouble()));
                it->params[key] = fx->second->param(i);
            }
        }
        saveTimer_.start();
        api_.broadcast(QStringLiteral("effect"), QJsonObject{{"input", in->id}, {"effects", effectsToJson(in->effects)}});
        return true;
    }

    if (method == "soundCheck") {
        const QString action = req(p, "action");
        if (p.contains("output")) {
            model_.soundCheckOutput = p.value("output").toString();
            saveTimer_.start();
        }
        if (action == "record") {
            const Input* in = model_.input(req(p, "input"));
            if (!in) throw ApiError("unknown input");
            graph_.stopSample();
            soundCheckInput_ = in->id;
            soundCheckHasRecording_ = false;
            mixer_.startRecording(in->slot);
            setSoundCheck(QStringLiteral("recording"));
        } else if (action == "stop") {  // ends recording or playback
            if (soundCheck_ == "recording") {
                mixer_.stopRecording();
                soundCheckHasRecording_ = mixer_.recordedFrames() > 0;
            }
            graph_.stopSample();
            setSoundCheck(QStringLiteral("idle"));
        } else if (action == "play") {
            if (!soundCheckHasRecording_ || soundCheck_ == "recording") throw ApiError("nothing recorded");
            const float* data = mixer_.recordData();
            graph_.playSample(std::vector<float>(data, data + mixer_.recordedFrames()), mixer_.recordRate(),
                              soundCheckTarget().toStdString());
            setSoundCheck(QStringLiteral("playing"));
        } else if (action == "reset") {
            mixer_.stopRecording();
            graph_.stopSample();
            soundCheckHasRecording_ = false;
            setSoundCheck(QStringLiteral("idle"));
        } else if (action != "output") {
            throw ApiError("unknown action");
        } else {
            api_.broadcast(QStringLiteral("soundCheck"), soundCheckState());
        }
        return true;
    }

    if (method == "setWave3") {
        if (!wave3State_["connected"].toBool()) throw ApiError("no Wave:3 connected");
        QMetaObject::invokeMethod(wave3_, "set", Qt::QueuedConnection, Q_ARG(QJsonObject, p));
        return true;
    }

    if (method == "setCell") {
        const Input* in = model_.input(req(p, "input"));
        const Mix* mix = model_.mix(req(p, "mix"));
        if (!in || !mix) throw ApiError("unknown input or mix");
        Cell& c = model_.cells[in->slot][mix->slot];
        if (p.contains("volume")) c.volume = std::clamp(float(p.value("volume").toDouble()), 0.0f, 1.0f);
        if (p.contains("muted")) c.muted = p.value("muted").toBool();
        if (p.contains("active")) c.active = p.value("active").toBool();
        // Volume moves are frequent: update gains immediately, persist and notify without
        // touching the graph.
        applyGains();
        saveTimer_.start();
        api_.broadcast(QStringLiteral("cell"),
                       QJsonObject{{"input", in->id}, {"mix", mix->id}, {"volume", c.volume},
                                   {"muted", c.muted}, {"active", c.active}});
        return true;
    }

    if (method == "addInput") {
        Input in;
        in.name = p.value("name").toString();
        if (p.value("kind").toString() == "hardware") {
            in.kind = InputKind::Hardware;
            in.deviceNodeName = req(p, "device");
            in.slot = model_.freeHardwareSlot();
            in.id = "hw:" + in.deviceNodeName;
            if (model_.input(in.id)) throw ApiError("device is already an input");
        } else {
            in.slot = model_.freeSoftwareSlot();
            if (in.slot >= 0) in.id = kSoftwareSlotDefs[in.slot].id;
        }
        if (in.slot < 0) throw ApiError("no free input slot");
        if (in.name.isEmpty()) in.name = in.kind == InputKind::Software ? kSoftwareSlotDefs[in.slot].name : in.deviceNodeName;
        for (int m = 0; m < kMixSlots; m++) model_.cells[in.slot][m] = Cell{};
        model_.inputs.push_back(in);
        if (in.kind == InputKind::Software && p.contains("app")) model_.appRoutes[p.value("app").toString()] = in.id;
        changed();
        if (in.kind == InputKind::Software) {
            graph_.setNodeVolume(kSoftwareSlotDefs[in.slot].nodeName, in.masterVolume, in.masterMuted);
            routedStreams_.clear();
            applyAppRoutes();
        }
        return QJsonObject{{"id", in.id}};
    }

    if (method == "removeInput") {
        const QString id = req(p, "input");
        auto it = std::find_if(model_.inputs.begin(), model_.inputs.end(), [&](const Input& i) { return i.id == id; });
        if (it == model_.inputs.end()) throw ApiError("unknown input");
        model_.inputs.erase(it);
        for (auto r = model_.appRoutes.begin(); r != model_.appRoutes.end();)
            r = r.value() == id ? model_.appRoutes.erase(r) : std::next(r);
        changed();
        return true;
    }

    if (method == "setInput") {
        Input* in = model_.input(req(p, "input"));
        if (!in) throw ApiError("unknown input");
        if (p.contains("volume") || p.contains("muted")) {
            if (p.contains("volume")) in->masterVolume = std::clamp(float(p.value("volume").toDouble()), 0.0f, 1.0f);
            if (p.contains("muted")) in->masterMuted = p.value("muted").toBool();
            if (in->kind == InputKind::Software)
                graph_.setNodeVolume(kSoftwareSlotDefs[in->slot].nodeName, in->masterVolume, in->masterMuted);
            else
                applyGains();
            saveTimer_.start();
            api_.broadcast(QStringLiteral("input"),
                           QJsonObject{{"id", in->id}, {"volume", in->masterVolume}, {"muted", in->masterMuted}});
            if (!p.contains("name") && !p.contains("hidden")) return true;
        }
        if (p.contains("name")) in->name = p.value("name").toString();
        if (p.contains("hidden")) in->hidden = p.value("hidden").toBool();
        changed();
        return true;
    }

    if (method == "routeApp") {
        const QString key = req(p, "app");
        const QString inputId = p.value("input").toString();
        if (inputId.isEmpty()) {
            model_.appRoutes.remove(key);
            for (const AppStream& s : graph_.appStreams())
                if (appKey(s) == key) graph_.setStreamTarget(s.id, "");
        } else {
            const Input* in = model_.input(inputId);
            if (!in || in->kind != InputKind::Software) throw ApiError("apps can only be routed to software channels");
            model_.appRoutes[key] = inputId;
        }
        routedStreams_.clear();
        applyAppRoutes();
        saveTimer_.start();
        api_.broadcast(QStringLiteral("state"), state());
        return true;
    }

    if (method == "addMix") {
        const int slot = model_.freeMixSlot();
        if (slot < 0) throw ApiError("no free mix slot");
        Mix mix{kMixSlotDefs[slot].id, slot, p.value("name").toString(), {}};
        if (mix.name.isEmpty()) mix.name = kMixSlotDefs[slot].name;
        for (int i = 0; i < kInputSlots; i++) model_.cells[i][slot] = Cell{};
        model_.mixes.push_back(mix);
        changed();
        return QJsonObject{{"id", mix.id}};
    }

    if (method == "removeMix") {
        const QString id = req(p, "mix");
        auto it = std::find_if(model_.mixes.begin(), model_.mixes.end(), [&](const Mix& m) { return m.id == id; });
        if (it == model_.mixes.end()) throw ApiError("unknown mix");
        model_.mixes.erase(it);
        changed();
        return true;
    }

    if (method == "setMix") {
        Mix* mix = model_.mix(req(p, "mix"));
        if (!mix) throw ApiError("unknown mix");
        if (p.contains("name")) mix->name = p.value("name").toString();
        if (p.contains("outputs")) {
            mix->outputDevices.clear();
            for (const auto& v : p.value("outputs").toArray())
                if (!v.toString().startsWith("wavelink.")) mix->outputDevices.append(v.toString());
        }
        changed();
        return true;
    }

    if (method == "setSettings") {
        if (p.contains("manageDefaultSink")) model_.manageDefaultSink = p.value("manageDefaultSink").toBool();
        changed();
        return true;
    }

    throw ApiError("unknown method: " + method.toStdString());
}

void Engine::publishMeters() {
    if (api_.meterSubscribers().isEmpty()) return;
    QJsonObject inputs, mixes, cells;
    for (const Input& in : model_.inputs) {
        inputs[in.id] = peakJson(mixer_.takeInputPeak(in.slot));
        for (const Mix& mix : model_.mixes) cells[in.id + "|" + mix.id] = peakJson(mixer_.takeCellPeak(in.slot, mix.slot));
    }
    for (const Mix& mix : model_.mixes) mixes[mix.id] = peakJson(mixer_.takeMixPeak(mix.slot));
    const QJsonObject data{{"inputs", inputs}, {"mixes", mixes}, {"cells", cells}};
    for (QWebSocket* client : api_.meterSubscribers()) api_.send(client, QStringLiteral("meters"), data);
}

}  // namespace wl
