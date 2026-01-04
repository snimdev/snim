#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <QString>
#include <QByteArray>
#include <QColor>

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
    // General
    static QString screenshotFolder();                 // default: <Pictures>/Screenshots
    static void setScreenshotFolder(const QString &path);
    static QString imageFormat();                       // default: "png"
    static void setImageFormat(const QString &fmt);

    // Recording
    static QString recordingFolder();                  // default: <Movies>/Niceshot
    static void setRecordingFolder(const QString &path);
    static QString recordingFormat();                  // default: "mp4"
    static void setRecordingFormat(const QString &fmt);
    static int recordingFps();                         // default: 30
    static void setRecordingFps(int fps);
    static bool recordingCaptureCursor();              // default: true
    static void setRecordingCaptureCursor(bool on);
    static bool recordingRetina();                     // default: true (capture at native pixel scale)
    static void setRecordingRetina(bool on);

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

    // Upload (S3-compatible). NON-SECRET config only - the secret access key lives in
    // the OS keychain (Core::KeychainStore), never in QSettings.
    static bool uploadEnabled();                       // default: false
    static void setUploadEnabled(bool on);
    static QString uploadEndpoint();                   // default: "s3.amazonaws.com"
    static void setUploadEndpoint(const QString &v);
    static QString uploadRegion();                     // default: "us-east-1" (R2 needs "auto")
    static void setUploadRegion(const QString &v);
    static QString uploadBucket();
    static void setUploadBucket(const QString &v);
    static QString uploadAccessKeyId();                // keychain account for the secret
    static void setUploadAccessKeyId(const QString &v);
    static QString uploadKeyPrefix();                  // default: "" (e.g. "screenshots/")
    static void setUploadKeyPrefix(const QString &v);
    static QString uploadPublicBaseUrl();              // default: "" (required for R2 public links)
    static void setUploadPublicBaseUrl(const QString &v);
    static bool uploadForcePathStyle();                // default: false (MinIO/Wasabi need true)
    static void setUploadForcePathStyle(bool on);
    // Multi-destination: a JSON array of server profiles + the default profile id.
    // (The legacy single-config accessors above are read once by the one-time migration.)
    static QString uploadProfilesJson();               // default: "" (empty list)
    static void setUploadProfilesJson(const QString &json);
    static QString uploadDefaultProfileId();           // default: ""
    static void setUploadDefaultProfileId(const QString &id);
    // SFTP host-key pins, a JSON object "host:port" -> fingerprint (Upload::KnownHosts
    // owns the shape; this is raw I/O).
    static QString uploadKnownHostKeys();              // default: "" (nothing pinned)
    static void setUploadKnownHostKeys(const QString &json);

    // Editor annotation defaults
    static QColor editorForeground();                   // default: red
    static void setEditorForeground(const QColor &c);
    static QColor editorBackground();                   // default: transparent
    static void setEditorBackground(const QColor &c);

    // Backdrop preset store (BackdropPresets owns the JSON shape; this is raw I/O).
    static QString backdropPresetsJson();
    static void setBackdropPresetsJson(const QString &json);
    static QString backdropDefaultName();
    static void setBackdropDefaultName(const QString &name);
};

} // namespace Core

#endif // CORE_SETTINGS_H
