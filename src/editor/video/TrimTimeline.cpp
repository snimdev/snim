#include "editor/video/TrimTimeline.h"

#include <QMouseEvent>
#include <QPainter>

namespace Editor::Video {

namespace {
const QColor kAccent(0, 150, 255);            // matches the selection / frame accent
constexpr int kTrackHeight = 14;
constexpr int kHandleWidth = 8;
constexpr int kHandleHeight = 26;
} // namespace

TrimTimeline::TrimTimeline(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(40);
}

QSize TrimTimeline::sizeHint() const { return {480, 44}; }

void TrimTimeline::setDurationMs(qint64 durationMs)
{
    m_state.setDurationMs(durationMs);
    m_playheadMs = 0;
    update();
}

void TrimTimeline::setPlayheadMs(qint64 ms)
{
    if (ms == m_playheadMs)
        return;
    m_playheadMs = ms;
    update();
}

void TrimTimeline::setInteractive(bool on)
{
    m_interactive = on;
    setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void TrimTimeline::setInMs(qint64 ms)
{
    m_state.setInMs(ms);
    emit inChanged(m_state.inMs());
    update();
}

void TrimTimeline::setOutMs(qint64 ms)
{
    m_state.setOutMs(ms);
    emit outChanged(m_state.outMs());
    update();
}

int TrimTimeline::trackWidth() const { return width() - 2 * kHandleHalfW; }

qint64 TrimTimeline::msAt(const QPoint &pos) const
{
    return m_state.xToMs(pos.x() - kHandleHalfW, trackWidth());
}

int TrimTimeline::xFor(qint64 ms) const
{
    return kHandleHalfW + m_state.msToX(ms, trackWidth());
}

TrimTimeline::Drag TrimTimeline::hitTest(const QPoint &pos) const
{
    if (!m_state.isTrimmable())
        return Drag::Playhead;                 // tiny clips: scrub only
    const int inX = xFor(m_state.inMs());
    const int outX = xFor(m_state.outMs());
    // Handles win over the body; if the range is collapsed, pick by which side.
    if (qAbs(pos.x() - inX) <= kHandleHalfW && qAbs(pos.x() - inX) <= qAbs(pos.x() - outX))
        return Drag::InHandle;
    if (qAbs(pos.x() - outX) <= kHandleHalfW)
        return Drag::OutHandle;
    return Drag::Playhead;
}

void TrimTimeline::mousePressEvent(QMouseEvent *event)
{
    if (!m_interactive || event->button() != Qt::LeftButton)
        return;
    m_drag = hitTest(event->pos());
    mouseMoveEvent(event);                     // apply immediately (click = jump)
}

void TrimTimeline::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_interactive || m_drag == Drag::None)
        return;
    const qint64 ms = msAt(event->pos());
    switch (m_drag) {
    case Drag::InHandle:
        m_state.setInMs(ms);
        emit inChanged(m_state.inMs());
        break;
    case Drag::OutHandle:
        m_state.setOutMs(ms);
        emit outChanged(m_state.outMs());
        break;
    case Drag::Playhead:
        m_playheadMs = ms;
        emit scrubbed(ms);
        break;
    case Drag::None:
        return;
    }
    update();
}

void TrimTimeline::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = Drag::None;
}

void TrimTimeline::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int trackY = (height() - kTrackHeight) / 2;
    const QRect track(kHandleHalfW, trackY, trackWidth(), kTrackHeight);

    // Full track (the parts being cut away stay this dim shade).
    p.setPen(Qt::NoPen);
    p.setBrush(palette().color(QPalette::Mid));
    p.drawRoundedRect(track, 4, 4);

    if (m_state.durationMs() <= 0)
        return;

    // Kept range.
    const int inX = xFor(m_state.inMs());
    const int outX = xFor(m_state.outMs());
    QColor keep = kAccent;
    keep.setAlpha(m_interactive ? 110 : 50);
    p.setBrush(keep);
    p.drawRoundedRect(QRect(inX, trackY, outX - inX, kTrackHeight), 4, 4);

    // In/out handles (vertical grab bars).
    if (m_state.isTrimmable()) {
        p.setBrush(m_interactive ? kAccent : palette().color(QPalette::Mid));
        const int handleY = (height() - kHandleHeight) / 2;
        p.drawRoundedRect(QRect(inX - kHandleWidth / 2, handleY, kHandleWidth, kHandleHeight), 3, 3);
        p.drawRoundedRect(QRect(outX - kHandleWidth / 2, handleY, kHandleWidth, kHandleHeight), 3, 3);
    }

    // Playhead.
    const int phX = xFor(m_playheadMs);
    p.setPen(QPen(palette().color(QPalette::Text), 2));
    p.drawLine(phX, trackY - 5, phX, trackY + kTrackHeight + 5);
}

} // namespace Editor::Video
