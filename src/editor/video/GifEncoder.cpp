#include "editor/video/GifEncoder.h"

#include <gif_lib.h>
#include <libimagequant.h>

#include <QIODevice>
#include <algorithm>
#include <vector>

namespace Editor::Video {

namespace {

// Where giflib's writes go; a failed write is remembered because EGifCloseFile ignores it.
struct Sink {
    QIODevice *device = nullptr;
    bool failed = false;
};

int writeToSink(GifFileType *gif, const GifByteType *data, int length)
{
    auto *sink = static_cast<Sink *>(gif->UserData);
    if (sink->failed || !sink->device)
        return 0;
    const qint64 written = sink->device->write(reinterpret_cast<const char *>(data), length);
    if (written != length) {
        sink->failed = true;
        return 0;
    }
    return length;
}

// GIF delays are centiseconds; rounding each timestamp (not each delta) carries the error.
qint64 toCentiseconds(qint64 ms)
{
    return (ms + 5) / 10;
}

// GifMakeMapObject only accepts a power of two, and at least 2 entries.
int paletteSize(int colors)
{
    int size = 2;
    while (size < colors)
        size *= 2;
    return size;
}

constexpr int kMaxDelayCs = 65535;   // the field is 16 bits
constexpr int kLiqSpeed = 4;         // libimagequant's balanced default

} // namespace

struct GifEncoder::Impl {
    liq_attr *attr = nullptr;
    GifFileType *gif = nullptr;
    Sink sink;
    QSize size;

    // The last frame waits here until the next one (or finish) says how long it shows.
    std::vector<GifPixelType> heldPixels;
    ColorMapObject *heldMap = nullptr;
    qint64 heldStartCs = 0;
    bool hasHeld = false;

    // Freeing everything here is what lets onCancel() be a plain state reset. The trailer
    // EGifCloseFile writes lands in a file the base is about to discard anyway.
    ~Impl()
    {
        if (gif) {
            int error = 0;
            EGifCloseFile(gif, &error);
        }
        if (heldMap)
            GifFreeMapObject(heldMap);
        if (attr)
            liq_attr_destroy(attr);
    }
};

GifEncoder::GifEncoder() : d(std::make_unique<Impl>()) {}

GifEncoder::~GifEncoder() = default;

bool GifEncoder::gifFailed(QString *errorOut, const QString &step)
{
    if (d->sink.failed) {
        *errorOut = writeError();
        return false;
    }
    const char *why = d->gif ? GifErrorString(d->gif->Error) : nullptr;
    *errorOut = QStringLiteral("giflib could not %1: %2")
                    .arg(step, QString::fromLatin1(why ? why : "unknown error"));
    return false;
}

bool GifEncoder::onBegin(const QSize &size, const AnimationParams &params, QString *errorOut)
{
    d->attr = liq_attr_create();
    if (!d->attr) {
        *errorOut = QStringLiteral("Could not start libimagequant.");
        return false;
    }
    // Minimum 0 so a frame is never refused, only given fewer colors.
    liq_set_quality(d->attr, 0, params.quality);
    liq_set_speed(d->attr, kLiqSpeed);

    d->sink.device = output();
    int error = 0;
    d->gif = EGifOpen(&d->sink, writeToSink, &error);
    if (!d->gif) {
        const char *why = GifErrorString(error);
        *errorOut = QStringLiteral("Could not start the GIF encoder: %1")
                        .arg(QString::fromLatin1(why ? why : "unknown error"));
        return false;
    }
    d->size = size;

    // GIF89a for the delays and the loop; no global palette, every frame brings its own.
    EGifSetGifVersion(d->gif, true);
    if (EGifPutScreenDesc(d->gif, size.width(), size.height(), 8, 0, nullptr) == GIF_ERROR)
        return gifFailed(errorOut, QStringLiteral("write the screen descriptor"));

    // NETSCAPE2.0 must come before the first frame for viewers to honour the loop count.
    const int loops = std::min(params.loopCount, 65535);
    const GifByteType loopBlock[3] = {1, GifByteType(loops & 0xff), GifByteType(loops >> 8)};
    if (EGifPutExtensionLeader(d->gif, APPLICATION_EXT_FUNC_CODE) == GIF_ERROR
        || EGifPutExtensionBlock(d->gif, 11, "NETSCAPE2.0") == GIF_ERROR
        || EGifPutExtensionBlock(d->gif, 3, loopBlock) == GIF_ERROR
        || EGifPutExtensionTrailer(d->gif) == GIF_ERROR) {
        return gifFailed(errorOut, QStringLiteral("write the loop count"));
    }
    return true;
}

bool GifEncoder::onFrame(const QImage &rgba, qint64 ms, QString *errorOut)
{
    const int width = rgba.width();
    const int height = rgba.height();

    // libimagequant only reads the rows; the pointer array must outlive the image.
    std::vector<void *> rows(static_cast<size_t>(height));
    for (int y = 0; y < height; ++y)
        rows[size_t(y)] = const_cast<uchar *>(rgba.constScanLine(y));
    liq_image *image = liq_image_create_rgba_rows(d->attr, rows.data(), width, height, 0);
    if (!image) {
        *errorOut = QStringLiteral("libimagequant could not read a %1x%2 frame.")
                        .arg(width).arg(height);
        return false;
    }

    liq_result *result = nullptr;
    const liq_error quantized = liq_image_quantize(image, d->attr, &result);
    if (quantized != LIQ_OK || !result) {
        liq_image_destroy(image);
        *errorOut = QStringLiteral("libimagequant could not build a palette (error %1).")
                        .arg(int(quantized));
        return false;
    }
    liq_set_dithering_level(result, 1.0f);

    std::vector<GifPixelType> pixels(size_t(width) * size_t(height));
    const liq_error remapped = liq_write_remapped_image(result, image, pixels.data(),
                                                        pixels.size());
    // The palette is final only after remapping.
    const liq_palette *palette = remapped == LIQ_OK ? liq_get_palette(result) : nullptr;
    ColorMapObject *map = nullptr;
    if (palette) {
        const int colors = std::clamp(int(palette->count), 1, 256);
        map = GifMakeMapObject(paletteSize(colors), nullptr);
        if (map) {
            for (int i = 0; i < map->ColorCount; ++i) {
                const liq_color c = i < colors ? palette->entries[i] : liq_color{0, 0, 0, 255};
                map->Colors[i] = GifColorType{c.r, c.g, c.b};
            }
        }
    }
    liq_result_destroy(result);
    liq_image_destroy(image);
    if (!map) {
        *errorOut = remapped != LIQ_OK
                        ? QStringLiteral("libimagequant could not remap a frame (error %1).")
                              .arg(int(remapped))
                        : QStringLiteral("Could not build a GIF palette.");
        return false;
    }

    // The held frame's delay is known now. One that rounds to nothing is dropped and its
    // time goes to this frame, so the total length still adds up.
    const qint64 startCs = toCentiseconds(ms);
    if (d->hasHeld) {
        const qint64 delayCs = startCs - d->heldStartCs;
        if (delayCs > 0) {
            if (!writeHeldFrame(int(std::min<qint64>(delayCs, kMaxDelayCs)), errorOut)) {
                GifFreeMapObject(map);
                return false;
            }
            d->heldStartCs = startCs;
        }
        if (d->heldMap)
            GifFreeMapObject(d->heldMap);
    } else {
        d->heldStartCs = startCs;
    }
    d->heldPixels = std::move(pixels);
    d->heldMap = map;
    d->hasHeld = true;
    return true;
}

bool GifEncoder::writeHeldFrame(int delayCs, QString *errorOut)
{
    GraphicsControlBlock control;
    control.DisposalMode = DISPOSE_DO_NOT;
    control.UserInputFlag = false;
    control.DelayTime = delayCs;
    control.TransparentColor = NO_TRANSPARENT_COLOR;
    GifByteType extension[4];
    const size_t length = EGifGCBToExtension(&control, extension);
    if (EGifPutExtension(d->gif, GRAPHICS_EXT_FUNC_CODE, int(length), extension) == GIF_ERROR)
        return gifFailed(errorOut, QStringLiteral("write a frame delay"));

    const int width = d->size.width();
    const int height = d->size.height();
    if (EGifPutImageDesc(d->gif, 0, 0, width, height, false, d->heldMap) == GIF_ERROR)
        return gifFailed(errorOut, QStringLiteral("write a frame header"));
    for (int y = 0; y < height; ++y) {
        GifPixelType *line = d->heldPixels.data() + size_t(y) * size_t(width);
        if (EGifPutLine(d->gif, line, width) == GIF_ERROR)
            return gifFailed(errorOut, QStringLiteral("compress a frame"));
    }
    return true;
}

bool GifEncoder::onFinish(qint64 endMs, QString *errorOut)
{
    if (!d->hasHeld) {
        *errorOut = QStringLiteral("No frames were added.");
        return false;
    }
    const qint64 delayCs = std::clamp<qint64>(toCentiseconds(endMs) - d->heldStartCs,
                                              1, kMaxDelayCs);
    if (!writeHeldFrame(int(delayCs), errorOut))
        return false;
    GifFreeMapObject(d->heldMap);
    d->heldMap = nullptr;
    d->hasHeld = false;

    // Writes the trailer and frees the handle, even when it reports an error.
    int error = 0;
    GifFileType *gif = d->gif;
    d->gif = nullptr;
    if (EGifCloseFile(gif, &error) == GIF_ERROR || d->sink.failed) {
        if (d->sink.failed) {
            *errorOut = writeError();
        } else {
            const char *why = GifErrorString(error);
            *errorOut = QStringLiteral("giflib could not close the file: %1")
                            .arg(QString::fromLatin1(why ? why : "unknown error"));
        }
        return false;
    }
    return true;
}

void GifEncoder::onCancel()
{
    d = std::make_unique<Impl>();
}

} // namespace Editor::Video
