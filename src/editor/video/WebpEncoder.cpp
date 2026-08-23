#include "editor/video/WebpEncoder.h"

#include <webp/encode.h>
#include <webp/mux.h>

#include <QIODevice>

namespace Editor::Video {

struct WebpEncoder::Impl {
    WebPAnimEncoder *enc = nullptr;
    WebPConfig config{};   // the same for every frame, so built once in onBegin()

    // Freeing the encoder here is what lets onCancel() be a plain state reset, and covers
    // the destructor path even if the caller never cancels.
    ~Impl()
    {
        if (enc)
            WebPAnimEncoderDelete(enc);
    }
};

WebpEncoder::WebpEncoder() : d(std::make_unique<Impl>()) {}

WebpEncoder::~WebpEncoder() = default;

bool WebpEncoder::onBegin(const QSize &size, const AnimationParams &params, QString *errorOut)
{
    WebPAnimEncoderOptions options;
    if (!WebPAnimEncoderOptionsInit(&options) || !WebPConfigInit(&d->config)) {
        *errorOut = QStringLiteral("libwebp is too old to encode animations.");
        return false;
    }
    options.minimize_size = params.minimizeSize ? 1 : 0;
    options.anim_params.loop_count = params.loopCount;
    d->config.lossless = params.lossless ? 1 : 0;
    d->config.quality = float(params.quality);   // 0-100; lossless reads it as effort
    d->config.method = params.effort;            // 0-6, clamped in AnimationParams

    d->enc = WebPAnimEncoderNew(size.width(), size.height(), &options);
    if (!d->enc) {
        *errorOut = QStringLiteral("Could not start the WebP encoder for %1x%2.")
                        .arg(size.width()).arg(size.height());
        return false;
    }
    return true;
}

bool WebpEncoder::onFrame(const QImage &rgba, qint64 ms, QString *errorOut)
{
    WebPPicture picture;
    if (!WebPPictureInit(&picture)) {
        *errorOut = QStringLiteral("libwebp is too old for this build.");
        return false;
    }
    picture.use_argb = 1;
    picture.width = rgba.width();
    picture.height = rgba.height();
    if (!WebPPictureImportRGBA(&picture, rgba.constBits(), rgba.bytesPerLine())) {
        WebPPictureFree(&picture);
        *errorOut = QStringLiteral("libwebp could not read a %1x%2 frame.")
                        .arg(rgba.width()).arg(rgba.height());
        return false;
    }

    const int timestampMs = int(ms);
    const bool ok = WebPAnimEncoderAdd(d->enc, &picture, timestampMs, &d->config) != 0;
    const char *why = ok ? nullptr : WebPAnimEncoderGetError(d->enc);
    WebPPictureFree(&picture);
    if (!ok) {
        *errorOut = QStringLiteral("libwebp rejected the frame at %1 ms: %2")
                        .arg(timestampMs)
                        .arg(QString::fromLatin1(why ? why : "unknown"));
        return false;
    }
    return true;
}

bool WebpEncoder::onFinish(qint64 endMs, QString *errorOut)
{
    // A NULL picture only moves the end timestamp; without it the last frame's
    // duration is unknown and libwebp drops that frame from the output.
    if (!WebPAnimEncoderAdd(d->enc, nullptr, int(endMs), nullptr)) {
        const char *why = WebPAnimEncoderGetError(d->enc);
        *errorOut = QStringLiteral("libwebp rejected the end of the animation: %1")
                        .arg(QString::fromLatin1(why ? why : "unknown"));
        return false;
    }

    WebPData data;
    WebPDataInit(&data);
    if (!WebPAnimEncoderAssemble(d->enc, &data) || data.bytes == nullptr) {
        const char *why = WebPAnimEncoderGetError(d->enc);
        WebPDataClear(&data);
        *errorOut = QStringLiteral("Could not assemble the WebP animation: %1")
                        .arg(QString::fromLatin1(why ? why : "unknown"));
        return false;
    }
    const qint64 total = qint64(data.size);
    const qint64 written = output()->write(reinterpret_cast<const char *>(data.bytes), total);
    WebPDataClear(&data);
    if (written != total) {
        *errorOut = writeError();
        return false;
    }
    return true;
}

void WebpEncoder::onCancel()
{
    d = std::make_unique<Impl>();
}

} // namespace Editor::Video
