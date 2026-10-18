// Elgato's fonts, images and UI strings, imported from the user's own Wave Link installer
// (tools/import-elgato-assets.sh) and never bundled with the source.
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>

class Assets : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString textFont READ textFont CONSTANT)
    Q_PROPERTY(QString iconFont READ iconFont CONSTANT)

public:
    explicit Assets(QObject* parent = nullptr);

    bool available() const { return !root_.isEmpty(); }
    QString textFont() const { return textFont_; }
    QString iconFont() const { return iconFont_; }

    // file:// URL of an image under the installer's Assets folder, e.g. "StoreLogo.png".
    Q_INVOKABLE QUrl image(const QString& name) const;
    // Wave Link's English UI text for a resource key; the key itself when unknown.
    Q_INVOKABLE QString text(const QString& key) const;

    QString root() const { return root_; }

private:
    QString root_;
    QString textFont_;
    QString iconFont_;
    QJsonObject strings_;
};
