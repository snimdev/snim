#include "core/Settings.h"

#include <QFileInfo>
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
constexpr auto kRecordingsInProgress   = "Recording/InProgress";
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
constexpr auto kHotkeyCaptureArea       = "Hotkeys/CaptureArea";
constexpr auto kHotkeyCaptureWindow     = "Hotkeys/CaptureWindow";
constexpr auto kHotkeyCaptureFullScreen = "Hotkeys/CaptureFullScreen";
constexpr auto kHotkeyOcrTextSnip       = "Hotkeys/OcrTextSnip";
constexpr auto kHotkeyRecordArea        = "Hotkeys/RecordArea";
constexpr auto kHotkeyRecordWindow      = "Hotkeys/RecordWindow";
constexpr auto kDesktopIntegrationDismissed = "DesktopIntegration/PromptDismissed";
constexpr auto kAnimationOptionsSkip   = "Animation/SkipOptions";

// "Animation/<format>/<field>", one group per export format.
QString animationKey(const QString &format, const char *field)
{
    return QStringLiteral("Animation/%1/%2").arg(format, QLatin1String(field));
}

QString portablePath;
}

void Settings::setPortableFile(const QString &iniPath) { portablePath = iniPath; }
QString Settings::portableFile() { return portablePath; }

QString Settings::portableFileIn(const QString &dir)
{
    const QString path = dir + QStringLiteral("/snim.ini");
    return QFileInfo(path).isFile() ? path : QString();
}

std::unique_ptr<QSettings> Settings::store()
{
    if (portablePath.isEmpty())
        return std::make_unique<QSettings>();
    return std::make_unique<QSettings>(portablePath, QSettings::IniFormat);
}

QString Settings::screenshotFolder()
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                        + "/Screenshots";
    return store()->value(kScreenshotFolder, def).toString();
}
void Settings::setScreenshotFolder(const QString &path) { store()->setValue(kScreenshotFolder, path); }

QString Settings::imageFormat() { return store()->value(kImageFormat, "png").toString(); }
void Settings::setImageFormat(const QString &fmt) { store()->setValue(kImageFormat, fmt); }

QString Settings::recordingFolder()
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
                        + "/Snim";
    return store()->value(kRecordingFolder, def).toString();
}
void Settings::setRecordingFolder(const QString &path) { store()->setValue(kRecordingFolder, path); }

QString Settings::recordingFormat() { return store()->value(kRecordingFormat, "mp4").toString(); }
void Settings::setRecordingFormat(const QString &fmt) { store()->setValue(kRecordingFormat, fmt); }

int Settings::recordingFps() { return store()->value(kRecordingFps, 30).toInt(); }
void Settings::setRecordingFps(int fps) { store()->setValue(kRecordingFps, fps); }

bool Settings::recordingCaptureCursor() { return store()->value(kRecordingCaptureCursor, true).toBool(); }
void Settings::setRecordingCaptureCursor(bool on) { store()->setValue(kRecordingCaptureCursor, on); }

bool Settings::recordingRetina() { return store()->value(kRecordingRetina, true).toBool(); }
void Settings::setRecordingRetina(bool on) { store()->setValue(kRecordingRetina, on); }

QStringList Settings::recordingsInProgress() { return store()->value(kRecordingsInProgress).toStringList(); }
void Settings::setRecordingsInProgress(const QStringList &paths) { store()->setValue(kRecordingsInProgress, paths); }

int Settings::animationFps(const QString &format) { return store()->value(animationKey(format, "Fps"), 10).toInt(); }
void Settings::setAnimationFps(const QString &format, int fps) { store()->setValue(animationKey(format, "Fps"), fps); }
int Settings::animationMaxWidth(const QString &format) { return store()->value(animationKey(format, "MaxWidth"), 1200).toInt(); }
void Settings::setAnimationMaxWidth(const QString &format, int px) { store()->setValue(animationKey(format, "MaxWidth"), px); }
int Settings::animationQuality(const QString &format) { return store()->value(animationKey(format, "Quality"), 75).toInt(); }
void Settings::setAnimationQuality(const QString &format, int quality) { store()->setValue(animationKey(format, "Quality"), quality); }
bool Settings::animationLossless(const QString &format) { return store()->value(animationKey(format, "Lossless"), false).toBool(); }
void Settings::setAnimationLossless(const QString &format, bool on) { store()->setValue(animationKey(format, "Lossless"), on); }
int Settings::animationLoopCount(const QString &format) { return store()->value(animationKey(format, "LoopCount"), 0).toInt(); }
void Settings::setAnimationLoopCount(const QString &format, int count) { store()->setValue(animationKey(format, "LoopCount"), count); }
bool Settings::animationOptionsSkip() { return store()->value(kAnimationOptionsSkip, false).toBool(); }
void Settings::setAnimationOptionsSkip(bool on) { store()->setValue(kAnimationOptionsSkip, on); }

bool Settings::cameraEnabled() { return store()->value(kCameraEnabled, false).toBool(); }
void Settings::setCameraEnabled(bool on) { store()->setValue(kCameraEnabled, on); }
QByteArray Settings::cameraDeviceId() { return store()->value(kCameraDeviceId).toByteArray(); }
void Settings::setCameraDeviceId(const QByteArray &id) { store()->setValue(kCameraDeviceId, id); }

bool Settings::micEnabled() { return store()->value(kMicEnabled, false).toBool(); }
void Settings::setMicEnabled(bool on) { store()->setValue(kMicEnabled, on); }
QByteArray Settings::micDeviceId() { return store()->value(kMicDeviceId).toByteArray(); }
void Settings::setMicDeviceId(const QByteArray &id) { store()->setValue(kMicDeviceId, id); }

bool Settings::systemAudioEnabled() { return store()->value(kSystemAudioEnabled, true).toBool(); }
void Settings::setSystemAudioEnabled(bool on) { store()->setValue(kSystemAudioEnabled, on); }

bool Settings::recordingFrameEnabled() { return store()->value(kRecordingFrameEnabled, true).toBool(); }
void Settings::setRecordingFrameEnabled(bool on) { store()->setValue(kRecordingFrameEnabled, on); }

bool Settings::uploadEnabled() { return store()->value(kUploadEnabled, false).toBool(); }
void Settings::setUploadEnabled(bool on) { store()->setValue(kUploadEnabled, on); }
QString Settings::uploadEndpoint() { return store()->value(kUploadEndpoint, "s3.amazonaws.com").toString(); }
void Settings::setUploadEndpoint(const QString &v) { store()->setValue(kUploadEndpoint, v); }
QString Settings::uploadRegion() { return store()->value(kUploadRegion, "us-east-1").toString(); }
void Settings::setUploadRegion(const QString &v) { store()->setValue(kUploadRegion, v); }
QString Settings::uploadBucket() { return store()->value(kUploadBucket).toString(); }
void Settings::setUploadBucket(const QString &v) { store()->setValue(kUploadBucket, v); }
QString Settings::uploadAccessKeyId() { return store()->value(kUploadAccessKeyId).toString(); }
void Settings::setUploadAccessKeyId(const QString &v) { store()->setValue(kUploadAccessKeyId, v); }
QString Settings::uploadKeyPrefix() { return store()->value(kUploadKeyPrefix).toString(); }
void Settings::setUploadKeyPrefix(const QString &v) { store()->setValue(kUploadKeyPrefix, v); }
QString Settings::uploadPublicBaseUrl() { return store()->value(kUploadPublicBaseUrl).toString(); }
void Settings::setUploadPublicBaseUrl(const QString &v) { store()->setValue(kUploadPublicBaseUrl, v); }
bool Settings::uploadForcePathStyle() { return store()->value(kUploadForcePathStyle, false).toBool(); }
void Settings::setUploadForcePathStyle(bool on) { store()->setValue(kUploadForcePathStyle, on); }
QString Settings::uploadProfilesJson() { return store()->value(kUploadProfilesJson).toString(); }
void Settings::setUploadProfilesJson(const QString &json) { store()->setValue(kUploadProfilesJson, json); }
QString Settings::uploadDefaultProfileId() { return store()->value(kUploadDefaultProfileId).toString(); }
void Settings::setUploadDefaultProfileId(const QString &id) { store()->setValue(kUploadDefaultProfileId, id); }
QString Settings::uploadKnownHostKeys() { return store()->value(kUploadKnownHostKeys).toString(); }
void Settings::setUploadKnownHostKeys(const QString &json) { store()->setValue(kUploadKnownHostKeys, json); }

// The default only applies when the key is absent; a stored "" stays "unbound".
QString Settings::hotkeyCaptureArea() { return store()->value(kHotkeyCaptureArea, "Ctrl+Shift+A").toString(); }
void Settings::setHotkeyCaptureArea(const QString &seq) { store()->setValue(kHotkeyCaptureArea, seq); }
QString Settings::hotkeyCaptureWindow() { return store()->value(kHotkeyCaptureWindow, "Ctrl+Shift+W").toString(); }
void Settings::setHotkeyCaptureWindow(const QString &seq) { store()->setValue(kHotkeyCaptureWindow, seq); }
QString Settings::hotkeyCaptureFullScreen() { return store()->value(kHotkeyCaptureFullScreen, "").toString(); }
void Settings::setHotkeyCaptureFullScreen(const QString &seq) { store()->setValue(kHotkeyCaptureFullScreen, seq); }
QString Settings::hotkeyOcrTextSnip() { return store()->value(kHotkeyOcrTextSnip, "Ctrl+Shift+T").toString(); }
void Settings::setHotkeyOcrTextSnip(const QString &seq) { store()->setValue(kHotkeyOcrTextSnip, seq); }
QString Settings::hotkeyRecordArea() { return store()->value(kHotkeyRecordArea, "Ctrl+Shift+R").toString(); }
void Settings::setHotkeyRecordArea(const QString &seq) { store()->setValue(kHotkeyRecordArea, seq); }
QString Settings::hotkeyRecordWindow() { return store()->value(kHotkeyRecordWindow, "").toString(); }
void Settings::setHotkeyRecordWindow(const QString &seq) { store()->setValue(kHotkeyRecordWindow, seq); }

QColor Settings::editorForeground() { return store()->value(kForeground, QColor(Qt::red)).value<QColor>(); }
void Settings::setEditorForeground(const QColor &c) { store()->setValue(kForeground, c); }

QColor Settings::editorBackground() { return store()->value(kBackground, QColor(Qt::transparent)).value<QColor>(); }
void Settings::setEditorBackground(const QColor &c) { store()->setValue(kBackground, c); }

bool Settings::desktopIntegrationPromptDismissed()
{
    return store()->value(kDesktopIntegrationDismissed, false).toBool();
}
void Settings::setDesktopIntegrationPromptDismissed(bool on)
{
    store()->setValue(kDesktopIntegrationDismissed, on);
}

QString Settings::backdropPresetsJson() { return store()->value(kBackdropPresets).toString(); }
void Settings::setBackdropPresetsJson(const QString &json) { store()->setValue(kBackdropPresets, json); }

QString Settings::backdropDefaultName() { return store()->value(kBackdropDefault).toString(); }
void Settings::setBackdropDefaultName(const QString &name) { store()->setValue(kBackdropDefault, name); }

} // namespace Core
