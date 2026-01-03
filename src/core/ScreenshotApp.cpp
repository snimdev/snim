#include "ScreenshotApp.h"
#include "SettingsDialog.h"
#include "TextSnipCapture.h"
#include "core/IconUtil.h"
#include "../capture/CaptureFactory.h"
#include "../capture/strategies/CaptureStrategy.h"
#include <QTimer>
#include <QKeyEvent>
#include <QMessageBox>
#include <QDir>
#include <QStyle>
#include <QPalette>
#include <QFile>

#include "editor/image/ImageEditor.h"

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

        connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotReady, this, &ScreenshotApp::onScreenshotReady);
        connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotFailed, this, [](const QString &error) {
            QMessageBox::warning(nullptr, "Screenshot Failed", error);
        });

        // Initialize text snip capture
        m_textSnipCapture = std::make_unique<TextSnipCapture>(this);
        connect(m_textSnipCapture.get(), &TextSnipCapture::textExtracted,
                this, &ScreenshotApp::onTextExtracted);

        setupSystemTray();
    }

    ScreenshotApp::~ScreenshotApp() {
        if (m_trayIcon) {
            m_trayIcon->hide();
        }
    }

    QIcon ScreenshotApp::createThemedTrayIcon(const QString &iconPath) {
#ifdef Q_OS_MACOS
        // The macOS menu bar is translucent and its shade varies by display, wallpaper,
        // and appearance, so a fixed icon colour can't track it (hence "black on the
        // built-in screen, white on an external one"). Render the glyph opaque and mark
        // the QIcon as a template/mask: macOS then tints it to match the menu bar and
        // highlights it when the menu is open, like a native status item. Only the alpha
        // matters for a mask, so the colour is moot.
        QIcon icon = themedSvgIcon(iconPath, QColor(Qt::black), 22);
        icon.setIsMask(true);
        return icon;
#else
        // Other platforms have no template-image concept: pick a tone from the palette.
        const QColor windowColor = QApplication::palette().color(QPalette::Window);
        const bool isDarkMode = windowColor.lightness() < 128;
        const QColor iconColor(isDarkMode ? "#e0e0e0" : "#1a1a1a");
        return themedSvgIcon(iconPath, iconColor, 22);
#endif
    }

    void ScreenshotApp::setupSystemTray() {
        // Create actions
        m_captureAreaAction = new QAction("Capture Area", this);
        m_captureAreaAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
        connect(m_captureAreaAction, &QAction::triggered, this, &ScreenshotApp::captureArea);

        m_captureWindowAction = new QAction("Capture Window", this);
        m_captureWindowAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
        connect(m_captureWindowAction, &QAction::triggered, this, &ScreenshotApp::captureWindow);

        m_textSnipAction = new QAction("Extract Text (OCR)", this);
        m_textSnipAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
        connect(m_textSnipAction, &QAction::triggered, this, &ScreenshotApp::captureTextSnip);

        // Disable text snip if OCR is not available
        if (!TextSnipCapture::isOCRAvailable()) {
            m_textSnipAction->setEnabled(false);
            m_textSnipAction->setText("Extract Text (OCR not available)");
            m_textSnipAction->setToolTip("Install tesseract-ocr to enable this feature");
        }

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
        m_trayMenu->addAction(m_textSnipAction);
        m_trayMenu->addSeparator();
        m_trayMenu->addAction(m_settingsAction);
        m_trayMenu->addAction(m_aboutAction);
        m_trayMenu->addSeparator();
        m_trayMenu->addAction(m_quitAction);

        // Create tray icon
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setContextMenu(m_trayMenu);
        m_trayIcon->setIcon(createThemedTrayIcon(":/icons/icons/tray-icon.svg"));
        m_trayIcon->setToolTip("Niceshot - Screenshot App");
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
        QString aboutText = "Screenshot App v1.0\n\n"
                           "A simple screenshot tool with editing capabilities.\n\n"
                           "Shortcuts:\n"
                           "• Ctrl+Shift+A: Capture Area\n"
                           "• Ctrl+Shift+W: Capture Window";

        if (TextSnipCapture::isOCRAvailable()) {
            aboutText += "\n• Ctrl+Shift+T: Extract Text (OCR)";
        }

        QMessageBox::about(nullptr, "About Screenshot App", aboutText);
    }

    void ScreenshotApp::showSettings() {
        auto *settingsDialog = new SettingsDialog();
        settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        settingsDialog->exec();
    }


    void ScreenshotApp::onScreenshotReady(const QPixmap &screenshot) {
        qDebug() << "Screenshot ready, opening ImageEditor";

        // Create and show the ImageEditor with the captured screenshot
        auto *editor = new Editor::Image::ImageEditor(screenshot);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        editor->show();
        editor->raise();
        editor->activateWindow();
    }

    void ScreenshotApp::captureTextSnip() {
        qDebug() << "Starting text snip with OCR";

        if (!m_textSnipCapture) {
            QMessageBox::warning(nullptr, "Text Snip",
                               "Text snip feature is not initialized.");
            return;
        }

        m_textSnipCapture->startTextSnip();
    }

    void ScreenshotApp::onTextExtracted(const QString &text, bool success) {
        qDebug() << "Text extraction" << (success ? "succeeded" : "failed");
        qDebug() << "Extracted text:" << text;
    }

    void ScreenshotApp::quit() {
        QApplication::quit();
    }

} // namespace Core
