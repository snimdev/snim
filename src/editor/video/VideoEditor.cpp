#include "editor/video/VideoEditor.h"
#include "editor/EditorChrome.h"
#include "editor/video/TrimTimeline.h"
#include "editor/video/VideoExporter.h"
#include "editor/video/WebpExporter.h"
#include "editor/video/AnimationParams.h"
#include "editor/video/AnimationOptionsDialog.h"
#include "editor/video/Timecode.h"
#include "upload/UploaderFactory.h"
#include "upload/UploadConfig.h"
#include "upload/UploadMenu.h"
#include <QMenu>
#include "core/IconUtil.h"
#include "core/Settings.h"

#include <QApplication>
#include <QAudioOutput>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMediaMetaData>
#include <QMessageBox>
#include <QMimeData>
#include <QStandardPaths>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>
#include <QWidget>

#include <algorithm>

namespace Editor::Video {

namespace {
// The recording's name with the uploaded file's container (the untrimmed temp may be .mov).
QString uploadNameFor(const QString &sourceName, const QString &path)
{
    const QString base = QFileInfo(sourceName).completeBaseName();
    const QString suffix = QFileInfo(path).suffix();
    return (base.isEmpty() ? QStringLiteral("recording") : base) + QStringLiteral(".")
           + (suffix.isEmpty() ? QStringLiteral("mp4") : suffix);
}

// Everything the animation export path reads off the chosen format.
struct AnimationFormatInfo {
    QString extension;     // also the saved file's and the temp file's suffix
    QString dialogTitle;
    QString nameFilter;
    QString tempPrefix;
    QString label;         // the format's name in status and error text
};

// A function, not a static table: tr() must not run before QApplication exists.
AnimationFormatInfo animationFormatInfo(AnimationFormat format)
{
    if (format == AnimationFormat::WebP)
        return {QStringLiteral("webp"), VideoEditor::tr("Export WebP"),
                VideoEditor::tr("WebP (*.webp)"), QStringLiteral("Snim_webp_"),
                VideoEditor::tr("WebP")};
    return {QStringLiteral("gif"), VideoEditor::tr("Export GIF"),
            VideoEditor::tr("GIF (*.gif)"), QStringLiteral("Snim_gif_"),
            VideoEditor::tr("GIF")};
}

AnimationParams loadAnimationParams(AnimationFormat format)
{
    const QString key = animationFormatInfo(format).extension;
    AnimationParams p;
    p.fps = Core::Settings::animationFps(key);
    p.maxWidth = Core::Settings::animationMaxWidth(key);
    p.quality = Core::Settings::animationQuality(key);
    p.lossless = Core::Settings::animationLossless(key);
    p.loopCount = Core::Settings::animationLoopCount(key);
    return p.clamped();
}

void saveAnimationParams(AnimationFormat format, const AnimationParams &p)
{
    const QString key = animationFormatInfo(format).extension;
    Core::Settings::setAnimationFps(key, p.fps);
    Core::Settings::setAnimationMaxWidth(key, p.maxWidth);
    Core::Settings::setAnimationQuality(key, p.quality);
    Core::Settings::setAnimationLossless(key, p.lossless);
    Core::Settings::setAnimationLoopCount(key, p.loopCount);
}
} // namespace

VideoEditor::VideoEditor(const QString &tempPath, QWidget *parent)
    : QMainWindow(parent), m_tempPath(tempPath)
{
    setWindowTitle(tr("Snim - Recording"));
    resize(900, 620);
    setupUi();
    if (!exporter()->supportsGif()) {
        m_gifAction->setEnabled(false);
        m_gifAction->setToolTip(tr("GIF export is not available on this platform yet"));
    }

    // Player wiring. The editor stays usable even if decoding fails: trimming is
    // disabled, but Save (a plain file move) keeps working.
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoWidget);

    connect(m_player, &QMediaPlayer::durationChanged, this, &VideoEditor::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &VideoEditor::onPositionChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &VideoEditor::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &) { previewFailed(); });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState) { updatePlayPauseIcon(); });

    connect(m_timeline, &TrimTimeline::scrubbed, this,
            [this](qint64 ms) { m_player->setPosition(ms); });
    connect(m_timeline, &TrimTimeline::inChanged, this, [this](qint64 ms) {
        m_player->setPosition(ms);   // live preview of where the cut will start
        updateTimeLabel();
    });
    connect(m_timeline, &TrimTimeline::outChanged, this, [this](qint64 ms) {
        if (m_player->position() > ms)
            m_player->setPosition(ms);
        updateTimeLabel();
    });

    m_player->setSource(QUrl::fromLocalFile(m_tempPath));
}

VideoEditor::~VideoEditor()
{
    if (m_player)
        m_player->stop();   // release the file before any pending temp deletion
}

void VideoEditor::setupUi()
{
    // Same shell as the image editor: identical icon size, icon-only buttons, and the
    // shared stylesheet, so the two windows read as one app.
    m_toolbar = addToolBar(tr("Recording"));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconSize(QSize(20, 20));
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    Editor::applyEditorStyleSheet(this);

    m_saveAction = m_toolbar->addAction(themedIcon(":/icons/icons/save.svg"), QString());
    m_saveAction->setToolTip(tr("Save As (Ctrl+S)"));
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &VideoEditor::onSave);

    m_copyAction = m_toolbar->addAction(themedIcon(":/icons/icons/copy.svg"), QString());
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_copyAction->setToolTip(tr("Save into the recordings folder and copy the file to the clipboard"));
    connect(m_copyAction, &QAction::triggered, this, &VideoEditor::onCopy);

    m_toolbar->addSeparator();

    m_gifAction = new QAction(themedIcon(":/icons/icons/export-gif.svg"), QString(), this);
    m_gifAction->setToolTip(tr("Export the trimmed range as an animated GIF"));
    connect(m_gifAction, &QAction::triggered, this,
            [this] { exportAnimation(AnimationFormat::Gif); });
    buildAnimationMenu(addSplitButton(m_gifAction), AnimationFormat::Gif);

    m_webpAction = new QAction(themedIcon(":/icons/icons/export-webp.svg"), QString(), this);
    m_webpAction->setToolTip(tr("Export the trimmed range as an animated WebP"));
    connect(m_webpAction, &QAction::triggered, this,
            [this] { exportAnimation(AnimationFormat::WebP); });
    buildAnimationMenu(addSplitButton(m_webpAction), AnimationFormat::WebP);

    // Frame: click = save as PNG; the menu holds the other uses.
    m_frameAction = new QAction(themedIcon(":/icons/icons/frame.svg"), QString(), this);
    m_frameAction->setToolTip(tr("Save the current frame as a PNG"));
    m_frameAction->setEnabled(false);   // until the media loads
    connect(m_frameAction, &QAction::triggered, this, &VideoEditor::onSaveFrame);
    QMenu *frameMenu = addSplitButton(m_frameAction);
    connect(frameMenu->addAction(tr("Save frame as PNG…")), &QAction::triggered,
            this, &VideoEditor::onSaveFrame);
    connect(frameMenu->addAction(tr("Open frame in editor")), &QAction::triggered,
            this, &VideoEditor::onEditFrame);

    // Upload: click = default destination; the menu picks a saved server.
    m_uploadAction = new QAction(themedIcon(":/icons/icons/upload.svg"), QString(), this);
    m_uploadAction->setToolTip(tr("Upload to the default server and copy the link"));
    connect(m_uploadAction, &QAction::triggered, this, [this] { doUpload(QString()); });
    QMenu *uploadMenu = addSplitButton(m_uploadAction);
    connect(uploadMenu, &QMenu::aboutToShow, this, [this, uploadMenu] {
        Upload::rebuildUploadMenu(uploadMenu, [this](const QString &id) { doUpload(id); });
    });

    auto *spacer = new QWidget(m_toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_statusLabel = new QLabel(m_toolbar);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray; padding-right: 8px;"));
    m_toolbar->addWidget(m_statusLabel);

    m_discardAction = m_toolbar->addAction(tr("Discard"));
    connect(m_discardAction, &QAction::triggered, this, &QWidget::close);   // closeEvent confirms

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_videoWidget = new QVideoWidget(central);
    layout->addWidget(m_videoWidget, /*stretch=*/1);

    // Transport row: play/pause, elapsed/total, then the trim strip.
    auto *transport = new QWidget(central);
    auto *transportLayout = new QHBoxLayout(transport);
    transportLayout->setContentsMargins(10, 6, 10, 8);
    transportLayout->setSpacing(10);

    m_playPauseAction = new QAction(themedIcon(":/icons/icons/play.svg"), tr("Play"), this);
    m_playPauseAction->setEnabled(false);   // until the media loads
    connect(m_playPauseAction, &QAction::triggered, this, &VideoEditor::togglePlayPause);
    auto *playButton = new QToolButton(transport);
    playButton->setDefaultAction(m_playPauseAction);
    playButton->setAutoRaise(true);
    playButton->setIconSize(QSize(20, 20));   // match the toolbar glyphs, not the 16pt default
    transportLayout->addWidget(playButton);

    m_timeLabel = new QLabel(QStringLiteral("0:00.0 / 0:00.0"), transport);
    transportLayout->addWidget(m_timeLabel);

    m_keptLabel = new QLabel(transport);
    m_keptLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_keptLabel->hide();   // only while trimmed
    transportLayout->addWidget(m_keptLabel);

    m_timeline = new TrimTimeline(transport);
    transportLayout->addWidget(m_timeline, /*stretch=*/1);

    layout->addWidget(transport);
    setCentralWidget(central);
}

QMenu *VideoEditor::addSplitButton(QAction *defaultAction)
{
    auto *button = new QToolButton(m_toolbar);
    button->setDefaultAction(defaultAction);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    auto *menu = new QMenu(button);
    button->setMenu(menu);
    m_toolbar->addWidget(button);
    return menu;
}

void VideoEditor::buildAnimationMenu(QMenu *menu, AnimationFormat format)
{
    connect(menu->addAction(tr("Copy")), &QAction::triggered, this,
            [this, format] { copyAnimation(format); });
    QMenu *upload = menu->addMenu(tr("Upload"));
    connect(upload, &QMenu::aboutToShow, this, [this, upload, format] {
        Upload::rebuildUploadMenu(upload, [this, format](const QString &id) {
            uploadAnimation(format, id);
        });
    });
    menu->addSeparator();
    connect(menu->addAction(tr("Export options…")), &QAction::triggered, this,
            [this, format] { (void)resolveAnimationParams(format, /*alwaysAsk=*/true); });
}

QIcon VideoEditor::themedIcon(const QString &svgPath) const
{
    // Same tone selection as the image editor's toolbar, rendered at the toolbar's own
    // icon size so the glyph fills its slot exactly (a hardcoded size in a bigger slot
    // reads as a small icon floating in dead space).
    const QColor windowColor = QApplication::palette().color(QPalette::Window);
    const bool isDarkMode = windowColor.lightness() < 128;
    const int size = m_toolbar ? m_toolbar->iconSize().width() : 20;
    return Core::themedSvgIcon(svgPath, QColor(isDarkMode ? "#d0d0d0" : "#333333"), size);
}

// ---- playback ------------------------------------------------------------------

void VideoEditor::onDurationChanged(qint64 durationMs)
{
    if (durationMs <= 0)
        return;
    m_timeline->setDurationMs(durationMs);
    updateTimeLabel();
}

void VideoEditor::onPositionChanged(qint64 positionMs)
{
    m_timeline->setPlayheadMs(positionMs);
    updateTimeLabel();
    // Preview plays the KEPT range: stop at the out point and rewind to the cut start.
    const TrimState &state = m_timeline->state();
    if (m_player->playbackState() == QMediaPlayer::PlayingState
        && state.durationMs() > 0 && positionMs >= state.outMs()) {
        m_player->pause();
        m_player->setPosition(state.inMs());
    }
}

void VideoEditor::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
        m_playPauseAction->setEnabled(true);
        m_frameAction->setEnabled(m_pending == Pending::None);
    } else if (status == QMediaPlayer::InvalidMedia) {
        previewFailed();
    } else if (status == QMediaPlayer::EndOfMedia) {
        m_player->setPosition(m_timeline->state().inMs());
        updatePlayPauseIcon();
    }
}

void VideoEditor::previewFailed()
{
    m_previewOk = false;
    m_playPauseAction->setEnabled(false);
    m_frameAction->setEnabled(false);
    m_timeline->setInteractive(false);   // no duration -> no trimming
    m_statusLabel->setText(tr("Preview unavailable, Save still works"));
}

void VideoEditor::togglePlayPause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        return;
    }
    // Play the kept range from wherever the playhead is (or its start).
    const TrimState &state = m_timeline->state();
    const qint64 pos = m_player->position();
    if (pos < state.inMs() || pos >= state.outMs())
        m_player->setPosition(state.inMs());
    m_player->play();
}

void VideoEditor::updatePlayPauseIcon()
{
    const bool playing = m_player->playbackState() == QMediaPlayer::PlayingState;
    m_playPauseAction->setIcon(themedIcon(playing ? QStringLiteral(":/icons/icons/pause.svg")
                                                  : QStringLiteral(":/icons/icons/play.svg")));
    m_playPauseAction->setText(playing ? tr("Pause") : tr("Play"));
}

void VideoEditor::updateTimeLabel()
{
    const TrimState &state = m_timeline->state();
    m_timeLabel->setText(QStringLiteral("%1 / %2")
                             .arg(formatTimecode(m_player ? m_player->position() : 0),
                                  formatTimecode(state.durationMs())));
    m_keptLabel->setVisible(state.isTrimmed());
    if (state.isTrimmed())
        m_keptLabel->setText(tr("kept %1").arg(formatTimecode(state.trimmedDurationMs())));
}

void VideoEditor::keyPressEvent(QKeyEvent *event)
{
    const TrimState &state = m_timeline->state();
    switch (event->key()) {
    case Qt::Key_Space:
        if (m_playPauseAction->isEnabled())
            togglePlayPause();
        return;
    case Qt::Key_Left:
    case Qt::Key_Right: {
        if (!m_previewOk)
            break;
        const qint64 step = (event->modifiers() & Qt::ShiftModifier) ? 5000 : 1000;
        const qint64 delta = (event->key() == Qt::Key_Left) ? -step : step;
        // The whole clip, not the kept range: I/O must be able to widen the cut.
        m_player->setPosition(std::clamp(m_player->position() + delta,
                                         qint64(0), state.durationMs()));
        return;
    }
    case Qt::Key_Comma:
    case Qt::Key_Period: {
        if (!m_previewOk)
            break;
        double fps = m_player->metaData().value(QMediaMetaData::VideoFrameRate).toReal();
        if (!(fps > 0))
            fps = Core::Settings::recordingFps();
        const qint64 step = frameStepMs(fps);
        const qint64 delta = (event->key() == Qt::Key_Comma) ? -step : step;
        m_player->pause();
        m_player->setPosition(std::clamp(m_player->position() + delta,
                                         qint64(0), state.durationMs()));
        return;
    }
    case Qt::Key_Home:
    case Qt::Key_End:
        if (!m_previewOk)
            break;
        m_player->setPosition(event->key() == Qt::Key_Home ? state.inMs() : state.outMs());
        return;
    case Qt::Key_I:
        m_timeline->setInMs(m_player->position());
        return;
    case Qt::Key_O:
        m_timeline->setOutMs(m_player->position());
        return;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}

// ---- save / copy / discard -------------------------------------------------------

QString VideoEditor::suggestedFileName() const
{
    // The recorder already names the temp after the recording's start time.
    return QFileInfo(m_tempPath).fileName();
}

QString VideoEditor::recordingsDir() const
{
    const QString dir = Core::Settings::recordingFolder();
    QDir().mkpath(dir);
    return dir;
}

bool VideoEditor::moveFileTo(const QString &source, const QString &dest)
{
    QFile::remove(dest);   // overwrite if it already exists
    // rename() is atomic on the same volume; fall back to copy+remove across volumes.
    return QFile::rename(source, dest) || (QFile::copy(source, dest) && QFile::remove(source));
}

void VideoEditor::finishSaved(const QString &finalPath)
{
    m_saved = true;
    emit recordingSaved(finalPath);
    close();   // m_saved suppresses the discard prompt
}

VideoExporter *VideoEditor::exporter()
{
    if (!m_exporter) {
        m_exporter = VideoExporter::create(this);
        connect(m_exporter.get(), &VideoExporter::finished,
                this, &VideoEditor::onExporterFinished);
        connect(m_exporter.get(), &VideoExporter::failed,
                this, &VideoEditor::onExporterFailed);
        connect(m_exporter.get(), &VideoExporter::progress,
                this, &VideoEditor::onExportProgress);
    }
    return m_exporter.get();
}

WebpExporter *VideoEditor::webpExporter()
{
    if (!m_webpExporter) {
        m_webpExporter = std::make_unique<WebpExporter>(this);
        connect(m_webpExporter.get(), &WebpExporter::finished,
                this, &VideoEditor::onExporterFinished);
        connect(m_webpExporter.get(), &WebpExporter::failed,
                this, &VideoEditor::onExporterFailed);
        connect(m_webpExporter.get(), &WebpExporter::progress,
                this, &VideoEditor::onExportProgress);
    }
    return m_webpExporter.get();
}

void VideoEditor::setBusy(bool busy)
{
    m_saveAction->setEnabled(!busy);
    m_copyAction->setEnabled(!busy);
    m_gifAction->setEnabled(!busy && m_previewOk && exporter()->supportsGif());
    m_webpAction->setEnabled(!busy && m_previewOk);   // WebP needs a decodable source
    m_frameAction->setEnabled(!busy && m_previewOk);
    m_uploadAction->setEnabled(!busy);
    m_discardAction->setEnabled(!busy);
    m_playPauseAction->setEnabled(!busy && m_previewOk);
    m_timeline->setInteractive(!busy && m_previewOk);
    m_statusLabel->setText(busy ? tr("Exporting…") : QString());
}

bool VideoEditor::confirmUntrimmedFallback()
{
    const auto answer = QMessageBox::question(
        this, tr("Trimming Unavailable"),
#ifdef Q_OS_LINUX
        tr("Trimming is not available (missing GStreamer plugins). "
           "Use the full recording instead?"));
#else
        tr("Trimming is not supported on this platform. Use the full recording instead?"));
#endif
    return answer == QMessageBox::Yes;
}

void VideoEditor::onSave()
{
    const QString dest = QFileDialog::getSaveFileName(
        this, tr("Save Recording"), recordingsDir() + "/" + suggestedFileName(),
        tr("Video (*.mp4 *.mov)"));
    if (dest.isEmpty())
        return;                                   // cancelled: keep editing

    const TrimState &state = m_timeline->state();
    if (!state.isTrimmed()) {                     // fast path: just move the file
        if (moveFileTo(m_tempPath, dest))
            finishSaved(dest);
        else
            QMessageBox::warning(this, tr("Save Failed"),
                                 tr("Could not save the recording to %1").arg(dest));
        return;
    }

    if (!exporter()->isAvailable()) {
        if (!confirmUntrimmedFallback())
            return;
        if (moveFileTo(m_tempPath, dest))
            finishSaved(dest);
        else
            QMessageBox::warning(this, tr("Save Failed"),
                                 tr("Could not save the recording to %1").arg(dest));
        return;
    }

    m_pending = Pending::SaveMove;
    m_pendingDest = dest;
    m_exportTempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + QStringLiteral("/Snim_trim_")
                       + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))
                       + QStringLiteral(".mp4");
    setBusy(true);
    m_player->pause();
    exporter()->trim(m_tempPath, m_exportTempPath, state.inMs(), state.outMs());
}

void VideoEditor::onCopy()
{
    // Copy needs a path that outlives this window, so the file lands in the
    // recordings folder first; the clipboard then carries a stable file URL.
    const TrimState &state = m_timeline->state();
    const QString dest = recordingsDir() + "/" + suggestedFileName();

    bool trimmed = state.isTrimmed();
    if (trimmed && !exporter()->isAvailable()) {
        if (!confirmUntrimmedFallback())
            return;
        trimmed = false;
    }
    if (!trimmed) {
        if (moveFileTo(m_tempPath, dest)) {
            putOnClipboard(dest);
            finishSaved(dest);
        } else {
            QMessageBox::warning(this, tr("Copy Failed"),
                                 tr("Could not save the recording to %1").arg(dest));
        }
        return;
    }

    m_pending = Pending::Copy;
    m_pendingDest = dest;
    setBusy(true);
    m_player->pause();
    exporter()->trim(m_tempPath, dest, state.inMs(), state.outMs());   // straight to dest
}

bool VideoEditor::animationAvailable(AnimationFormat format)
{
    if (!m_previewOk)
        return false;
    if (format == AnimationFormat::Gif && !exporter()->supportsGif()) {
        QMessageBox::warning(this, tr("GIF Unavailable"),
                             tr("Exporting to GIF is not supported on this platform."));
        return false;
    }
    return true;
}

std::optional<AnimationParams> VideoEditor::resolveAnimationParams(AnimationFormat format,
                                                                   bool alwaysAsk)
{
    const bool skip = Core::Settings::animationOptionsSkip();
    if (skip && !alwaysAsk)
        return loadAnimationParams(format);
    AnimationOptionsDialog options(format, loadAnimationParams(format), this);
    options.setSkipNextTime(skip);
    if (options.exec() != QDialog::Accepted)
        return std::nullopt;
    saveAnimationParams(format, options.params());
    Core::Settings::setAnimationOptionsSkip(options.skipNextTime());
    return options.params();
}

void VideoEditor::exportAnimation(AnimationFormat format)
{
    if (!animationAvailable(format))
        return;
    const std::optional<AnimationParams> params = resolveAnimationParams(format);
    if (!params)
        return;

    const AnimationFormatInfo info = animationFormatInfo(format);
    const QString dest = QFileDialog::getSaveFileName(
        this, info.dialogTitle,
        recordingsDir() + "/" + animationFileNameFor(suggestedFileName(), info.extension),
        info.nameFilter);
    if (dest.isEmpty())
        return;                                   // cancelled: keep editing

    m_pendingDest = dest;
    startAnimation(format, Pending::ExportAnimation, *params);
}

void VideoEditor::copyAnimation(AnimationFormat format)
{
    if (!animationAvailable(format))
        return;
    const std::optional<AnimationParams> params = resolveAnimationParams(format);
    if (!params)
        return;

    // Like Copy: the file lands in the recordings folder so the clipboard URL stays valid.
    m_pendingDest = recordingsDir() + "/"
                    + animationFileNameFor(suggestedFileName(),
                                           animationFormatInfo(format).extension);
    startAnimation(format, Pending::CopyAnimation, *params);
}

void VideoEditor::uploadAnimation(AnimationFormat format, const QString &profileId)
{
    if (!animationAvailable(format))
        return;
    if (!Upload::UploadConfig::forProfile(profileId).isComplete()) {
        QMessageBox::information(this, tr("Upload not configured"),
                                tr("Set up an upload destination in Settings → Upload first."));
        return;
    }
    const std::optional<AnimationParams> params = resolveAnimationParams(format);
    if (!params)
        return;

    m_pendingUploadProfileId = profileId;
    startAnimation(format, Pending::UploadAnimation, *params);
}

void VideoEditor::startAnimation(AnimationFormat format, Pending kind,
                                 const AnimationParams &params)
{
    // Both formats always re-encode (no fast-path move): use the whole clip untrimmed.
    const TrimState &state = m_timeline->state();
    const qint64 in = state.isTrimmed() ? state.inMs() : 0;
    const qint64 out = state.isTrimmed() ? state.outMs() : state.durationMs();

    const AnimationFormatInfo info = animationFormatInfo(format);
    m_animationFormat = format;
    m_pending = kind;
    m_exportTempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + QStringLiteral("/") + info.tempPrefix
                       + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))
                       + QStringLiteral(".") + info.extension;
    setBusy(true);
    m_player->pause();
    if (format == AnimationFormat::WebP)
        webpExporter()->start(m_tempPath, m_exportTempPath, in, out, params);
    else
        exporter()->toGif(m_tempPath, m_exportTempPath, in, out, params);
}

void VideoEditor::doUpload(const QString &profileId)
{
    // Validate the CHOSEN destination (empty id = default), not just the default.
    if (!Upload::UploadConfig::forProfile(profileId).isComplete()) {
        QMessageBox::information(this, tr("Upload not configured"),
                                tr("Set up an upload destination in Settings → Upload first."));
        return;
    }
    const TrimState &state = m_timeline->state();
    bool trimmed = state.isTrimmed();
    if (trimmed && !exporter()->isAvailable()) {
        if (!confirmUntrimmedFallback())
            return;
        trimmed = false;
    }
    if (!trimmed) {
        // Hand the recording temp to the app's uploader; mark saved so closeEvent
        // won't delete it out from under the in-flight PUT (the app owns it now).
        m_saved = true;
        emit uploadRequested(m_tempPath, uploadNameFor(suggestedFileName(), m_tempPath),
                             /*deleteWhenDone=*/true, profileId);
        close();
        return;
    }
    // Trimmed: export to a temp, then upload that (handled in onExporterFinished). Stash
    // the chosen profile across the async export.
    m_pending = Pending::Upload;
    m_pendingUploadProfileId = profileId;
    m_exportTempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + QStringLiteral("/Snim_upload_")
                       + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))
                       + QStringLiteral(".mp4");
    setBusy(true);
    m_player->pause();
    exporter()->trim(m_tempPath, m_exportTempPath, state.inMs(), state.outMs());
}

QImage VideoEditor::grabFrame()
{
    m_player->pause();
    return m_videoWidget->videoSink()->videoFrame().toImage();
}

void VideoEditor::onSaveFrame()
{
    if (!m_previewOk)
        return;
    const QImage frame = grabFrame();
    if (frame.isNull()) {
        QMessageBox::information(this, tr("No Frame"), tr("There is no frame to save yet."));
        return;
    }

    const QString dir = Core::Settings::screenshotFolder();
    QDir().mkpath(dir);
    const QString dest = QFileDialog::getSaveFileName(
        this, tr("Save Frame"),
        dir + "/" + frameFileNameFor(suggestedFileName(), m_player->position()),
        tr("PNG (*.png)"));
    if (dest.isEmpty())
        return;
    if (!frame.save(dest, "PNG"))
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not save the frame to %1").arg(dest));
}

void VideoEditor::onEditFrame()
{
    if (!m_previewOk)
        return;
    const QImage frame = grabFrame();
    if (frame.isNull()) {
        QMessageBox::information(this, tr("No Frame"), tr("There is no frame to edit yet."));
        return;
    }
    emit frameEditRequested(QPixmap::fromImage(frame));
}

void VideoEditor::putOnClipboard(const QString &path)
{
    auto *mime = new QMimeData();   // clipboard takes ownership
    mime->setUrls({QUrl::fromLocalFile(path)});
    QApplication::clipboard()->setMimeData(mime);
}

void VideoEditor::onExporterFinished(const QString &exportedPath)
{
    setBusy(false);
    const Pending pending = m_pending;
    m_pending = Pending::None;

    if (pending == Pending::SaveMove) {
        if (moveFileTo(exportedPath, m_pendingDest)) {
            QFile::remove(m_tempPath);            // the original is no longer needed
            m_exportTempPath.clear();
            finishSaved(m_pendingDest);
        } else {
            QFile::remove(exportedPath);          // keep only the original temp
            m_exportTempPath.clear();
            QMessageBox::warning(this, tr("Save Failed"),
                                 tr("Could not save the recording to %1").arg(m_pendingDest));
        }
    } else if (pending == Pending::Copy) {
        putOnClipboard(exportedPath);             // already at its final destination
        QFile::remove(m_tempPath);
        finishSaved(exportedPath);
    } else if (pending == Pending::Upload) {
        // The trimmed temp is ready: hand it to the app's uploader (which deletes it
        // when done), drop the original recording, and close.
        QFile::remove(m_tempPath);
        m_exportTempPath.clear();
        m_saved = true;
        emit uploadRequested(exportedPath, uploadNameFor(suggestedFileName(), exportedPath),
                             /*deleteWhenDone=*/true, m_pendingUploadProfileId);
        close();
    } else if (pending == Pending::CopyAnimation) {
        const QString label = animationFormatInfo(m_animationFormat).label;
        if (moveFileTo(exportedPath, m_pendingDest)) {
            putOnClipboard(m_pendingDest);
            QFile::remove(m_tempPath);
            m_exportTempPath.clear();
            finishSaved(m_pendingDest);
        } else {
            QFile::remove(exportedPath);
            m_exportTempPath.clear();
            QMessageBox::warning(this, tr("Copy Failed"),
                                 tr("Could not save the %1 to %2").arg(label, m_pendingDest));
        }
    } else if (pending == Pending::UploadAnimation) {
        // The app's uploader owns the encoded temp from here and deletes it when done.
        QFile::remove(m_tempPath);
        m_exportTempPath.clear();
        m_saved = true;
        emit uploadRequested(exportedPath, uploadNameFor(suggestedFileName(), exportedPath),
                             /*deleteWhenDone=*/true, m_pendingUploadProfileId);
        close();
    } else if (pending == Pending::ExportAnimation) {
        // Mirror SaveMove exactly: move the temp to the destination, drop the source MP4
        // temp, clear m_exportTempPath on BOTH the success and failure branches so a later
        // close()/discard never re-removes a stale path.
        const QString label = animationFormatInfo(m_animationFormat).label;
        if (moveFileTo(exportedPath, m_pendingDest)) {
            QFile::remove(m_tempPath);
            m_exportTempPath.clear();
            finishSaved(m_pendingDest);
        } else {
            QFile::remove(exportedPath);
            m_exportTempPath.clear();
            QMessageBox::warning(this, tr("Export Failed"),
                                 tr("Could not save the %1 to %2").arg(label, m_pendingDest));
        }
    }
}

void VideoEditor::onExporterFailed(const QString &error)
{
    setBusy(false);
    const Pending pending = m_pending;
    m_pending = Pending::None;
    if (!m_exportTempPath.isEmpty()) {            // belt & suspenders; the .mm cleans too
        QFile::remove(m_exportTempPath);
        m_exportTempPath.clear();
    }
    // A failure with nothing pending means the export was cancelled (e.g. the window is
    // closing): clean up silently, no error dialog.
    if (pending == Pending::None)
        return;
    // Keep the original temp so the user can retry, save untrimmed, or discard.
    QMessageBox::warning(this, tr("Export Failed"), error);
}

void VideoEditor::onExportProgress(int done, int total)
{
    const bool animation = m_pending == Pending::ExportAnimation
                           || m_pending == Pending::CopyAnimation
                           || m_pending == Pending::UploadAnimation;
    if (!animation || total <= 0)
        return;
    m_statusLabel->setText(tr("Encoding %1… %2/%3")
                               .arg(animationFormatInfo(m_animationFormat).label)
                               .arg(done).arg(total));
}

void VideoEditor::closeEvent(QCloseEvent *event)
{
    if (m_saved) {
        event->accept();
        return;
    }
    // Cancel any in-flight export AND drop its pending action BEFORE the prompt: the
    // QMessageBox below spins a nested event loop that can deliver the export's queued
    // completion, which would otherwise run its move/upload (or pop an error) before the
    // user has answered. With m_pending cleared, a late finished/failed callback no-ops.
    if (m_pending != Pending::None) {
        if (m_exporter)
            m_exporter->cancel();
        if (m_webpExporter)
            m_webpExporter->cancel();
        m_pending = Pending::None;
    }

    const auto answer = QMessageBox::question(
        this, tr("Discard Recording"), tr("Discard this recording?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        setBusy(false);          // the export (if any) was cancelled; return to editing
        event->ignore();
        return;
    }
    if (m_player)
        m_player->stop();
    QFile::remove(m_tempPath);
    if (!m_exportTempPath.isEmpty())
        QFile::remove(m_exportTempPath);
    event->accept();
}

} // namespace Editor::Video
