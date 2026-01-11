#include "ScreenshotApp.h"
#include "SettingsDialog.h"
#include "ocr/TextSnipCapture.h"
#include "core/IconUtil.h"
#include "core/Perf.h"
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
#include "capture/AreaSelector.h"
#include "hotkeys/GlobalHotkeyManager.h"
#include "hotkeys/HotkeyBindings.h"
#include "editor/image/ImageEditor.h"
#include "editor/video/VideoEditor.h"
#include "recording/RecordingController.h"
#include "recording/RecordingControls.h"
#include "upload/Uploader.h"
#include "upload/UploaderFactory.h"
#include <QClipboard>
#include <QUrl>
#include <algorithm>

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
        m_textSnipCapture = std::make_unique<OCR::TextSnipCapture>(this);
        connect(m_textSnipCapture.get(), &OCR::TextSnipCapture::textExtracted,
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
                        m_trayIcon->showMessage(tr("Snim"), message, QSystemTrayIcon::Warning);
                });

        setupSystemTray();

        // Deferred: the backends register against a running event loop.
        QTimer::singleShot(0, this, [this] {
            m_hotkeyManager = std::make_unique<Hotkeys::GlobalHotkeyManager>(this);
            connect(m_hotkeyManager.get(), &Hotkeys::GlobalHotkeyManager::actionTriggered,
                    this, [this](Hotkeys::HotkeyAction action) {
                        if (hotkeyBlockedBySelection(action))
                            return;
                        // trigger() on a disabled action is a no-op, so the OCR /
                        // recorder availability gating carries over unchanged.
                        if (QAction *target = actionFor(action))
                            target->trigger();
                    });
            connect(m_hotkeyManager.get(), &Hotkeys::GlobalHotkeyManager::registrationFailed,
                    this, [this](const QString &message) {
                        if (m_trayIcon)
                            m_trayIcon->showMessage(tr("Hotkey unavailable"), message,
                                                    QSystemTrayIcon::Warning, 5000);
                    });
            m_hotkeyManager->applyBindings();
            refreshActionShortcuts();
        });
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
        connect(m_captureAreaAction, &QAction::triggered, this, &ScreenshotApp::captureArea);

        m_captureWindowAction = new QAction("Capture Window", this);
        connect(m_captureWindowAction, &QAction::triggered, this, &ScreenshotApp::captureWindow);

        m_captureFullScreenAction = new QAction("Capture Full Screen", this);
        connect(m_captureFullScreenAction, &QAction::triggered, this, &ScreenshotApp::captureFullScreen);

        m_textSnipAction = new QAction("Extract Text (OCR)", this);
        connect(m_textSnipAction, &QAction::triggered, this, &ScreenshotApp::captureTextSnip);

        // Disable text snip if OCR is not available
        if (!OCR::TextSnipCapture::isOCRAvailable()) {
            m_textSnipAction->setEnabled(false);
            m_textSnipAction->setText("Extract Text (OCR not available)");
            m_textSnipAction->setToolTip("Install tesseract-ocr to enable this feature");
        }

        m_recordAreaAction = new QAction("Record Area", this);
        connect(m_recordAreaAction, &QAction::triggered, this, &ScreenshotApp::toggleAreaRecording);

        m_recordWindowAction = new QAction("Record Window", this);
        connect(m_recordWindowAction, &QAction::triggered, this, &ScreenshotApp::startWindowRecording);

        // Disable recording where the platform backend is unavailable (e.g. macOS < 12.3).
        if (m_recordingController && !m_recordingController->isAvailable()) {
#if defined(Q_OS_MACOS)
            const QString reason = "Screen recording requires macOS 12.3 or later";
#elif defined(Q_OS_LINUX)
            const QString reason = "Screen recording requires the ScreenCast portal and "
                                   "GStreamer (with an H.264 encoder)";
#else
            const QString reason = "Screen recording is not supported on this platform yet";
#endif
            m_recordAreaAction->setEnabled(false);
            m_recordAreaAction->setText("Record Area (unavailable)");
            m_recordAreaAction->setToolTip(reason);
            m_recordWindowAction->setEnabled(false);
            m_recordWindowAction->setToolTip(reason);
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
        m_trayMenu->addAction(m_captureFullScreenAction);
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
        m_trayIcon->setToolTip("Snim - Screenshot App");
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
        Perf::markCaptureStart("area");

        // Hide tray icon temporarily
        if (m_trayIcon) {
            //m_trayIcon->hide();
        }

        // Use the strategy to capture area
        m_captureStrategy->captureArea();
    }

    void ScreenshotApp::captureWindow() const {
        qDebug() << "Capture window using strategy:" << m_captureStrategy->name();
        Perf::markCaptureStart("window");

        // Hide tray icon temporarily
        if (m_trayIcon) {
          //  m_trayIcon->hide();
        }

        // Use the strategy to capture window
        m_captureStrategy->captureWindow();
    }

    void ScreenshotApp::captureFullScreen() const {
        qDebug() << "Capture full screen using strategy:" << m_captureStrategy->name();
        Perf::markCaptureStart("fullscreen");

        // The strategy emits screenshotReady, so this joins the normal editor flow.
        m_captureStrategy->captureFullScreen();
    }

    QAction *ScreenshotApp::actionFor(const Hotkeys::HotkeyAction action) const {
        switch (action) {
        case Hotkeys::HotkeyAction::CaptureArea:       return m_captureAreaAction;
        case Hotkeys::HotkeyAction::CaptureWindow:     return m_captureWindowAction;
        case Hotkeys::HotkeyAction::CaptureFullScreen: return m_captureFullScreenAction;
        case Hotkeys::HotkeyAction::OcrTextSnip:       return m_textSnipAction;
        case Hotkeys::HotkeyAction::RecordArea:        return m_recordAreaAction;
        case Hotkeys::HotkeyAction::RecordWindow:      return m_recordWindowAction;
        }
        return nullptr;
    }

    bool ScreenshotApp::hotkeyBlockedBySelection(const Hotkeys::HotkeyAction action) const {
        // A stateless widget scan on purpose, not a latch: the overlay's Esc-cancel
        // path emits no completion signal, so a flag would stay stuck.
        const auto widgets = QApplication::topLevelWidgets();
        const bool selecting = std::any_of(widgets.cbegin(), widgets.cend(), [](const QWidget *w) {
            return w->isVisible() && qobject_cast<const Capture::AreaSelector *>(w) != nullptr;
        });
        if (!selecting)
            return false;

        // Record Area doubles as Stop, so it still passes while a recording runs.
        if (action == Hotkeys::HotkeyAction::RecordArea)
            return !(m_recordingController && m_recordingController->isRecording());
        return true;
    }

    void ScreenshotApp::refreshActionShortcuts() {
        for (const Hotkeys::HotkeyAction action : Hotkeys::allHotkeyActions()) {
            if (QAction *target = actionFor(action))
                target->setShortcut(Hotkeys::HotkeyBindings::sequence(action));
        }
    }

    void ScreenshotApp::showAbout() {
        // Rich text so the website is a clickable link.
        QString aboutText = "<b>Snim 1.0</b><br><br>"
                           "A screenshot tool with editing capabilities.<br><br>"
                           "Darko Gjorgjijoski<br>"
                           "<a href=\"https://snim.dev\">snim.dev</a><br><br>"
                           "Shortcuts:";

        bool anyBound = false;
        for (const Hotkeys::HotkeyAction action : Hotkeys::allHotkeyActions()) {
            if (action == Hotkeys::HotkeyAction::OcrTextSnip && !OCR::TextSnipCapture::isOCRAvailable())
                continue;
            const QKeySequence seq = Hotkeys::HotkeyBindings::sequence(action);
            if (seq.isEmpty())
                continue;
            aboutText += "<br>• " + Hotkeys::hotkeyActionDescription(action).toHtmlEscaped() + ": "
                         + seq.toString(QKeySequence::NativeText).toHtmlEscaped();
            anyBound = true;
        }
        if (!anyBound)
            aboutText += "<br>• None configured";

        QMessageBox::about(nullptr, "About Snim", aboutText);
    }

    void ScreenshotApp::showSettings() {
        auto *settingsDialog = new SettingsDialog();
        settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, [this] {
            if (m_hotkeyManager)
                m_hotkeyManager->applyBindings();
            refreshActionShortcuts();
        });
        settingsDialog->exec();
    }


    void ScreenshotApp::onScreenshotReady(const QPixmap &screenshot) {
        // Fullscreen shows no overlay, so this is its visible-endpoint; area/window already reported.
        Perf::reportCaptureShown("frame ready");
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

        Perf::markCaptureStart("textsnip");
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
