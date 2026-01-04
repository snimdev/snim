#include "core/Settings.h"

#include <QSettings>
#include <QStandardPaths>

namespace Core {

namespace {
constexpr auto kScreenshotFolder = "General/ScreenshotFolder";
constexpr auto kImageFormat      = "General/ImageFormat";
constexpr auto kForeground       = "Editor/ForegroundColor";
constexpr auto kBackground       = "Editor/BackgroundColor";
constexpr auto kBackdropPresets  = "editor/backdropPresets";
constexpr auto kBackdropDefault  = "editor/backdropDefault";
constexpr auto kRecordingFolder        = "Recording/Folder";
constexpr auto kRecordingFormat        = "Recording/Format";
constexpr auto kRecordingFps           = "Recording/Fps";
constexpr auto kRecordingCaptureCursor = "Recording/CaptureCursor";
constexpr auto kCameraEnabled          = "Recording/CameraEnabled";
constexpr auto kCameraDeviceId         = "Recording/CameraDeviceId";
constexpr auto kMicEnabled             = "Recording/MicEnabled";
constexpr auto kMicDeviceId            = "Recording/MicDeviceId";
constexpr auto kSystemAudioEnabled     = "Recording/SystemAudioEnabled";
constexpr auto kRecordingFrameEnabled  = "Recording/FrameEnabled";
constexpr auto kRecordingRetina        = "Recording/RetinaCapture";
constexpr auto kUploadEnabled          = "Upload/Enabled";
constexpr auto kUploadEndpoint         = "Upload/Endpoint";
constexpr auto kUploadRegion           = "Upload/Region";
constexpr auto kUploadBucket           = "Upload/Bucket";
constexpr auto kUploadAccessKeyId      = "Upload/AccessKeyId";
constexpr auto kUploadKeyPrefix        = "Upload/KeyPrefix";
constexpr auto kUploadPublicBaseUrl    = "Upload/PublicBaseUrl";
constexpr auto kUploadForcePathStyle   = "Upload/ForcePathStyle";
constexpr auto kUploadProfilesJson     = "Upload/Profiles";
constexpr auto kUploadDefaultProfileId = "Upload/DefaultProfileId";
constexpr auto kUploadKnownHostKeys    = "Upload/KnownHostKeys";
}

QString Settings::screenshotFolder()
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                        + "/Screenshots";
    return QSettings().value(kScreenshotFolder, def).toString();
}
void Settings::setScreenshotFolder(const QString &path) { QSettings().setValue(kScreenshotFolder, path); }

QString Settings::imageFormat() { return QSettings().value(kImageFormat, "png").toString(); }
void Settings::setImageFormat(const QString &fmt) { QSettings().setValue(kImageFormat, fmt); }

QString Settings::recordingFolder()
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
                        + "/Niceshot";
    return QSettings().value(kRecordingFolder, def).toString();
}
void Settings::setRecordingFolder(const QString &path) { QSettings().setValue(kRecordingFolder, path); }

QString Settings::recordingFormat() { return QSettings().value(kRecordingFormat, "mp4").toString(); }
void Settings::setRecordingFormat(const QString &fmt) { QSettings().setValue(kRecordingFormat, fmt); }

int Settings::recordingFps() { return QSettings().value(kRecordingFps, 30).toInt(); }
void Settings::setRecordingFps(int fps) { QSettings().setValue(kRecordingFps, fps); }

bool Settings::recordingCaptureCursor() { return QSettings().value(kRecordingCaptureCursor, true).toBool(); }
void Settings::setRecordingCaptureCursor(bool on) { QSettings().setValue(kRecordingCaptureCursor, on); }

bool Settings::recordingRetina() { return QSettings().value(kRecordingRetina, true).toBool(); }
void Settings::setRecordingRetina(bool on) { QSettings().setValue(kRecordingRetina, on); }

bool Settings::cameraEnabled() { return QSettings().value(kCameraEnabled, false).toBool(); }
void Settings::setCameraEnabled(bool on) { QSettings().setValue(kCameraEnabled, on); }
QByteArray Settings::cameraDeviceId() { return QSettings().value(kCameraDeviceId).toByteArray(); }
void Settings::setCameraDeviceId(const QByteArray &id) { QSettings().setValue(kCameraDeviceId, id); }

bool Settings::micEnabled() { return QSettings().value(kMicEnabled, false).toBool(); }
void Settings::setMicEnabled(bool on) { QSettings().setValue(kMicEnabled, on); }
QByteArray Settings::micDeviceId() { return QSettings().value(kMicDeviceId).toByteArray(); }
void Settings::setMicDeviceId(const QByteArray &id) { QSettings().setValue(kMicDeviceId, id); }

bool Settings::systemAudioEnabled() { return QSettings().value(kSystemAudioEnabled, true).toBool(); }
void Settings::setSystemAudioEnabled(bool on) { QSettings().setValue(kSystemAudioEnabled, on); }

bool Settings::recordingFrameEnabled() { return QSettings().value(kRecordingFrameEnabled, true).toBool(); }
void Settings::setRecordingFrameEnabled(bool on) { QSettings().setValue(kRecordingFrameEnabled, on); }

bool Settings::uploadEnabled() { return QSettings().value(kUploadEnabled, false).toBool(); }
void Settings::setUploadEnabled(bool on) { QSettings().setValue(kUploadEnabled, on); }
QString Settings::uploadEndpoint() { return QSettings().value(kUploadEndpoint, "s3.amazonaws.com").toString(); }
void Settings::setUploadEndpoint(const QString &v) { QSettings().setValue(kUploadEndpoint, v); }
QString Settings::uploadRegion() { return QSettings().value(kUploadRegion, "us-east-1").toString(); }
void Settings::setUploadRegion(const QString &v) { QSettings().setValue(kUploadRegion, v); }
QString Settings::uploadBucket() { return QSettings().value(kUploadBucket).toString(); }
void Settings::setUploadBucket(const QString &v) { QSettings().setValue(kUploadBucket, v); }
QString Settings::uploadAccessKeyId() { return QSettings().value(kUploadAccessKeyId).toString(); }
void Settings::setUploadAccessKeyId(const QString &v) { QSettings().setValue(kUploadAccessKeyId, v); }
QString Settings::uploadKeyPrefix() { return QSettings().value(kUploadKeyPrefix).toString(); }
void Settings::setUploadKeyPrefix(const QString &v) { QSettings().setValue(kUploadKeyPrefix, v); }
QString Settings::uploadPublicBaseUrl() { return QSettings().value(kUploadPublicBaseUrl).toString(); }
void Settings::setUploadPublicBaseUrl(const QString &v) { QSettings().setValue(kUploadPublicBaseUrl, v); }
bool Settings::uploadForcePathStyle() { return QSettings().value(kUploadForcePathStyle, false).toBool(); }
void Settings::setUploadForcePathStyle(bool on) { QSettings().setValue(kUploadForcePathStyle, on); }
QString Settings::uploadProfilesJson() { return QSettings().value(kUploadProfilesJson).toString(); }
void Settings::setUploadProfilesJson(const QString &json) { QSettings().setValue(kUploadProfilesJson, json); }
QString Settings::uploadDefaultProfileId() { return QSettings().value(kUploadDefaultProfileId).toString(); }
void Settings::setUploadDefaultProfileId(const QString &id) { QSettings().setValue(kUploadDefaultProfileId, id); }
QString Settings::uploadKnownHostKeys() { return QSettings().value(kUploadKnownHostKeys).toString(); }
void Settings::setUploadKnownHostKeys(const QString &json) { QSettings().setValue(kUploadKnownHostKeys, json); }

QColor Settings::editorForeground() { return QSettings().value(kForeground, QColor(Qt::red)).value<QColor>(); }
void Settings::setEditorForeground(const QColor &c) { QSettings().setValue(kForeground, c); }

QColor Settings::editorBackground() { return QSettings().value(kBackground, QColor(Qt::transparent)).value<QColor>(); }
void Settings::setEditorBackground(const QColor &c) { QSettings().setValue(kBackground, c); }

QString Settings::backdropPresetsJson() { return QSettings().value(kBackdropPresets).toString(); }
void Settings::setBackdropPresetsJson(const QString &json) { QSettings().setValue(kBackdropPresets, json); }

QString Settings::backdropDefaultName() { return QSettings().value(kBackdropDefault).toString(); }
void Settings::setBackdropDefaultName(const QString &name) { QSettings().setValue(kBackdropDefault, name); }

} // namespace Core
