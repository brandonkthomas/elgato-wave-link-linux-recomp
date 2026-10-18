#include "Assets.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QStandardPaths>

namespace {

QString findRoot() {
    QStringList candidates;
    if (const QByteArray env = qgetenv("WAVELINK_ELGATO_ASSETS"); !env.isEmpty())
        candidates << QString::fromLocal8Bit(env);
    candidates << QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/wavelink-linux/elgato";
    candidates << QCoreApplication::applicationDirPath() + "/../share/wavelink-linux/elgato";
    for (const QString& c : candidates)
        if (QFile::exists(c + "/fonts/Elgato-Icons.ttf")) return QDir(c).absolutePath();
    return {};
}

QString addFont(const QString& path) {
    const int id = QFontDatabase::addApplicationFont(path);
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty()) {
        qWarning() << "could not load font" << path;
        return {};
    }
    return families.first();
}

}  // namespace

Assets::Assets(QObject* parent) : QObject(parent), root_(findRoot()) {
    if (root_.isEmpty()) {
        qWarning() << "Elgato assets not found; run tools/import-elgato-assets.sh (set WAVELINK_ELGATO_ASSETS to override)";
        return;
    }
    iconFont_ = addFont(root_ + "/fonts/Elgato-Icons.ttf");
    textFont_ = addFont(root_ + "/fonts/UniversNextPro.ttc");
    QFile f(root_ + "/strings/en-US.json");
    if (f.open(QIODevice::ReadOnly)) strings_ = QJsonDocument::fromJson(f.readAll()).object();
}

QUrl Assets::image(const QString& name) const {
    if (root_.isEmpty()) return {};
    const QDir dir(root_ + "/Assets");
    if (dir.exists(name)) return QUrl::fromLocalFile(dir.filePath(name));
    // MSIX packages store scale-qualified variants (StoreLogo.scale-200.png); take the largest.
    const QFileInfo info(name);
    const QStringList variants = dir.entryList({info.completeBaseName() + ".scale-*." + info.suffix()}, QDir::Files);
    QString best;
    int bestScale = -1;
    for (const QString& v : variants) {
        const int scale = v.section(".scale-", 1).section('.', 0, 0).toInt();
        if (scale > bestScale) {
            bestScale = scale;
            best = v;
        }
    }
    return best.isEmpty() ? QUrl() : QUrl::fromLocalFile(dir.filePath(best));
}

QString Assets::text(const QString& key) const {
    const QJsonValue v = strings_.value(key);
    return v.isString() ? v.toString() : key;
}
