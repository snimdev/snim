#ifndef EDITOR_VIDEO_GIFPARAMS_H
#define EDITOR_VIDEO_GIFPARAMS_H

#include <QFileInfo>
#include <QSize>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace Editor::Video {

/**
 * GIF export knobs and the pure math behind them. Header-only and platform-neutral
 * (no Qt widgets, no Objective-C) so the macOS encoder, the cross-platform editor,
 * and the headless unit tests all share exactly one source of truth — the riskiest
 * arithmetic (frame timing, downscaling) is therefore testable off-platform.
 */
struct GifParams {
    int fps = 10;          // GIF frame rate (kept low — GIF size grows fast)
    int maxWidth = 600;    // longest edge cap; 0 = no downscale
    int loopCount = 0;     // 0 = loop forever

    [[nodiscard]] GifParams clamped() const {
        GifParams g = *this;
        g.fps = std::clamp(g.fps, 1, 50);
        g.maxWidth = std::max(0, g.maxWidth);
        g.loopCount = std::max(0, g.loopCount);
        return g;
    }
};

/**
 * Sample times (ms, on the SOURCE timeline) for the GIF frames covering [inMs, outMs].
 * Always at least one frame; evenly spaced at 1/fps; the last sample stays strictly
 * inside the range so it maps to a real decodable frame. A zero/negative span still
 * yields a single frame at inMs (callers must guarantee outMs >= inMs).
 */
inline QVector<qint64> planGifFrames(qint64 inMs, qint64 outMs, int fps)
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

// Per-frame on-screen duration in seconds (the GIF delay metadata).
inline double gifFrameDelaySec(int fps)
{
    return 1.0 / std::max(1, fps);
}

/**
 * Downscale `src` so its longest edge is <= maxWidth, preserving aspect and never
 * upscaling. maxWidth <= 0 (or an empty source) passes the size through unchanged.
 */
inline QSize gifScaledSize(const QSize &src, int maxWidth)
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

// Swap any extension on `sourceName` for ".gif" (Qt6-safe; no QRegExp).
inline QString gifFileNameFor(const QString &sourceName)
{
    const QFileInfo info(sourceName);
    const QString base = info.completeBaseName();
    return (base.isEmpty() ? QStringLiteral("recording") : base) + QStringLiteral(".gif");
}

} // namespace Editor::Video

#endif // EDITOR_VIDEO_GIFPARAMS_H
