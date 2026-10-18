// Notification-area icon (TrayIconManager in Wave Link): left click shows the window; the menu offers
// Show/Hide window and Exit. On KDE Plasma Qt publishes it as a StatusNotifierItem. Without a tray the
// app quits when its window closes.
#pragma once

#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class Assets;
class QQuickWindow;

class Tray : public QObject {
    Q_OBJECT
public:
    explicit Tray(const Assets& assets, QObject* parent = nullptr);
    void attach(QQuickWindow* window);
    // "Keep app minimized": closing the window leaves the app running in the tray.
    void setKeepRunning(bool keep);

private:
    void showWindow();
    void updateToggleText();

    const Assets& assets_;
    QSystemTrayIcon icon_;
    QMenu menu_;
    QAction* toggle_ = nullptr;
    QQuickWindow* window_ = nullptr;
    bool keepRunning_ = true;
};
