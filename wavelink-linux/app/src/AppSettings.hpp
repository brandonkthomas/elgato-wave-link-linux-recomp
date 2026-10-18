// Wave Link's General settings that live in the app: color theme, "Start app at login" (an XDG
// autostart entry, which Plasma honours) and "Keep app minimized" (closing the window keeps the app in
// the tray). Stored with QSettings in ~/.config/wavelink-linux/.
#pragma once

#include <QObject>
#include <QSettings>

class AppSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(int colorTheme READ colorTheme WRITE setColorTheme NOTIFY colorThemeChanged)  // 0 system, 1 light, 2 dark
    Q_PROPERTY(bool startAtLogin READ startAtLogin WRITE setStartAtLogin NOTIFY startAtLoginChanged)
    Q_PROPERTY(bool keepMinimized READ keepMinimized WRITE setKeepMinimized NOTIFY keepMinimizedChanged)

public:
    explicit AppSettings(QObject* parent = nullptr);

    int colorTheme() const;
    void setColorTheme(int theme);
    bool startAtLogin() const;
    void setStartAtLogin(bool start);
    bool keepMinimized() const;
    void setKeepMinimized(bool keep);

signals:
    void colorThemeChanged();
    void startAtLoginChanged();
    void keepMinimizedChanged();

private:
    static QString autostartFile();
    QSettings settings_;
};
