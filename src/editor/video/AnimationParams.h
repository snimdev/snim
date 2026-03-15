#ifndef EDITOR_VIDEO_ANIMATIONPARAMS_H
#define EDITOR_VIDEO_ANIMATIONPARAMS_H

#include <QFileInfo>
#include <QSize>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace Editor::Video {

/**
 * Knobs for the silent short-loop exports (animated GIF, animated WebP) and the pure
 * math behind them. Header-only and platform-neutral (no Qt widgets, no Objective-C)
 * so the encoders, the frame grabber, the editor, and the headless unit tests all
 * share exactly one source of truth for frame timing and downscaling.
 */
struct AnimationParams {
    int fps = 10;          // frame rate (kept low: these formats grow fast)
    int maxWidth = 1200;   // longest edge cap; 0 = no downscale
    int loopCount = 0;     // 0 = loop forever
    int quality = 75;      // lossy quality 0-100; ignored when lossless
    bool lossless = false; // WebP only: GIF has no lossless mode
    int effort = 4;        // WebP only: libwebp method 0-6, cpu spent per frame
    bool minimizeSize = true;  // WebP only: extra passes for a smaller file

    [[nodiscard]] AnimationParams clamped() const {
        AnimationParams p = *this;
        p.fps = std::clamp(p.fps, 1, 50);
        p.maxWidth = std::max(0, p.maxWidth);
        p.loopCount = std::max(0, p.loopCount);
        p.quality = std::clamp(p.quality, 0, 100);
        p.effort = std::clamp(p.effort, 0, 6);
        return p;
    }
};

/**
 * Sample times (ms, on the SOURCE timeline) for the animation frames covering
 * [inMs, outMs]. Always at least one frame; evenly spaced at 1/fps; the last sample
 * stays strictly inside the range so it maps to a real decodable frame. A
 * zero/negative span still yields a single frame at inMs (callers must guarantee
 * outMs >= inMs).
 */
inline QVector<qint64> planAnimationFrames(qint64 inMs, qint64 outMs, int fps)
{
    QVector<qint64> times;
    const qint64 span = std::max<qint64>(0, outMs - inMs);
    const int safeFps = std::max(1, fps);
    const qint64 stepMs = std::llround(1000.0 / safeFps);
    const int count = std::max<int>(1, int((span * safeFps) / 1000));   // floor, >=1
    times.reserve(count);
    for (int i = 0; i < count; ++i) {
        qint64 t = inMs + i * stepMs;
        if (t >= outMs && span > 0)
            t = outMs - 1;          // keep the last sample inside the range
        times.append(t);
    }
    return times;
}

/**
 * Downscale `src` so its longest edge is <= maxWidth, preserving aspect and never
 * upscaling. maxWidth <= 0 (or an empty source) passes the size through unchanged.
 */
inline QSize animationScaledSize(const QSize &src, int maxWidth)
{
    if (maxWidth <= 0 || src.isEmpty())
        return src;
    const int longest = std::max(src.width(), src.height());
    if (longest <= maxWidth)
        return src;
    const double scale = double(maxWidth) / longest;
    return QSize(std::max(1, int(std::lround(src.width() * scale))),
                 std::max(1, int(std::lround(src.height() * scale))));
}

// Which short-loop format an export produces. Both share one path through the editor
// (temp file, progress, move into place); only the strings and the exporter differ.
enum class AnimationFormat { Gif, WebP };

// Swap any extension on `sourceName` for "." + extension (Qt6-safe; no QRegExp).
inline QString animationFileNameFor(const QString &sourceName, const QString &extension)
{
    const QFileInfo info(sourceName);
    const QString base = info.completeBaseName();
    return (base.isEmpty() ? QStringLiteral("recording") : base)
           + QStringLiteral(".") + extension;
}

} // namespace Editor::Video

#endif // EDITOR_VIDEO_ANIMATIONPARAMS_H
