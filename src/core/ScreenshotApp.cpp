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
#include <QFileInfo>
#include <QFileDialog>

#include "core/Settings.h"
#include "editor/image/ImageEditor.h"
#include "editor/video/VideoEditor.h"
#include "recording/RecordingController.h"
#include "recording/RecordingControls.h"
#include "upload/Uploader.h"
#include "upload/UploaderFactory.h"
#include <QClipboard>
#include <QUrl>

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

        // Initialize screen recording. RecordingFactory picks the platform backend
        // (ScreenCaptureKit on macOS; a stub elsewhere), so the app is unchanged
        // when Linux/Windows backends are added later.
        m_recordingController = std::make_unique<Recording::RecordingController>(this);
        connect(m_recordingController.get(), &Recording::RecordingController::recordingStateChanged,
                this, &ScreenshotApp::onRecordingStateChanged);
        connect(m_recordingController.get(), &Recording::RecordingController::recordingFinished,
                this, &ScreenshotApp::onRecordingFinished);
        connect(m_recordingController.get(), &Recording::RecordingController::recordingFailed,
                this, &ScreenshotApp::onRecordingFailed);
        // Non-fatal setup problems (e.g. camera/mic access denied): a tray balloon,
        // not a modal box - the recording itself still proceeds.
        connect(m_recordingController.get(), &Recording::RecordingController::recordingWarning,
                this, [this](const QString &message) {
                    if (m_trayIcon)
                        m_trayIcon->showMessage(tr("Niceshot"), message, QSystemTrayIcon::Warning);
                });

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

        m_recordAreaAction = new QAction("Record Area", this);
        m_recordAreaAction->setShortcut(QKeySequence("Ctrl+Shift+R"));
        connect(m_recordAreaAction, &QAction::triggered, this, &ScreenshotApp::toggleAreaRecording);

        m_recordWindowAction = new QAction("Record Window", this);
        connect(m_recordWindowAction, &QAction::triggered, this, &ScreenshotApp::startWindowRecording);

        // Disable recording where the platform backend is unavailable (e.g. macOS < 12.3).
        if (m_recordingController && !m_recordingController->isAvailable()) {
            m_recordAreaAction->setEnabled(false);
            m_recordAreaAction->setText("Record Area (unavailable)");
            m_recordAreaAction->setToolTip("Screen recording requires macOS 12.3 or later");
            m_recordWindowAction->setEnabled(false);
            m_recordWindowAction->setToolTip("Screen recording requires macOS 12.3 or later");
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
        m_trayMenu->addAction(m_recordAreaAction);
        m_trayMenu->addAction(m_recordWindowAction);
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
        connect(editor, &Editor::Image::ImageEditor::uploadRequested,
                this, &ScreenshotApp::startUpload);
        editor->show();
        editor->raise();
        editor->activateWindow();
    }

    void ScreenshotApp::startUpload(const QString &localPath, const QString &suggestedName,
                                    bool deleteWhenDone, const QString &profileId) {
        if (m_uploader) {   // one upload at a time
            if (m_trayIcon)
                m_trayIcon->showMessage(tr("Upload"), tr("An upload is already in progress."),
                                        QSystemTrayIcon::Warning, 3000);
            if (deleteWhenDone)
                QFile::remove(localPath);
            return;
        }
        // Factory yields the configured S3 uploader for the chosen profile (empty =
        // default), or the stub if that profile isn't fully configured.
        m_uploader = Upload::UploaderFactory::create(
                         Upload::UploaderFactory::StrategyType::Auto, this, profileId).release();
        if (m_trayIcon)
            m_trayIcon->showMessage(tr("Uploading…"), QFileInfo(suggestedName).fileName(),
                                    QSystemTrayIcon::Information, 2000);

        auto cleanup = [this, localPath, deleteWhenDone] {
            if (deleteWhenDone)
                QFile::remove(localPath);
            if (m_uploader) { m_uploader->deleteLater(); m_uploader = nullptr; }
        };
        connect(m_uploader, &Upload::Uploader::uploaded, this,
                [this, cleanup](const QUrl &url) {
                    QApplication::clipboard()->setText(url.toString());
                    if (m_trayIcon)
                        m_trayIcon->showMessage(tr("Uploaded - link copied"), url.toString(),
                                                QSystemTrayIcon::Information, 5000);
                    cleanup();
                });
        connect(m_uploader, &Upload::Uploader::failed, this,
                [this, cleanup](const QString &err) {
                    if (m_trayIcon)
                        m_trayIcon->showMessage(tr("Upload failed"), err,
                                                QSystemTrayIcon::Warning, 6000);
                    cleanup();
                });
        m_uploader->upload(localPath, suggestedName);
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

    void ScreenshotApp::toggleAreaRecording() {
        if (!m_recordingController)
            return;
        if (m_recordingController->isRecording())
            m_recordingController->stop();
        else
            m_recordingController->recordArea();
    }

    void ScreenshotApp::startWindowRecording() {
        if (m_recordingController)
            m_recordingController->recordWindow();
    }

    void ScreenshotApp::onRecordingStateChanged(bool recording) {
        if (m_recordAreaAction)
            m_recordAreaAction->setText(recording ? "Stop Recording" : "Record Area");
        if (m_recordWindowAction)
            m_recordWindowAction->setEnabled(!recording);   // one session at a time

        if (recording) {
            if (!m_recordingControls) {
                m_recordingControls = new Recording::RecordingControls();
                connect(m_recordingControls, &Recording::RecordingControls::stopRequested,
                        this, &ScreenshotApp::toggleAreaRecording);
                connect(m_recordingControls, &Recording::RecordingControls::pauseRequested,
                        m_recordingController.get(), &Recording::RecordingController::togglePause);
                connect(m_recordingController.get(), &Recording::RecordingController::recordingDuration,
                        m_recordingControls, &Recording::RecordingControls::setElapsed);
                connect(m_recordingController.get(), &Recording::RecordingController::recordingPausedChanged,
                        m_recordingControls, &Recording::RecordingControls::setPaused);
            }
            m_recordingControls->setElapsed(0);
            m_recordingControls->show();
            m_recordingControls->raise();
        } else if (m_recordingControls) {
            m_recordingControls->hide();
            m_recordingControls->deleteLater();
            m_recordingControls = nullptr;
        }
    }

    void ScreenshotApp::onRecordingFinished(const QString &tempPath) {
        // The recording was written to a temp file; open it in the trim editor,
        // which owns the file from here (save / copy / discard all clean it up).
        auto *editor = new Editor::Video::VideoEditor(tempPath);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        connect(editor, &Editor::Video::VideoEditor::recordingSaved, this,
                [this](const QString &finalPath) {
                    if (m_trayIcon)
                        m_trayIcon->showMessage(tr("Recording saved"),
                                                QFileInfo(finalPath).fileName(),
                                                QSystemTrayIcon::Information, 4000);
                });
        connect(editor, &Editor::Video::VideoEditor::uploadRequested,
                this, &ScreenshotApp::startUpload);
        editor->show();
        editor->raise();
        editor->activateWindow();
    }

    void ScreenshotApp::onRecordingFailed(const QString &error) {
        QMessageBox::warning(nullptr, "Recording Failed", error);
    }

    void ScreenshotApp::quit() {
        QApplication::quit();
    }

} // namespace Core
