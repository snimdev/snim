#ifndef EDITOR_VIDEO_GIFENCODER_H
#define EDITOR_VIDEO_GIFENCODER_H

#include "editor/video/AnimationEncoder.h"

#include <memory>

namespace Editor::Video {

/**
 * The animated-GIF strategy: libimagequant picks a palette per frame and dithers the
 * frame onto it, and giflib streams the result into the file. The only place in the
 * app that touches either library.
 *
 * Every frame carries its own local palette, since one global palette would need a
 * second pass over frames the grabber only delivers once. GIF delays are whole
 * centiseconds, so each frame is held back until the next one fixes its end, and the
 * rounding error carries forward instead of piling up.
 */
class GifEncoder final : public AnimationEncoder
{
public:
    GifEncoder();
    ~GifEncoder() override;

protected:
    bool onBegin(const QSize &size, const AnimationParams &params, QString *errorOut) override;
    bool onFrame(const QImage &rgba, qint64 ms, QString *errorOut) override;
    bool onFinish(qint64 endMs, QString *errorOut) override;
    void onCancel() override;

private:
    bool writeHeldFrame(int delayCs, QString *errorOut);
    bool gifFailed(QString *errorOut, const QString &step);

    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_GIFENCODER_H
