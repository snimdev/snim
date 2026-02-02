#ifndef EDITOR_VIDEO_VIDEOFRAMEGRABBER_H
#define EDITOR_VIDEO_VIDEOFRAMEGRABBER_H

#include "editor/video/AnimationParams.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <memory>

namespace Editor::Video {

/**
 * Delivers the frames covering [inMs, outMs] of `input`, one per planned sample,
 * downscaled to maxWidth. The Qt Multimedia implementation is the only one, so a single
 * implementation covers macOS and Linux.
 *
 * The seam exists because the frame source is the untestable part: the exporter's tests
 * drive synthetic frames through a fake instead of decoding a real file.
 */
class VideoFrameGrabber : public QObject
{
    Q_OBJECT

public:
    explicit VideoFrameGrabber(QObject *parent = nullptr) : QObject(parent) {}
    ~VideoFrameGrabber() override = default;

    // Asynchronously deliver the frames for [inMs, outMs] at `fps`. Connect first:
    // an unusable input fails immediately. Exactly one of finished()/failed() fires
    // later, on this object's thread.
    virtual void start(const QString &input, qint64 inMs, qint64 outMs,
                       int fps, int maxWidth) = 0;

    // Stops delivery without emitting finished()/failed(). Safe at any point.
    virtual void cancel() = 0;

    // Suspends and resumes delivery of an in-flight run, so a slow consumer can apply
    // backpressure instead of queueing frames. Both are no-ops unless they change the
    // state, so redundant calls are safe.
    virtual void pause() = 0;
    virtual void resume() = 0;

    static std::unique_ptr<VideoFrameGrabber> create(QObject *parent = nullptr);

signals:
    // `sourceMs` is the planned sample time on the SOURCE timeline, which is what the
    // encoder turns into frame durations. Frames may repeat if the decoder drops some.
    void frameReady(const QImage &frame, qint64 sourceMs);
    void finished();
    void failed(const QString &error);
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_VIDEOFRAMEGRABBER_H
