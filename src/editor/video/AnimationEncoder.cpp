#include "editor/video/AnimationEncoder.h"

#include "editor/video/WebpEncoder.h"

#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace Editor::Video {

namespace {
// Frame duration in ms from the clamped fps, matching planAnimationFrames' spacing.
int frameDurationMs(int fps)
{
    return int(std::lround(1000.0 / std::max(1, fps)));
}
} // namespace

AnimationEncoder::AnimationEncoder() = default;

// A subclass has already released its own state by now, so this only drops the file.
AnimationEncoder::~AnimationEncoder() = default;

std::unique_ptr<AnimationEncoder> AnimationEncoder::create(AnimationFormat format)
{
    switch (format) {
    case AnimationFormat::WebP:
        return std::make_unique<WebpEncoder>();
    case AnimationFormat::Gif:
        break;
    }
    return nullptr;
}

QIODevice *AnimationEncoder::output() const
{
    return m_file.get();
}

QString AnimationEncoder::writeError() const
{
    return QStringLiteral("Cannot write %1: %2")
        .arg(m_path, m_file ? m_file->errorString() : QString());
}

bool AnimationEncoder::fail(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
    // Every failure is terminal for this encode, so the encoder and any partial file
    // go away here rather than leaving the caller to remember.
    cancel();
    return false;
}

bool AnimationEncoder::begin(const QString &path, const QSize &size,
                             const AnimationParams &params, QString *errorOut)
{
    cancel();

    if (path.isEmpty() || size.isEmpty())
        return fail(errorOut, QStringLiteral("The animation canvas is empty."));

    const AnimationParams p = params.clamped();

    m_file = std::make_unique<QSaveFile>(path);
    m_path = path;
    if (!m_file->open(QIODevice::WriteOnly))
        return fail(errorOut, writeError());

    QString error;
    if (!onBegin(size, p, &error))
        return fail(errorOut, error);

    m_size = size;
    m_fps = p.fps;
    m_firstMs = 0;
    m_prevMs = 0;
    m_hasPrev = false;
    return true;
}

bool AnimationEncoder::addFrame(const QImage &frame, qint64 sourceMs, QString *errorOut)
{
    if (!m_file)
        return fail(errorOut, QStringLiteral("addFrame() called before begin()."));
    if (frame.isNull())
        return fail(errorOut, QStringLiteral("The frame is empty."));

    QImage rgba = frame.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.size() != m_size)   // the caller is expected to have scaled already
        rgba = rgba.scaled(m_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Frame 0 starts the output timeline at 0: durations are deltas between
    // timestamps, so an absolute source offset would only be noise. A timestamp that
    // does not advance (a repeated source frame) still holds for 1 ms.
    if (!m_hasPrev)
        m_firstMs = sourceMs;
    qint64 ms = sourceMs - m_firstMs;
    if (m_hasPrev && ms <= m_prevMs)
        ms = m_prevMs + 1;

    QString error;
    if (!onFrame(rgba, ms, &error))
        return fail(errorOut, error);

    m_prevMs = ms;
    m_hasPrev = true;
    return true;
}

bool AnimationEncoder::finish(QString *errorOut)
{
    if (!m_file)
        return fail(errorOut, QStringLiteral("finish() called before begin()."));
    if (!m_hasPrev)
        return fail(errorOut, QStringLiteral("No frames were added."));

    // The last frame has no successor to end it, so it gets one frame interval.
    QString error;
    if (!onFinish(m_prevMs + frameDurationMs(m_fps), &error))
        return fail(errorOut, error);
    if (!m_file->commit())
        return fail(errorOut, writeError());

    cancel();   // the file is on disk; release the encoder
    return true;
}

void AnimationEncoder::cancel()
{
    // The format goes first: it may still write to the file while closing its encoder.
    // Dropping an uncommitted QSaveFile then removes the temp, and the next begin()
    // starts clean. The editor relies on that reuse across exports.
    onCancel();
    m_file.reset();
    m_path.clear();
    m_size = QSize();
    m_fps = 10;
    m_firstMs = 0;
    m_prevMs = 0;
    m_hasPrev = false;
}

} // namespace Editor::Video
