#include "ApiServer.hpp"

#include <QJsonDocument>
#include <QWebSocket>
#include <QWebSocketServer>

namespace wl {

ApiServer::ApiServer(Handler handler, QObject* parent)
    : QObject(parent),
      handler_(std::move(handler)),
      server_(new QWebSocketServer(QStringLiteral("wavelinkd"), QWebSocketServer::NonSecureMode, this)) {
    connect(server_, &QWebSocketServer::newConnection, this, [this] {
        while (QWebSocket* client = server_->nextPendingConnection()) {
            clients_.insert(client);
            connect(client, &QWebSocket::textMessageReceived, this,
                    [this, client](const QString& text) { onMessage(client, text); });
            connect(client, &QWebSocket::disconnected, this, [this, client] {
                clients_.remove(client);
                meterSubscribers_.remove(client);
                client->deleteLater();
            });
        }
    });
}

ApiServer::~ApiServer() = default;

bool ApiServer::listen(const QHostAddress& host, quint16 port, QString* error) {
    if (server_->listen(host, port)) return true;
    *error = server_->errorString();
    return false;
}

void ApiServer::send(QWebSocket* client, const QString& event, const QJsonValue& data) {
    client->sendTextMessage(
        QString::fromUtf8(QJsonDocument(QJsonObject{{"event", event}, {"data", data}}).toJson(QJsonDocument::Compact)));
}

void ApiServer::broadcast(const QString& event, const QJsonValue& data) {
    const QString text =
        QString::fromUtf8(QJsonDocument(QJsonObject{{"event", event}, {"data", data}}).toJson(QJsonDocument::Compact));
    for (QWebSocket* c : std::as_const(clients_)) c->sendTextMessage(text);
}

void ApiServer::setMeterSubscriber(QWebSocket* client, bool enabled) {
    if (enabled) meterSubscribers_.insert(client);
    else meterSubscribers_.remove(client);
}

void ApiServer::onMessage(QWebSocket* client, const QString& text) {
    const QJsonObject request = QJsonDocument::fromJson(text.toUtf8()).object();
    const QJsonValue id = request.value("id");
    QJsonObject reply{{"id", id}};
    try {
        const QString method = request.value("method").toString();
        if (method.isEmpty()) throw ApiError("missing method");
        reply["result"] = handler_(method, request.value("params").toObject(), client);
    } catch (const std::exception& e) {
        reply["error"] = QJsonObject{{"message", QString::fromUtf8(e.what())}};
    }
    client->sendTextMessage(QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact)));
}

}  // namespace wl
