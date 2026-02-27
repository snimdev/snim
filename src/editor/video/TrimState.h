#ifndef EDITOR_VIDEO_TRIMSTATE_H
#define EDITOR_VIDEO_TRIMSTATE_H

#include <QtGlobal>

namespace Editor::Video {

/**
 * The trim model: a duration plus an [in, out] keep-range in milliseconds. Pure
 * arithmetic (no widgets, no player), the single source of truth for clamping
 * rules and the timeline's px<->ms mapping, so the riskiest math is unit-tested.
 */
class TrimState
{
public:
    // A cut can never be shorter than this; handle drags snap against it.
    static constexpr qint64 kMinSpanMs = 200;

    // Setting the duration resets the range to "keep everything" and re-clamps.
    void setDurationMs(qint64 durationMs);
    [[nodiscard]] qint64 durationMs() const { return m_durationMs; }

    void setInMs(qint64 ms);    // clamped to [0, out - kMinSpanMs]
    void setOutMs(qint64 ms);   // clamped to [in + kMinSpanMs, duration]
    [[nodiscard]] qint64 inMs() const { return m_inMs; }
    [[nodiscard]] qint64 outMs() const { return m_outMs; }

    // True when saving must export a cut instead of moving the file as-is. Clips
    // shorter than the minimum span are never trimmable.
    [[nodiscard]] bool isTrimmed() const;
    [[nodiscard]] bool isTrimmable() const { return m_durationMs >= kMinSpanMs; }
    [[nodiscard]] qint64 trimmedDurationMs() const { return m_outMs - m_inMs; }

    // px<->ms mapping for a track of trackWidthPx pixels. Guards zero/negative
    // widths and zero duration (both return 0), so headless construction is safe.
    [[nodiscard]] qint64 xToMs(int x, int trackWidthPx) const;
    [[nodiscard]] int msToX(qint64 ms, int trackWidthPx) const;

private:
    qint64 m_durationMs = 0;
    qint64 m_inMs = 0;
    qint64 m_outMs = 0;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_TRIMSTATE_H
