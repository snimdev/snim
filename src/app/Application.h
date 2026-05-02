#ifndef APP_APPLICATION_H
#define APP_APPLICATION_H

#include "hotkeys/HotkeyAction.h"

#include <QApplication>
#include <memory>

namespace Hotkeys {
class GlobalHotkeyManager;
} // namespace Hotkeys

namespace App {

class CaptureWorkflow;
class RecordingWorkflow;
class SettingsDialog;
class TextSnipWorkflow;
class TrayMenu;
class UploadWorkflow;

/**
 * Composition root: builds the tray, the services and one Mediator per user flow
 * (capture, recording, upload, text snip) and wires them together. What stays here is
 * app-wide: hotkeys, settings, the update check, About and quit.
 */
class Application : public QApplication
{
    Q_OBJECT

public:
    explicit Application(int &argc, char **argv);
    ~Application() override;

private slots:
    void captureTextSnip();
    void onTextExtracted(const QString &text, bool success);
    void showSettings();
    void checkForUpdates();
    static void showAbout();
    void quit();

private:
    // Wires the tray actions to their flows, then shows the icon.
    void setupSystemTray();

    // Should this hotkey be swallowed because a selection overlay is already up?
    [[nodiscard]] bool hotkeyBlockedBySelection(Hotkeys::HotkeyAction action) const;

    std::unique_ptr<TrayMenu> m_tray;
    std::unique_ptr<UploadWorkflow> m_uploadWorkflow;
    std::unique_ptr<CaptureWorkflow> m_captureWorkflow;
    std::unique_ptr<Hotkeys::GlobalHotkeyManager> m_hotkeyManager;
    std::unique_ptr<TextSnipWorkflow> m_textSnipWorkflow;
    std::unique_ptr<RecordingWorkflow> m_recordingWorkflow;
};

} // namespace App

#endif // APP_APPLICATION_H
