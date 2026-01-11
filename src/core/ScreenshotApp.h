#ifndef CORE_SCREENSHOTAPP_H
#define CORE_SCREENSHOTAPP_H

#include "hotkeys/HotkeyAction.h"

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <memory>

namespace Hotkeys {
class GlobalHotkeyManager;
} // namespace Hotkeys

namespace Capture {
class CaptureStrategy;
} // namespace Capture

namespace Recording {
class RecordingController;
class RecordingControls;
} // namespace Recording

namespace Upload {
class Uploader;
} // namespace Upload

namespace Core {

class ScreenshotDialog;
class SettingsDialog;
class TextSnipCapture;

class ScreenshotApp : public QApplication
{
    Q_OBJECT

public:
    explicit ScreenshotApp(int &argc, char **argv);
    ~ScreenshotApp() override;

private slots:
    void captureArea() const;
    void captureWindow() const;
    void captureFullScreen() const;
    void captureTextSnip();
    void onScreenshotReady(const QPixmap &screenshot);
    void onTextExtracted(const QString &text, bool success);
    void toggleAreaRecording();                              // Record Area / Stop (toggles)
    void startWindowRecording();                             // Record Window (disabled while recording)
    void onRecordingStateChanged(bool recording);
    void onRecordingFinished(const QString &path);
    void onRecordingFailed(const QString &error);
    // Owns the in-flight upload so it outlives the editor window that triggered it
    // (the editor may close mid-upload). Copies the URL + shows a tray toast on done.
    void startUpload(const QString &localPath, const QString &suggestedName, bool deleteWhenDone,
                     const QString &profileId);
    void showSettings();
    static void showAbout();
    static void quit();

private:
    void setupSystemTray();
    static QIcon createThemedTrayIcon(const QString &iconPath);

    // The tray action a hotkey fires; every hotkey action has one.
    [[nodiscard]] QAction *actionFor(Hotkeys::HotkeyAction action) const;

    // Should this hotkey be swallowed because a selection overlay is already up?
    [[nodiscard]] bool hotkeyBlockedBySelection(Hotkeys::HotkeyAction action) const;

    // Menu hint text only: a tray menu never dispatches a QAction shortcut.
    void refreshActionShortcuts();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_captureAreaAction{};
    QAction *m_captureWindowAction{};
    QAction *m_captureFullScreenAction{};
    QAction *m_textSnipAction{};
    QAction *m_recordAreaAction{};
    QAction *m_recordWindowAction{};
    QAction *m_settingsAction{};
    QAction *m_aboutAction{};
    QAction *m_quitAction{};

    std::unique_ptr<Capture::CaptureStrategy> m_captureStrategy;
    std::unique_ptr<Hotkeys::GlobalHotkeyManager> m_hotkeyManager;
    std::unique_ptr<TextSnipCapture> m_textSnipCapture;
    std::unique_ptr<Recording::RecordingController> m_recordingController;
    Recording::RecordingControls *m_recordingControls = nullptr;   // shown only while recording
    Upload::Uploader *m_uploader = nullptr;   // current upload (one at a time), parented to this
};

} // namespace Core

#endif // CORE_SCREENSHOTAPP_H
