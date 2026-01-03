#include "editor/video/TrimState.h"

#include <algorithm>
#include <cmath>

namespace Editor::Video {

void TrimState::setDurationMs(qint64 durationMs)
{
    m_durationMs = std::max<qint64>(0, durationMs);
    // New media (or the player refining its duration estimate): keep everything.
    m_inMs = 0;
    m_outMs = m_durationMs;
}

void TrimState::setInMs(qint64 ms)
{
    if (!isTrimmable())
        return;
    m_inMs = std::clamp<qint64>(ms, 0, m_outMs - kMinSpanMs);
}

void TrimState::setOutMs(qint64 ms)
{
    if (!isTrimmable())
        return;
    m_outMs = std::clamp<qint64>(ms, m_inMs + kMinSpanMs, m_durationMs);
}

bool TrimState::isTrimmed() const
{
    return isTrimmable() && (m_inMs > 0 || m_outMs < m_durationMs);
}

qint64 TrimState::xToMs(int x, int trackWidthPx) const
{
    if (trackWidthPx <= 0 || m_durationMs <= 0)
        return 0;
    const qint64 ms = (qint64) std::llround(double(x) * double(m_durationMs) / trackWidthPx);
    return std::clamp<qint64>(ms, 0, m_durationMs);
}

int TrimState::msToX(qint64 ms, int trackWidthPx) const
{
    if (trackWidthPx <= 0 || m_durationMs <= 0)
        return 0;
    const qint64 clamped = std::clamp<qint64>(ms, 0, m_durationMs);
    return (int) std::llround(double(clamped) * trackWidthPx / double(m_durationMs));
}

} // namespace Editor::Video
