#ifndef EDITOR_VIDEO_TIMECODE_H
#define EDITOR_VIDEO_TIMECODE_H

#include <QFileInfo>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace Editor::Video {

/**
 * The trim editor's time readout and frame-step math. Header-only and pure (no
 * widgets, no player) so the formatting and fallback rules are unit-tested.
 */

// "m:ss.d" with tenths truncated, never rounded up; negative times read as 0:00.0.
inline QString formatTimecode(qint64 ms)
{
    const qint64 safe = std::max<qint64>(0, ms);
    return QStringLiteral("%1:%2.%3")
        .arg(safe / 60'000)
        .arg((safe / 1000) % 60, 2, 10, QLatin1Char('0'))
        .arg((safe / 100) % 10);
}

// One source frame in ms; an unknown or nonsense rate falls back to 30 fps.
inline qint64 frameStepMs(double fps)
{
    const double safeFps = (std::isfinite(fps) && fps > 0) ? fps : 30.0;
    return std::max<qint64>(1, std::llround(1000.0 / safeFps));
}

// "<base>_frame_0m12s3.png" for a still at ms: the timecode without colons, file safe.
inline QString frameFileNameFor(const QString &sourceName, qint64 ms)
{
    const qint64 safe = std::max<qint64>(0, ms);
    const QString base = QFileInfo(sourceName).completeBaseName();
    return (base.isEmpty() ? QStringLiteral("recording") : base)
           + QStringLiteral("_frame_%1m%2s%3.png")
                 .arg(safe / 60'000)
                 .arg((safe / 1000) % 60, 2, 10, QLatin1Char('0'))
                 .arg((safe / 100) % 10);
}

} // namespace Editor::Video

#endif // EDITOR_VIDEO_TIMECODE_H
