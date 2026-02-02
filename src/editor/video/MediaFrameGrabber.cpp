#include "editor/video/MediaFrameGrabber.h"

#include <QFileInfo>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

namespace Editor::Video {

namespace {
// Frames only arrive during playback, so the clip is played faster than real time to
// keep the export short: a minute of recording is walked in about 15 s at 4x, bounded
// by how fast the decoder can actually produce frames.
constexpr double kPlaybackRate = 4.0;

// No frames for this long means the backend is not decoding. Fail rather than leave the
// editor stuck in its busy state, which is the failure mode that has no user recovery.
constexpr int kIdleTimeoutMs = 10000;

// Test seam: the suite shortens the watchdog so a 3 s fixture can trip it.
int idleTimeoutMs()
{
    const int fromEnv = qEnvironmentVariableIntValue("SNIM_GRABBER_IDLE_TIMEOUT_MS");
    return fromEnv > 0 ? fromEnv : kIdleTimeoutMs;
}
} // namespace

struct MediaFrameGrabber::Impl {
    QMediaPlayer player;
    QVideoSink sink;
    QTimer idle;
    QVector<qint64> plan;   // planned sample times, source ms
    int next = 0;           // index of the first sample not yet delivered
    qint64 pendingSeekMs = -1;   // in-point waiting for the media to load
    int maxWidth = 0;
    QSize outputSize;       // fixed by the first delivered frame
    bool running = false;
    bool paused = false;
};

MediaFrameGrabber::MediaFrameGrabber(QObject *parent)
    : VideoFrameGrabber(parent), d(std::make_unique<Impl>())
{
    // No audio output is attached, so the playback is silent by construction.
    d->player.setVideoOutput(&d->sink);
    d->idle.setSingleShot(true);

    connect(&d->idle, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("The video backend stopped delivering frames."));
    });
    connect(&d->sink, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) { onFrame(frame); });
    connect(&d->player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia)
                    onMediaLoaded();
                else if (status == QMediaPlayer::EndOfMedia)
                    onEndOfMedia();
            });
    connect(&d->player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                fail(message.isEmpty() ? QStringLiteral("The recording could not be decoded.")
                                       : message);
            });
}

MediaFrameGrabber::~MediaFrameGrabber()
{
    // Stop before Impl is destroyed: the sink's connection outlives this body and would
    // otherwise reach into freed state if a frame arrived mid-teardown.
    cancel();
}

void MediaFrameGrabber::start(const QString &input, qint64 inMs, qint64 outMs,
                              int fps, int maxWidth)
{
    cancel();
    // Set before validating, so fail() below can actually report the problem.
    d->running = true;

    if (input.isEmpty() || !QFileInfo::exists(input)) {
        fail(QStringLiteral("The recording is gone: %1").arg(input));
        return;
    }

    d->plan = planAnimationFrames(inMs, outMs, fps);
    d->next = 0;
    d->maxWidth = maxWidth;
    d->outputSize = QSize();

    d->pendingSeekMs = inMs;
    d->player.setPlaybackRate(kPlaybackRate);
    // Started here rather than on load, so a file that never loads still times out.
    d->idle.start(idleTimeoutMs());
    d->player.setSource(QUrl::fromLocalFile(input));
    // A repeat run on the same file leaves it loaded, so no status change would come.
    if (d->player.mediaStatus() == QMediaPlayer::LoadedMedia)
        onMediaLoaded();
}

void MediaFrameGrabber::onMediaLoaded()
{
    if (!d->running || d->pendingSeekMs < 0)
        return;
    // setSource() is asynchronous, so this is the first point where a seek sticks.
    const qint64 at = d->pendingSeekMs;
    d->pendingSeekMs = -1;
    d->player.setPosition(at);   // start at the trim, not at the file's start
    d->player.play();
}

void MediaFrameGrabber::onFrame(const QVideoFrame &frame)
{
    if (!d->running || !frame.isValid())
        return;

    // Presentation time, falling back for a backend that does not report one.
    qint64 sourceUs = frame.startTime();
    if (sourceUs < 0)
        sourceUs = frame.endTime();
    if (sourceUs < 0)
        sourceUs = d->player.position() * 1000;
    const qint64 sourceMs = sourceUs / 1000;
    if (sourceMs < 0)
        return;

    // Any decoded frame proves the backend is alive, even one before the first sample.
    // Not while paused: an in-flight frame must not rearm a watchdog we just stopped.
    if (!d->paused)
        d->idle.start(idleTimeoutMs());

    QImage image;
    while (d->next < d->plan.size() && d->plan.at(d->next) <= sourceMs) {
        if (image.isNull()) {
            image = frame.toImage();
            if (image.isNull())
                return;   // unusable frame: leave the sample pending for the next one
            d->outputSize = animationScaledSize(image.size(), d->maxWidth);
            if (!d->outputSize.isEmpty() && d->outputSize != image.size()) {
                image = image.scaled(d->outputSize, Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);
            }
        }
        const qint64 at = d->plan.at(d->next);
        ++d->next;
        emit frameReady(image, at);
        if (!d->running)
            return;   // the slot may cancel us re-entrantly
    }

    if (d->next >= d->plan.size())
        finish();
}

void MediaFrameGrabber::onEndOfMedia()
{
    if (!d->running)
        return;
    // The file ended before the plan was satisfied (a short file, or a decoder that gave
    // up early). Deliver what was collected rather than failing the whole export.
    finish();
}

void MediaFrameGrabber::finish()
{
    const bool delivered = d->next > 0;
    cancel();
    if (delivered)
        emit finished();
    else
        emit failed(QStringLiteral("No frames could be read from this recording."));
}

void MediaFrameGrabber::fail(const QString &error)
{
    if (!d->running)
        return;
    cancel();
    emit failed(error);
}

void MediaFrameGrabber::pause()
{
    if (!d->running || d->paused)
        return;
    d->paused = true;
    // The watchdog stops with the decoder: a pause is not the backend giving up.
    d->idle.stop();
    d->player.pause();
}

void MediaFrameGrabber::resume()
{
    if (!d->running || !d->paused)
        return;
    d->paused = false;
    d->player.play();
    d->idle.start(idleTimeoutMs());
}

void MediaFrameGrabber::cancel()
{
    d->running = false;
    d->paused = false;
    d->pendingSeekMs = -1;
    d->idle.stop();
    d->player.stop();
    d->plan.clear();
    d->next = 0;
    d->outputSize = QSize();
}

std::unique_ptr<VideoFrameGrabber> VideoFrameGrabber::create(QObject *parent)
{
    return std::make_unique<MediaFrameGrabber>(parent);
}

} // namespace Editor::Video
