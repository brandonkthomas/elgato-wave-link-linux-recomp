#include "AppSettings.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

AppSettings::AppSettings(QObject* parent)
    : QObject(parent), settings_(QStringLiteral("wavelink-linux"), QStringLiteral("wavelink-linux")) {}

int AppSettings::colorTheme() const { return settings_.value(QStringLiteral("colorTheme"), 0).toInt(); }

void AppSettings::setColorTheme(int theme) {
    if (theme == colorTheme()) return;
    settings_.setValue(QStringLiteral("colorTheme"), theme);
    emit colorThemeChanged();
}

QString AppSettings::autostartFile() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/wavelink-linux.desktop";
}

bool AppSettings::startAtLogin() const {
    QFile f(autostartFile());
    return f.open(QIODevice::ReadOnly) && !f.readAll().contains("Hidden=true");
}

void AppSettings::setStartAtLogin(bool start) {
    if (start == startAtLogin()) return;
    const QString path = autostartFile();
    if (!start) {
        QFile::remove(path);
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const QString icon = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                                 "/wavelink-linux/elgato/Assets/StoreLogo.scale-400.png";
            f.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=Wave Link\nExec=%1 --hidden\nIcon=%2\n"
                                   "Terminal=false\nX-GNOME-Autostart-enabled=true\n")
                        .arg(QCoreApplication::applicationFilePath(), icon)
                        .toUtf8());
        }
    }
    emit startAtLoginChanged();
}

bool AppSettings::keepMinimized() const { return settings_.value(QStringLiteral("keepMinimized"), true).toBool(); }

void AppSettings::setKeepMinimized(bool keep) {
    if (keep == keepMinimized()) return;
    settings_.setValue(QStringLiteral("keepMinimized"), keep);
    emit keepMinimizedChanged();
}
