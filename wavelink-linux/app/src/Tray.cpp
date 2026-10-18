#include "Tray.hpp"

#include <QApplication>
#include <QQuickWindow>

#include "Assets.hpp"

Tray::Tray(const Assets& assets, QObject* parent) : QObject(parent), assets_(assets) {
    icon_.setIcon(QIcon(assets.image(QStringLiteral("TrayIcon.ico")).toLocalFile()));
    icon_.setToolTip(QStringLiteral("Wave Link"));
    toggle_ = menu_.addAction(QString());
    menu_.addSeparator();
    QAction* exit = menu_.addAction(assets.text(QStringLiteral("Elgato.BaseClasses.WinUI/Resources/SystemTrayIcon_ExitButton")));
    connect(exit, &QAction::triggered, qApp, &QCoreApplication::quit);
    connect(toggle_, &QAction::triggered, this, [this] {
        if (window_ && window_->isVisible()) window_->hide();
        else showWindow();
    });
    connect(&icon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) showWindow();
    });
    icon_.setContextMenu(&menu_);
}

void Tray::attach(QQuickWindow* window) {
    window_ = window;
    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    setKeepRunning(keepRunning_);
    if (trayAvailable) icon_.show();
    connect(window, &QWindow::visibleChanged, this, &Tray::updateToggleText);
    updateToggleText();
}

void Tray::setKeepRunning(bool keep) {
    keepRunning_ = keep;
    // With a tray, closing the window only hides it, like Wave Link minimizing to the notification area.
    QApplication::setQuitOnLastWindowClosed(!(keep && QSystemTrayIcon::isSystemTrayAvailable()));
}

void Tray::showWindow() {
    if (!window_) return;
    window_->show();
    window_->raise();
    window_->requestActivate();
}

void Tray::updateToggleText() {
    const bool visible = window_ && window_->isVisible();
    toggle_->setText(assets_.text(visible ? QStringLiteral("Elgato.BaseClasses.WinUI/Resources/SystemTrayIcon_HideWindowButton")
                                          : QStringLiteral("Elgato.BaseClasses.WinUI/Resources/SystemTrayIcon_ShowWindowButton")));
}
