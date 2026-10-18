// Elgato Wave:3 hardware settings (gain, mute, Clipguard, low-cut, headphones, monitor mix, knob,
// LEDs, gain lock) through libwave3. Lives on its own thread because USB control transfers block;
// polls /config so knob turns and touch-mute on the mic show up in the UI.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <memory>

namespace wave3 {
class Device;
}

namespace wl {

class Wave3Service : public QObject {
    Q_OBJECT
public:
    explicit Wave3Service(QObject* parent = nullptr);
    ~Wave3Service() override;

public slots:
    void start();
    // Keys as in wave3ctl (input_gain, input_mute, clipguard_enable, ...); values are JSON numbers,
    // booleans or enum names.
    void set(const QJsonObject& changes);

signals:
    // {"connected": bool, "serial", "firmware", "api", "config": {key: value}}
    void stateChanged(const QJsonObject& state);

private:
    void poll();
    void open();
    void publish(const QJsonObject& state);

    std::unique_ptr<wave3::Device> device_;
    QTimer* timer_ = nullptr;
    QJsonObject identity_;
    QJsonObject last_;
    int idleTicks_ = 0;
};

}  // namespace wl
