#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QColor>
#include <memory>

class QSettings;

namespace Core {

/**
 * Wrapper over QSettings. One place that owns every persisted key and exposes a
 * typed accessor for it, instead of the stringly-typed QSettings("Editor/...")
 * reads/writes that were scattered across SettingsDialog, the tool templates,
 * and BackdropPresets. Each call uses a short-lived QSettings (which syncs on
 * destruction), so there is no shared state to manage.
 */
class Settings {
public:
    // Portable mode: every setting lives in this INI file instead of the platform store.
    // Set before the first setting is read; an empty path restores the platform store.
    static void setPortableFile(const QString &iniPath);
    // <dir>/snim.ini when that file exists, otherwise empty.
    static QString portableFileIn(const QString &dir);
    // The short-lived QSettings every accessor goes through.
    static std::unique_ptr<QSettings> store();

    // General
    static QString screenshotFolder();                 // default: <Pictures>/Screenshots
    static void setScreenshotFolder(const QString &path);
    static QString imageFormat();                       // default: "png"
    static void setImageFormat(const QString &fmt);

    // Recording
    static QString recordingFolder();                  // default: <Movies>/Snim
    static QString recordingFormat();                  // default: "mp4"
    static int recordingFps();                         // default: 30
    static void setRecordingFps(int fps);
    static bool recordingCaptureCursor();              // default: true
    static bool recordingRetina();                     // default: true (capture at native pixel scale)
    static void setRecordingRetina(bool on);
    static QStringList recordingsInProgress();         // RecordingJournal's entries
    static void setRecordingsInProgress(const QStringList &paths);

    // Animation export options per format key ("gif" / "webp"); defaults match AnimationParams.
    static int animationFps(const QString &format);             // default: 10
    static void setAnimationFps(const QString &format, int fps);
    static int animationMaxWidth(const QString &format);        // default: 1200 (0 = original)
    static void setAnimationMaxWidth(const QString &format, int px);
    static int animationQuality(const QString &format);         // default: 75
    static void setAnimationQuality(const QString &format, int quality);
    static bool animationLossless(const QString &format);       // default: false
    static void setAnimationLossless(const QString &format, bool on);
    static int animationLoopCount(const QString &format);       // default: 0 (forever)
    static void setAnimationLoopCount(const QString &format, int count);
    static bool animationOptionsSkip();                         // default: false (ask each export)
    static void setAnimationOptionsSkip(bool on);

    // Recording inputs: camera (webcam circle) + microphone + system audio
    static bool cameraEnabled();                       // default: false
    static void setCameraEnabled(bool on);
    static QByteArray cameraDeviceId();                // QCameraDevice::id(); empty = default
    static void setCameraDeviceId(const QByteArray &id);
    static bool micEnabled();                          // default: false
    static void setMicEnabled(bool on);
    static QByteArray micDeviceId();                   // QAudioDevice::id(); empty = default
    static void setMicDeviceId(const QByteArray &id);
    static bool systemAudioEnabled();                  // default: true
    static void setSystemAudioEnabled(bool on);
    static bool recordingFrameEnabled();               // default: true (border + dim around the recorded area)
    static void setRecordingFrameEnabled(bool on);

    // Upload. NON-SECRET config only - the secrets live in the OS keychain
    // (Core::KeychainStore), never in QSettings.
    static bool uploadEnabled();                       // default: false
    static void setUploadEnabled(bool on);
    // Multi-destination: a JSON array of server profiles + the default profile id.
    static QString uploadProfilesJson();               // default: "" (empty list)
    static void setUploadProfilesJson(const QString &json);
    static QString uploadDefaultProfileId();           // default: ""
    static void setUploadDefaultProfileId(const QString &id);
    // SFTP host-key pins, a JSON object "host:port" -> fingerprint (Upload::KnownHosts
    // owns the shape; this is raw I/O).
    static QString uploadKnownHostKeys();              // default: "" (nothing pinned)
    static void setUploadKnownHostKeys(const QString &json);

    // Global hotkeys under "Hotkeys/<name>", as QKeySequence::PortableText. Absent key =
    // defaultText; "" = explicitly unbound (Hotkeys::HotkeyBindings owns the distinction).
    static QString hotkey(const QString &name, const QString &defaultText);
    static void setHotkey(const QString &name, const QString &text);
    static bool hasHotkey(const QString &name);
    static void removeHotkey(const QString &name);   // hotkey() then returns its default
    // Last hotkey defaults migration applied to the stored bindings.
    static int hotkeyDefaultsVersion();                // default: 0 (rc.1 or a fresh store)
    static void setHotkeyDefaultsVersion(int version);
    // True once KDE's copy of Snim's rc.1 shortcuts was moved to the current bindings.
    static bool kdeKeysMoved();                        // default: false
    static void setKdeKeysMoved(bool on);
    // The screenshot key swap (Hotkeys::ScreenshotKeySwap owns the JSON shape; raw I/O here).
    static QString screenshotKeyMemento();             // default: "" (not swapped)
    static void setScreenshotKeyMemento(const QString &json);
    // True once the user answered "Don't ask again" to the first-run screenshot key offer.
    static bool screenshotKeyOfferDismissed();         // default: false
    static void setScreenshotKeyOfferDismissed(bool on);

    // Editor annotation defaults
    static QColor editorForeground();                   // default: red
    static void setEditorForeground(const QColor &c);
    static QColor editorBackground();                   // default: transparent
    static void setEditorBackground(const QColor &c);

    // Desktop integration (Linux/KDE): true once the user answered "Never ask again" to the
    // prompt that offers to register Snim's desktop entry for KWin's fast capture path.
    static bool desktopIntegrationPromptDismissed();    // default: false
    static void setDesktopIntegrationPromptDismissed(bool on);

    // Backdrop preset store (BackdropPresets owns the JSON shape; this is raw I/O).
    static QString backdropPresetsJson();
    static void setBackdropPresetsJson(const QString &json);
    static QString backdropDefaultName();
    static void setBackdropDefaultName(const QString &name);
};

} // namespace Core

#endif // CORE_SETTINGS_H
