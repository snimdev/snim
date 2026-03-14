#ifndef EDITOR_VIDEO_WEBPENCODER_H
#define EDITOR_VIDEO_WEBPENCODER_H

#include "editor/video/AnimationEncoder.h"

#include <memory>

namespace Editor::Video {

/**
 * The animated-WebP strategy: frames are handed to libwebp as they arrive and the whole
 * file is assembled on finish(). The only place in the app that touches libwebp.
 *
 * A one-frame export comes out as a still WebP: libwebp skips the animation chunks
 * entirely for a single frame, which is what a trim shorter than one frame interval
 * produces. That is a valid file, just not a looping one.
 */
class WebpEncoder final : public AnimationEncoder
{
public:
    WebpEncoder();
    ~WebpEncoder() override;

protected:
    bool onBegin(const QSize &size, const AnimationParams &params, QString *errorOut) override;
    bool onFrame(const QImage &rgba, qint64 ms, QString *errorOut) override;
    bool onFinish(qint64 endMs, QString *errorOut) override;
    void onCancel() override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_WEBPENCODER_H
