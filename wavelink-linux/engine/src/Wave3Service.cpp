#include "Wave3Service.hpp"

#include <QDebug>

#include <wave3/device.hpp>
#include <wave3/fake_transport.hpp>

namespace wl {

namespace {

constexpr int kPollMs = 200;         // knob/touch changes appear within a fifth of a second
constexpr int kReconnectTicks = 10;  // look for the mic every 2 s while it is absent

QJsonValue toJson(const wave3::Value& v) {
    if (auto b = std::get_if<bool>(&v)) return *b;
    if (auto u = std::get_if<uint64_t>(&v)) return double(*u);
    if (auto d = std::get_if<double>(&v)) return *d;
    return QString::fromStdString(std::get<std::string>(v));
}

std::string toText(const QJsonValue& v) {
    if (v.isBool()) return v.toBool() ? "on" : "off";
    if (v.isDouble()) return QString::number(v.toDouble(), 'g', 10).toStdString();
    return v.toString().toStdString();
}

}  // namespace

Wave3Service::Wave3Service(QObject* parent) : QObject(parent) {}

Wave3Service::~Wave3Service() = default;

void Wave3Service::start() {
    timer_ = new QTimer(this);
    timer_->setInterval(kPollMs);
    connect(timer_, &QTimer::timeout, this, &Wave3Service::poll);
    timer_->start();
    open();
}

void Wave3Service::open() {
    try {
        QString serial, route;
        if (qEnvironmentVariableIsSet("WAVELINK_FAKE_WAVE3")) {  // UI development without hardware
            serial = QStringLiteral("BS0000000000");
            route = QStringLiteral("simulated");
            device_ = std::make_unique<wave3::Device>(std::make_unique<wave3::FakeTransport>());
        } else {
            auto transport = std::make_unique<wave3::UsbTransport>(wave3::OpenOptions{});
            serial = QString::fromStdString(transport->serial());
            route = QString::fromStdString(transport->route());
            device_ = std::make_unique<wave3::Device>(std::move(transport));
        }
        const auto version = device_->read(*device_->schema().message("version"));
        auto num = [&](const char* k) { return QString::number(std::get<uint64_t>(version.at(k))); };
        identity_ = QJsonObject{
            {"serial", serial},
            {"firmware", num("fw/major") + "." + num("fw/minor") + "." + num("fw/patch")},
            {"api", QString("%1.%2").arg(device_->api_major()).arg(device_->api_minor())},
            {"route", route},
        };
        qInfo() << "Wave:3" << serial << "connected via" << route;
        poll();
    } catch (const std::exception& e) {
        device_.reset();
        if (!last_.isEmpty() && last_["connected"].toBool()) qInfo() << "Wave:3 unavailable:" << e.what();
        publish(QJsonObject{{"connected", false}});
    }
}

void Wave3Service::poll() {
    if (!device_) {
        if (++idleTicks_ >= kReconnectTicks) {
            idleTicks_ = 0;
            open();
        }
        return;
    }
    try {
        QJsonObject config;
        for (const auto& [key, value] : device_->read(*device_->schema().message("config")))
            config[QString::fromStdString(key)] = toJson(value);
        QJsonObject state = identity_;
        state["connected"] = true;
        state["config"] = config;
        publish(state);
    } catch (const std::exception& e) {
        qInfo() << "Wave:3 disconnected:" << e.what();
        device_.reset();
        publish(QJsonObject{{"connected", false}});
    }
}

void Wave3Service::set(const QJsonObject& changes) {
    if (!device_) return;
    std::vector<std::pair<std::string, std::string>> assignments;
    for (auto it = changes.begin(); it != changes.end(); ++it)
        assignments.emplace_back(it.key().toStdString(), toText(it.value()));
    try {
        device_->set(assignments);
    } catch (const std::exception& e) {
        qWarning() << "Wave:3 setting failed:" << e.what();
    }
    poll();
}

void Wave3Service::publish(const QJsonObject& state) {
    if (state == last_) return;
    last_ = state;
    emit stateChanged(state);
}

}  // namespace wl
