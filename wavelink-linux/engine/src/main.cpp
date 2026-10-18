// wavelinkd: the Wave Link for Linux audio engine.
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHostAddress>
#include <QStandardPaths>

#include <pipewire/pipewire.h>

#include <csignal>

#include "Engine.hpp"

int main(int argc, char** argv) {
    pw_init(&argc, &argv);
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("wavelink-linux"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Wave Link for Linux audio engine"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption portOption(QStringLiteral("port"), QStringLiteral("WebSocket API port on 127.0.0.1."),
                                  QStringLiteral("port"), QStringLiteral("1884"));
    QCommandLineOption hostOption(QStringLiteral("host"), QStringLiteral("Address to listen on (default 127.0.0.1)."),
                                  QStringLiteral("address"), QStringLiteral("127.0.0.1"));
    QCommandLineOption configOption(
        QStringLiteral("config"), QStringLiteral("Configuration file."), QStringLiteral("file"),
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/wavelink-linux/config.json");
    parser.addOptions({portOption, hostOption, configOption});
    parser.process(app);

    std::signal(SIGINT, [](int) { QCoreApplication::quit(); });
    std::signal(SIGTERM, [](int) { QCoreApplication::quit(); });

    int rc = 1;
    {
        wl::Engine engine(parser.value(configOption), QHostAddress(parser.value(hostOption)),
                          quint16(parser.value(portOption).toUInt()));
        QString error;
        if (!engine.start(&error)) {
            qCritical("wavelinkd: %s", qPrintable(error));
        } else {
            rc = app.exec();
        }
    }
    pw_deinit();
    return rc;
}
