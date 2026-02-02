#ifndef EDITOR_VIDEO_MEDIAFRAMEGRABBER_H
#define EDITOR_VIDEO_MEDIAFRAMEGRABBER_H

#include "editor/video/VideoFrameGrabber.h"
#include <memory>

class QVideoFrame;

namespace Editor::Video {

/**
 * VideoFrameGrabber on QMediaPlayer + QVideoSink. The backend only produces frames
 * during playback (a paused seek delivers none at all, measured on Qt 6.11's FFmpeg
 * backend), so the clip is played from the trim start at a raised rate and each
 * delivered frame serves every planned sample whose time it has reached.
 *
 * Qt Multimedia is a hard dependency, and its backend (FFmpeg in Qt 6.8+, so the same
 * decoder on every platform) is what makes one implementation enough.
 */
class MediaFrameGrabber : public VideoFrameGrabber
{
    Q_OBJECT

public:
    explicit MediaFrameGrabber(QObject *parent = nullptr);
    ~MediaFrameGrabber() override;

    void start(const QString &input, qint64 inMs, qint64 outMs,
               int fps, int maxWidth) override;
    void cancel() override;
    void pause() override;
    void resume() override;

private:
    void onFrame(const QVideoFrame &frame);
    void onMediaLoaded();
    void onEndOfMedia();
    void finish();
    void fail(const QString &error);

    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_MEDIAFRAMEGRABBER_H
