#ifndef EDITOR_VIDEO_WEBPENCODER_H
#define EDITOR_VIDEO_WEBPENCODER_H

#include "editor/video/AnimationParams.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <memory>

namespace Editor::Video {

/**
 * Streaming animated-WebP writer: frames go in one at a time and the whole file is
 * written on finish(). The only place in the app that touches libwebp.
 *
 * A one-frame export comes out as a still WebP: libwebp skips the animation chunks
 * entirely for a single frame, which is what a trim shorter than one frame interval
 * produces. That is a valid file, just not a looping one.
 */
class WebpEncoder
{
public:
    WebpEncoder();
    ~WebpEncoder();

    WebpEncoder(const WebpEncoder &) = delete;
    WebpEncoder &operator=(const WebpEncoder &) = delete;

    // Opens `path` for a `size`-pixel canvas and clamps `params`. The file is created
    // here, not in finish(), so a permission problem surfaces before the caller spends
    // minutes decoding frames. Timestamps are SOURCE milliseconds; only their spacing
    // matters, since it becomes each frame's on-screen duration.
    bool begin(const QString &path, const QSize &size, const AnimationParams &params,
               QString *errorOut = nullptr);

    // Appends one frame, shown until the next one. A frame that does not match the
    // canvas is scaled to it. Timestamps that do not advance are nudged forward so a
    // repeated frame holds for at least 1 ms instead of failing the encode.
    bool addFrame(const QImage &frame, qint64 sourceMs, QString *errorOut = nullptr);

    // Closes the last frame's duration and writes the file. A false return leaves no
    // file behind.
    bool finish(QString *errorOut = nullptr);

    // Drops the encoder and the partial file. Safe at any point, including after
    // finish() and from the destructor.
    void cancel();

private:
    bool fail(QString *errorOut, const QString &message);

    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_WEBPENCODER_H
