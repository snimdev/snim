#include "editor/video/WebpEncoder.h"

#include <webp/encode.h>
#include <webp/mux.h>

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

struct WebpEncoder::Impl {
    WebPAnimEncoder *enc = nullptr;
    // QSaveFile so a half-written animation can never land: its destructor discards the
    // temp unless commit() runs, which only happens on a successful finish().
    QSaveFile file;
    QString path;
    QSize size;
    int fps = 10;
    int quality = 75;
    bool lossless = false;
    int effort = 4;        // libwebp method, cpu spent per frame
    qint64 firstMs = 0;    // frame 0 is rebased to 0 on the output timeline
    qint64 prevMs = 0;     // last accepted timestamp, already rebased
    bool hasPrev = false;

    // Freeing the encoder here is what lets cancel() be a plain state reset, and covers
    // the destructor path even if the caller never cancels.
    ~Impl()
    {
        if (enc)
            WebPAnimEncoderDelete(enc);
    }
};

WebpEncoder::WebpEncoder() : d(std::make_unique<Impl>()) {}

WebpEncoder::~WebpEncoder() = default;

bool WebpEncoder::fail(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
    // Every failure is terminal for this encode, so the encoder and any partial file
    // go away here rather than leaving the caller to remember.
    cancel();
    return false;
}

bool WebpEncoder::begin(const QString &path, const QSize &size,
                        const AnimationParams &params, QString *errorOut)
{
    cancel();

    if (path.isEmpty() || size.isEmpty())
        return fail(errorOut, QStringLiteral("The WebP canvas is empty."));

    const AnimationParams p = params.clamped();

    WebPAnimEncoderOptions options;
    if (!WebPAnimEncoderOptionsInit(&options))
        return fail(errorOut, QStringLiteral("libwebp is too old to encode animations."));
    options.minimize_size = p.minimizeSize ? 1 : 0;
    options.anim_params.loop_count = p.loopCount;

    d->enc = WebPAnimEncoderNew(size.width(), size.height(), &options);
    if (!d->enc) {
        return fail(errorOut, QStringLiteral("Could not start the WebP encoder for %1x%2.")
                                 .arg(size.width()).arg(size.height()));
    }

    d->file.setFileName(path);
    if (!d->file.open(QIODevice::WriteOnly)) {
        return fail(errorOut, QStringLiteral("Cannot write %1: %2")
                                 .arg(path, d->file.errorString()));
    }

    d->path = path;
    d->size = size;
    d->fps = p.fps;
    d->quality = p.quality;
    d->lossless = p.lossless;
    d->effort = p.effort;
    d->firstMs = 0;
    d->prevMs = 0;
    d->hasPrev = false;
    return true;
}

bool WebpEncoder::addFrame(const QImage &frame, qint64 sourceMs, QString *errorOut)
{
    if (!d->enc)
        return fail(errorOut, QStringLiteral("addFrame() called before begin()."));
    if (frame.isNull())
        return fail(errorOut, QStringLiteral("The frame is empty."));

    QImage rgba = frame.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.size() != d->size)   // the caller is expected to have scaled already
        rgba = rgba.scaled(d->size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Frame 0 starts the output timeline at 0: durations are deltas between
    // timestamps, so an absolute source offset would only be noise. A timestamp that
    // does not advance (a repeated source frame) still holds for 1 ms.
    if (!d->hasPrev)
        d->firstMs = sourceMs;
    qint64 ms = sourceMs - d->firstMs;
    if (d->hasPrev && ms <= d->prevMs)
        ms = d->prevMs + 1;

    WebPPicture picture;
    if (!WebPPictureInit(&picture))
        return fail(errorOut, QStringLiteral("libwebp is too old for this build."));
    picture.use_argb = 1;
    picture.width = rgba.width();
    picture.height = rgba.height();
    if (!WebPPictureImportRGBA(&picture, rgba.constBits(), rgba.bytesPerLine())) {
        WebPPictureFree(&picture);
        return fail(errorOut, QStringLiteral("libwebp could not read a %1x%2 frame.")
                                 .arg(rgba.width()).arg(rgba.height()));
    }

    WebPConfig config;
    if (!WebPConfigInit(&config)) {
        WebPPictureFree(&picture);
        return fail(errorOut, QStringLiteral("libwebp is too old for this build."));
    }
    config.lossless = d->lossless ? 1 : 0;
    config.quality = float(d->quality);   // 0-100; lossless reads it as effort
    config.method = d->effort;            // 0-6, clamped in AnimationParams

    const int timestampMs = int(ms);
    const bool ok = WebPAnimEncoderAdd(d->enc, &picture, timestampMs, &config) != 0;
    const char *why = ok ? nullptr : WebPAnimEncoderGetError(d->enc);
    WebPPictureFree(&picture);
    if (!ok) {
        return fail(errorOut, QStringLiteral("libwebp rejected the frame at %1 ms: %2")
                                 .arg(timestampMs)
                                 .arg(QString::fromLatin1(why ? why : "unknown")));
    }

    d->prevMs = ms;
    d->hasPrev = true;
    return true;
}

bool WebpEncoder::finish(QString *errorOut)
{
    if (!d->enc)
        return fail(errorOut, QStringLiteral("finish() called before begin()."));
    if (!d->hasPrev)
        return fail(errorOut, QStringLiteral("No frames were added."));

    // A NULL picture only moves the end timestamp; without it the last frame's
    // duration is unknown and libwebp drops that frame from the output.
    const int endMs = int(d->prevMs + frameDurationMs(d->fps));
    if (!WebPAnimEncoderAdd(d->enc, nullptr, endMs, nullptr)) {
        const char *why = WebPAnimEncoderGetError(d->enc);
        return fail(errorOut, QStringLiteral("libwebp rejected the end of the animation: %1")
                                 .arg(QString::fromLatin1(why ? why : "unknown")));
    }

    WebPData data;
    WebPDataInit(&data);
    if (!WebPAnimEncoderAssemble(d->enc, &data) || data.bytes == nullptr) {
        const char *why = WebPAnimEncoderGetError(d->enc);
        WebPDataClear(&data);
        return fail(errorOut, QStringLiteral("Could not assemble the WebP animation: %1")
                                 .arg(QString::fromLatin1(why ? why : "unknown")));
    }
    const qint64 total = qint64(data.size);
    const qint64 written = d->file.write(reinterpret_cast<const char *>(data.bytes), total);
    WebPDataClear(&data);
    if (written != total) {
        return fail(errorOut, QStringLiteral("Cannot write %1: %2")
                                 .arg(d->path, d->file.errorString()));
    }
    if (!d->file.commit()) {
        return fail(errorOut, QStringLiteral("Cannot write %1: %2")
                                 .arg(d->path, d->file.errorString()));
    }

    cancel();   // the file is on disk; release the encoder
    return true;
}

void WebpEncoder::cancel()
{
    // Replacing the state wholesale is the simplest way to guarantee no stale encoder and
    // no half-open QSaveFile: the old Impl's destructor frees both, and the next begin()
    // starts clean. The editor relies on that reuse across exports.
    d = std::make_unique<Impl>();
}

} // namespace Editor::Video
