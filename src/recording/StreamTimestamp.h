#ifndef RECORDING_STREAMTIMESTAMP_H
#define RECORDING_STREAMTIMESTAMP_H

#include <QtGlobal>

#include <limits>

namespace Recording {

/**
 * Running time for a frame from a ScreenCast stream. All times are nanoseconds; the
 * clock is the pipeline's monotonic one. Producers stamp frames in their own domain:
 * KDE and GNOME on the monotonic clock, xdg-desktop-portal-wlr there too except for
 * the odd frame stamped 0. A stamp within kTolerance of now is trusted, as it is the
 * capture moment; anything else falls back to the arrival time. The result never goes
 * backwards from `previous`. Pure, so it is unit-tested.
 */
struct StreamTimestamp {
    static constexpr quint64 kNone = std::numeric_limits<quint64>::max();
    static constexpr quint64 kTolerance = 1'000'000'000;

    [[nodiscard]] static quint64 runningTime(quint64 pts, quint64 clockNow, quint64 baseTime,
                                             quint64 previous)
    {
        if (clockNow < baseTime)
            return previous == kNone ? 0 : previous;

        quint64 clockTime = clockNow;
        const bool trusted = pts != kNone && pts >= baseTime
                             && (pts > clockNow ? pts - clockNow : clockNow - pts) <= kTolerance;
        if (trusted)
            clockTime = pts;

        const quint64 running = clockTime - baseTime;
        if (previous != kNone && running <= previous)
            return previous + 1;
        return running;
    }
};

} // namespace Recording

#endif // RECORDING_STREAMTIMESTAMP_H
