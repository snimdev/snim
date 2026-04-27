#ifndef CORE_SCREENSHOTAPP_H
#define CORE_SCREENSHOTAPP_H

#include "core/DesktopIntegration.h"
#include "hotkeys/HotkeyAction.h"
#include "editor/annotations/AnnotationSet.h"

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <functional>
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

namespace OCR {
class TextSnipCapture;
} // namespace OCR

namespace Core {

class ScreenshotDialog;
class SettingsDialog;

class ScreenshotApp : public QApplication
{
    Q_OBJECT

public:
    explicit ScreenshotApp(int &argc, char **argv);
    ~ScreenshotApp() override;

    // Is the KWin authorization offer worth making at all? Pure so it can be tested
    // without a live capture; the caller supplies the four live facts. An AppImage can
    // never be authorized (KWin matches /proc/PID/exe, which its mount point changes
    // every launch), so there the offer is a promise Snim cannot keep.
    [[nodiscard]] static bool kwinPromptApplicable(bool dismissed, DesktopIntegration::Status status,
                                                   bool alreadyShown, bool fromAppImage);

private slots:
    void captureArea() const;
    void captureWindow() const;
    void captureFullScreen() const;
    void captureTextSnip();
    void onScreenshotReady(const QPixmap &screenshot, const Editor::AnnotationSet &annotations);
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
    void checkForUpdates();
    static void showAbout();
    static void quit();

private:
    void setupSystemTray();

    // Recordings a crash or a failure left unsaved: open each in the editor, or discard it.
    void offerUnsavedRecordings();
    void offerRecording(const QString &path, QMessageBox::Icon icon, const QString &title,
                        const QString &text);
    void openImageEditor(const QPixmap &image, const Editor::AnnotationSet &annotations);

    // KWin refused ScreenShot2: offer to install the desktop entry that authorizes it,
    // then resume the pending capture (true retries it, false takes the slow fallback).
    void askForKWinAuthorization(const std::function<void(bool)> &resume);

    // Writes the desktop entry and reports the outcome; shared by the prompt and the tray action.
    void runDesktopIntegrationSetup();
    void refreshDesktopIntegrationAction() const;

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
    QAction *m_checkUpdatesAction{};
    QAction *m_desktopIntegrationAction{};   // Linux only, hidden once the entry is in place
    QAction *m_quitAction{};

    bool m_kwinAuthPromptShown = false;      // the offer is made once per run

    std::unique_ptr<Capture::CaptureStrategy> m_captureStrategy;
    std::unique_ptr<Hotkeys::GlobalHotkeyManager> m_hotkeyManager;
    std::unique_ptr<OCR::TextSnipCapture> m_textSnipCapture;
    std::unique_ptr<Recording::RecordingController> m_recordingController;
    Recording::RecordingControls *m_recordingControls = nullptr;   // shown only while recording
    QString m_partialRecording;   // footage the failing recording kept, until recordingFailed
    Upload::Uploader *m_uploader = nullptr;   // current upload (one at a time), parented to this
};

} // namespace Core

#endif // CORE_SCREENSHOTAPP_H
