// QML-facing client of the wavelinkd WebSocket API. Keeps a local copy of the engine state, applies
// slider moves optimistically and exposes meter peaks at the engine's 30 Hz rate.
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QWebSocket>
#include <functional>

class EngineClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY stateChanged)
    Q_PROPERTY(QVariantList mixes READ mixes NOTIFY stateChanged)
    Q_PROPERTY(QVariantList outputDevices READ outputDevices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList inputDevices READ inputDevices NOTIFY devicesChanged)
    Q_PROPERTY(QVariantList apps READ apps NOTIFY devicesChanged)
    Q_PROPERTY(QVariantMap appRoutes READ appRoutes NOTIFY stateChanged)  // app key -> input id
    // "Lock system devices": Wave Link System stays the system's default output.
    Q_PROPERTY(bool manageDefaultSink READ manageDefaultSink WRITE setManageDefaultSink NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap wave3 READ wave3 NOTIFY wave3Changed)
    // {state: idle|recording|playing, hasRecording, input, output, length, position, bars[26]}
    Q_PROPERTY(QVariantMap soundCheck READ soundCheck NOTIFY soundCheckChanged)
    Q_PROPERTY(QVariantList effectTypes READ effectTypes NOTIFY effectTypesChanged)
    Q_PROPERTY(int effectRevision READ effectRevision NOTIFY effectsChanged)
    Q_PROPERTY(int cellRevision READ cellRevision NOTIFY cellsChanged)
    Q_PROPERTY(int inputRevision READ inputRevision NOTIFY inputLevelsChanged)
    Q_PROPERTY(int meterRevision READ meterRevision NOTIFY metersChanged)

public:
    explicit EngineClient(QUrl url, QObject* parent = nullptr);

    bool connected() const { return connected_; }
    QVariantList inputs() const { return inputs_; }
    QVariantList mixes() const { return mixes_; }
    QVariantList outputDevices() const { return outputDevices_; }
    QVariantList inputDevices() const { return inputDevices_; }
    QVariantList apps() const { return apps_; }
    QVariantMap appRoutes() const { return appRoutes_; }
    bool manageDefaultSink() const { return manageDefaultSink_; }
    void setManageDefaultSink(bool manage);
    QVariantMap wave3() const { return wave3_; }
    QVariantMap soundCheck() const { return soundCheck_; }
    QVariantList effectTypes() const { return effectTypes_; }
    int effectRevision() const { return effectRevision_; }

    int cellRevision() const { return cellRevision_; }
    int inputRevision() const { return inputRevision_; }
    int meterRevision() const { return meterRevision_; }

    // {volume, muted, active} for an input in a mix.
    Q_INVOKABLE QVariantMap cell(const QString& input, const QString& mix) const;
    Q_INVOKABLE void setCellVolume(const QString& input, const QString& mix, double volume);
    Q_INVOKABLE void setCellMuted(const QString& input, const QString& mix, bool muted);
    Q_INVOKABLE void setCellActive(const QString& input, const QString& mix, bool active);

    // A channel's own volume (slider 0..1) and mute.
    Q_INVOKABLE QVariantMap inputLevel(const QString& input) const;
    Q_INVOKABLE void setInputVolume(const QString& input, double volume);
    Q_INVOKABLE void setInputMuted(const QString& input, bool muted);
    // Route an app (its key from `apps`) to a software channel; empty input removes the rule.
    Q_INVOKABLE void routeApp(const QString& appKey, const QString& input);
    // Sound Check: "record" (input), "stop", "play", "reset", or "output" to only pick the output.
    Q_INVOKABLE void soundCheckAction(const QString& action, const QString& input = {});
    Q_INVOKABLE void setSoundCheckOutput(const QString& output);
    // Change a Wave:3 setting (wave3ctl key names); applied optimistically.
    Q_INVOKABLE void setWave3(const QString& key, const QVariant& value);

    // Effect chain of an input: [{id, type, name?, bypass, params: {id: value}}] in processing order.
    Q_INVOKABLE QVariantList effects(const QString& input) const;
    // index < 0 appends.
    Q_INVOKABLE void addEffect(const QString& input, const QString& type, int index = -1);
    Q_INVOKABLE void removeEffect(const QString& input, const QString& effect);
    Q_INVOKABLE void renameEffect(const QString& input, const QString& effect, const QString& name);
    Q_INVOKABLE void setEffectBypass(const QString& input, const QString& effect, bool bypass);
    Q_INVOKABLE void setEffectParam(const QString& input, const QString& effect, const QString& param, double value);

    // Linear peaks [left, right] since the previous meter frame.
    Q_INVOKABLE QPointF inputPeak(const QString& input) const;
    Q_INVOKABLE QPointF mixPeak(const QString& mix) const;
    Q_INVOKABLE QPointF cellPeak(const QString& input, const QString& mix) const;

    Q_INVOKABLE void addSoftwareInput(const QString& name);
    Q_INVOKABLE void addHardwareInput(const QString& device, const QString& name);
    // New software channel named after an app, with the app routed to it.
    Q_INVOKABLE void addAppInput(const QString& appKey, const QString& name);
    Q_INVOKABLE void removeInput(const QString& input);
    Q_INVOKABLE void renameInput(const QString& input, const QString& name);
    Q_INVOKABLE void addMix(const QString& name);
    Q_INVOKABLE void removeMix(const QString& mix);
    Q_INVOKABLE void renameMix(const QString& mix, const QString& name);
    Q_INVOKABLE void setMixOutputs(const QString& mix, const QStringList& devices);

signals:
    void connectedChanged();
    void stateChanged();
    void devicesChanged();
    void cellsChanged();
    void inputLevelsChanged();
    void wave3Changed();
    void soundCheckChanged();
    void effectTypesChanged();
    void effectsChanged();
    void metersChanged();
    void error(const QString& message);

private:
    void call(const QString& method, const QJsonObject& params,
              std::function<void(const QJsonValue&)> onResult = {});
    void storeEffects(const QString& input, const QJsonArray& effects);
    // Applies `change` to one effect of the local copy and notifies (optimistic update).
    void editEffect(const QString& input, const QString& effect, const std::function<void(QVariantMap&)>& change);
    void onMessage(const QString& text);
    void applyState(const QJsonObject& state);
    void applyDevices(const QJsonObject& devices);
    void storeCell(const QJsonObject& cell);
    void flushPendingVolumes();

    QUrl url_;
    QWebSocket socket_;
    QTimer reconnectTimer_;
    QTimer volumeThrottle_;
    bool connected_ = false;
    int nextId_ = 1;

    QVariantList inputs_, mixes_, outputDevices_, inputDevices_, apps_;
    QHash<QString, QVariantMap> inputLevels_;
    QVariantMap wave3_;
    QVariantMap appRoutes_;
    bool manageDefaultSink_ = true;
    QVariantMap soundCheck_;
    QVariantList effectTypes_;
    QHash<QString, QVariantList> effects_;  // input -> chain
    int effectRevision_ = 0;
    QHash<int, std::function<void(const QJsonValue&)>> pending_;
    QHash<QString, QJsonObject> pendingParams_;  // "input|effect" -> params not yet sent
    QHash<QString, qint64> paramEditedAt_;       // "input|effect" -> clock_ time of the last local change
    QElapsedTimer clock_;
    QTimer wave3Settle_;  // ignore device polls briefly after a local change
    QHash<QString, QVariantMap> cells_;  // "input|mix"
    QHash<QString, QPointF> inputPeaks_, mixPeaks_, cellPeaks_;
    QHash<QString, double> pendingVolumes_;       // "input|mix" -> volume not yet sent
    QHash<QString, double> pendingInputVolumes_;  // input -> volume not yet sent
    int cellRevision_ = 0;
    int inputRevision_ = 0;
    int meterRevision_ = 0;
};
