#include "ScreenshotApp.h"
#include "SettingsDialog.h"
#include "../capture/CaptureFactory.h"
#include "../capture/CaptureStrategy.h"
#include <QPainter>
#include <QTimer>
#include <QKeyEvent>
#include <QMessageBox>
#include <QDir>
#include <QStyle>

namespace Core {
    ScreenshotApp::ScreenshotApp(int &argc, char **argv)
        : QApplication(argc, argv)
          , m_trayIcon(nullptr)
          , m_trayMenu(nullptr) {
        setQuitOnLastWindowClosed(false);

        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            QMessageBox::critical(nullptr, "Screenshot App",
                                  "System tray is not available on this system.");
            return;
        }

        // Initialize capture strategy using factory
        m_captureStrategy = Capture::CaptureFactory::createStrategy(
                Capture::CaptureFactory::StrategyType::Auto,
                this
            );

        setupSystemTray();
    }

    ScreenshotApp::~ScreenshotApp() {
        if (m_trayIcon) {
            m_trayIcon->hide();
        }
    }

    void ScreenshotApp::setupSystemTray() {
        // Create actions
        m_captureAreaAction = new QAction("Capture Area", this);
        m_captureAreaAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
        connect(m_captureAreaAction, &QAction::triggered, this, &ScreenshotApp::captureArea);

        m_captureWindowAction = new QAction("Capture Window", this);
        m_captureWindowAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
        connect(m_captureWindowAction, &QAction::triggered, this, &ScreenshotApp::captureWindow);

        m_aboutAction = new QAction("About", this);
        m_settingsAction = new QAction("Settings", this);
        connect(m_settingsAction, &QAction::triggered, this, &ScreenshotApp::showSettings);

        connect(m_aboutAction, &QAction::triggered, this, &ScreenshotApp::showAbout);

        m_quitAction = new QAction("Quit", this);
        connect(m_quitAction, &QAction::triggered, this, &ScreenshotApp::quit);

        // Create tray menu
        m_trayMenu = new QMenu();
        m_trayMenu->addAction(m_captureAreaAction);
        m_trayMenu->addAction(m_captureWindowAction);
        m_trayMenu->addSeparator();
        m_trayMenu->addAction(m_settingsAction);
        m_trayMenu->addAction(m_aboutAction);
        m_trayMenu->addSeparator();
        m_trayMenu->addAction(m_quitAction);

        // Create tray icon
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setContextMenu(m_trayMenu);
        m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
        m_trayIcon->setToolTip("Screenshot App");
        m_trayIcon->show();

        // Connect tray icon activation
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                captureArea();
            }
            // Single click (Trigger) will show the context menu automatically
        });
    }

    void ScreenshotApp::captureArea() const {
        qDebug() << "Capture area using strategy:" << m_captureStrategy->name();

        // Hide tray icon temporarily
        if (m_trayIcon) {
            //m_trayIcon->hide();
        }

        // Use the strategy to capture area
        m_captureStrategy->captureArea();
    }

    void ScreenshotApp::captureWindow() const {
        qDebug() << "Capture window using strategy:" << m_captureStrategy->name();

        // Hide tray icon temporarily
        if (m_trayIcon) {
          //  m_trayIcon->hide();
        }

        // Use the strategy to capture window
        m_captureStrategy->captureWindow();
    }

    void ScreenshotApp::showAbout() {
        QMessageBox::about(nullptr, "About Screenshot App",
                           "Screenshot App v1.0\n\n"
                           "A simple screenshot tool with editing capabilities.\n\n"
                           "Shortcuts:\n"
                           "• Ctrl+Shift+A: Capture Area\n"
                           "• Ctrl+Shift+W: Capture Window");
    }

    void ScreenshotApp::showSettings() {
        auto *settingsDialog = new SettingsDialog();
        settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        settingsDialog->exec();
    }

    void ScreenshotApp::quit() {
        QApplication::quit();
    }

} // namespace Core
