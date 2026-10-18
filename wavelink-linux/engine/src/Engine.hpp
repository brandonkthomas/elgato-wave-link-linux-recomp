// Ties the model, the PipeWire graph, the real-time mixer and the API together.
#pragma once

#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include <map>
#include <set>

#include "ApiServer.hpp"
#include "MixerCore.hpp"
#include "Model.hpp"
#include "PwGraph.hpp"
#include "Wave3Service.hpp"
#include "effects/Registry.hpp"

#include <QElapsedTimer>
#include <memory>

#include <QThread>

namespace wl {

class Engine : public QObject {
    Q_OBJECT
public:
    Engine(QString configPath, QHostAddress host, quint16 port, QObject* parent = nullptr);
    ~Engine() override;
    bool start(QString* error);

private:
    QJsonValue handle(const QString& method, const QJsonObject& params, QWebSocket* client);
    QJsonObject state() const;
    QJsonObject devices() const;

    void apply();  // push the model into the mixer and the graph, persist, notify
    void applyGains();
    void rebuildGraph();
    void changed();

    void applyAppRoutes();
    void autoAddWave3();
    void ensureMonitorOutput();
    void rebuildChains();
    void purgeRetiredChains();
    QJsonArray effectTypes() const;
    void pushMasterVolumes();

    void load();
    void save();
    void publishMeters();

    // Sound Check (Wave Link's SampleRecorder): record an input after its effects, play it back.
    QJsonObject soundCheckState() const;
    QString soundCheckTarget() const;  // the configured output, else the Wave:3 headphones, else ""
    void soundCheckTick();
    void setSoundCheck(const QString& state);

    QString configPath_;
    bool firstRun_ = false;  // no configuration existed: Wave Link's InitializeDefaultConfiguration applies
    std::set<std::string> wave3Seen_;  // Wave:3 capture nodes already handled while connected
    QHostAddress host_;
    quint16 port_;
    Model model_;
    MixerCore mixer_;
    PwGraph graph_;
    ApiServer api_;
    QTimer saveTimer_;
    QTimer meterTimer_;
    QTimer devicesTimer_;
    std::map<uint32_t, QString> routedStreams_;  // stream id -> channel node we sent it to
    QThread wave3Thread_;
    Wave3Service* wave3_ = nullptr;  // lives on wave3Thread_
    QJsonObject wave3State_{{"connected", false}};
    QTimer soundCheckTimer_;
    QString soundCheck_ = QStringLiteral("idle");  // idle | recording | playing
    QString soundCheckInput_;
    bool soundCheckHasRecording_ = false;

    // Effects: one instance per configured effect id, shared by successive chain snapshots.
    std::string pluginDir_;
    std::map<QString, std::shared_ptr<Effect>> effects_;
    std::map<QString, std::shared_ptr<std::atomic<bool>>> bypass_;
    std::array<std::shared_ptr<Chain>, kInputSlots> chains_;
    std::vector<std::pair<std::shared_ptr<Chain>, qint64>> retired_;  // kept until the data thread moved on
    QElapsedTimer clock_;
};

}  // namespace wl
