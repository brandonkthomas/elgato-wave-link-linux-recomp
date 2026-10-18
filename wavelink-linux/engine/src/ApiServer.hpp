// JSON-over-WebSocket control API on localhost.
//   request  {"id": 1, "method": "setCell", "params": {...}}
//   response {"id": 1, "result": ...} or {"id": 1, "error": {"message": "..."}}
//   event    {"event": "state" | "meters", "data": {...}}
#pragma once

#include <QHostAddress>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QSet>
#include <functional>
#include <stdexcept>

class QWebSocket;
class QWebSocketServer;

namespace wl {

class ApiError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ApiServer : public QObject {
    Q_OBJECT
public:
    // Handler runs on the main thread; throw ApiError to reply with an error.
    using Handler = std::function<QJsonValue(const QString& method, const QJsonObject& params, QWebSocket* client)>;

    explicit ApiServer(Handler handler, QObject* parent = nullptr);
    ~ApiServer() override;

    bool listen(const QHostAddress& host, quint16 port, QString* error);
    void broadcast(const QString& event, const QJsonValue& data);
    void send(QWebSocket* client, const QString& event, const QJsonValue& data);

    void setMeterSubscriber(QWebSocket* client, bool enabled);
    const QSet<QWebSocket*>& meterSubscribers() const { return meterSubscribers_; }

private:
    void onMessage(QWebSocket* client, const QString& text);

    Handler handler_;
    QWebSocketServer* server_;
    QSet<QWebSocket*> clients_;
    QSet<QWebSocket*> meterSubscribers_;
};

}  // namespace wl
