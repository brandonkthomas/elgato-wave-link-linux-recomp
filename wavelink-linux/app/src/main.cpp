// wavelink: the Wave Link for Linux user interface.
#include <QCommandLineParser>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include "AppSettings.hpp"
#include "Assets.hpp"
#include "EngineClient.hpp"
#include "Tray.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("wavelink-linux"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Wave Link"));
    QGuiApplication::setDesktopFileName(QStringLiteral("wavelink-linux"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption engineOption(QStringLiteral("engine"), QStringLiteral("Engine API URL."), QStringLiteral("url"),
                                    QStringLiteral("ws://127.0.0.1:1884"));
    QCommandLineOption shotOption(QStringLiteral("screenshot"), QStringLiteral("Save the window to a PNG and exit."),
                                  QStringLiteral("file"));
    QCommandLineOption shotDelayOption(QStringLiteral("screenshot-delay"), QStringLiteral("Delay before the screenshot (ms)."),
                                       QStringLiteral("ms"), QStringLiteral("2500"));
    QCommandLineOption pageOption(QStringLiteral("page"), QStringLiteral("Page to open (mixes, device)."), QStringLiteral("page"),
                                  QStringLiteral("mixes"));
    QCommandLineOption hiddenOption(QStringLiteral("hidden"), QStringLiteral("Start in the tray without showing the window."));
    QCommandLineOption tabOption(QStringLiteral("tab"), QStringLiteral("Device page tab to open (0-2)."), QStringLiteral("index"),
                                 QStringLiteral("0"));
    QCommandLineOption channelOption(QStringLiteral("channel"), QStringLiteral("Open the dialog of this channel id."),
                                     QStringLiteral("id"));
    QCommandLineOption effectOption(QStringLiteral("effect"), QStringLiteral("Open the settings of this effect id."),
                                    QStringLiteral("id"));
    QCommandLineOption sizeOption(QStringLiteral("size"), QStringLiteral("Initial window size."), QStringLiteral("WxH"),
                                  QStringLiteral("1600x900"));
    parser.addOptions({engineOption, shotOption, shotDelayOption, pageOption, hiddenOption, tabOption, channelOption,
                       sizeOption, effectOption});
    parser.process(app);

    Assets assets;
    if (assets.available()) app.setWindowIcon(QIcon(assets.image(QStringLiteral("StoreLogo.png")).toLocalFile()));
    EngineClient engine(QUrl(parser.value(engineOption)));

    QQmlApplicationEngine qml;
    qmlRegisterSingletonInstance("WaveLink.Backend", 1, 0, "Engine", &engine);
    qmlRegisterSingletonInstance("WaveLink.Backend", 1, 0, "Assets", &assets);
    AppSettings appSettings;
    qmlRegisterSingletonInstance("WaveLink.Backend", 1, 0, "AppSettings", &appSettings);
    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    const QStringList size = parser.value(sizeOption).split(QLatin1Char('x'));
    qml.setInitialProperties({{QStringLiteral("page"), parser.value(pageOption)},
                              {QStringLiteral("width"), size.value(0).toInt()},
                              {QStringLiteral("height"), size.value(1).toInt()},
                              {QStringLiteral("deviceTab"), parser.value(tabOption).toInt()},
                              {QStringLiteral("openChannel"), parser.value(channelOption)},
                              {QStringLiteral("openEffect"), parser.value(effectOption)},
                              {QStringLiteral("visible"), !parser.isSet(hiddenOption)}});
    qml.loadFromModule("WaveLink", "Main");
    Tray tray(assets);
    tray.setKeepRunning(appSettings.keepMinimized());
    QObject::connect(&appSettings, &AppSettings::keepMinimizedChanged, &tray,
                     [&] { tray.setKeepRunning(appSettings.keepMinimized()); });
    if (auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().value(0))) tray.attach(window);
    if (parser.isSet(shotOption)) {
        const QString file = parser.value(shotOption);
        QTimer::singleShot(parser.value(shotDelayOption).toInt(), &app, [&qml, file] {
            if (auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().value(0)))
                window->grabWindow().save(file);
            QCoreApplication::quit();
        });
    }
    return app.exec();
}
