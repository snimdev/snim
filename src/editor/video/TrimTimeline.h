#ifndef EDITOR_VIDEO_TRIMTIMELINE_H
#define EDITOR_VIDEO_TRIMTIMELINE_H

#include "editor/video/TrimState.h"

#include <QWidget>

namespace Editor::Video {

/**
 * The trim strip under the video preview: a track with the kept range highlighted,
 * a draggable in/out handle at each end, and a playhead line. Dragging the body or
 * the playhead scrubs; dragging a handle adjusts the cut (clamped by TrimState).
 */
class TrimTimeline : public QWidget
{
    Q_OBJECT

public:
    explicit TrimTimeline(QWidget *parent = nullptr);

    void setDurationMs(qint64 durationMs);   // resets the range to keep-everything
    void setPlayheadMs(qint64 ms);
    void setInteractive(bool on);            // false while preview failed / exporting

    [[nodiscard]] const TrimState &state() const { return m_state; }
    void setInMs(qint64 ms);                 // keyboard I/O shortcuts route through these
    void setOutMs(qint64 ms);

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void inChanged(qint64 ms);
    void outChanged(qint64 ms);
    void scrubbed(qint64 ms);                // user moved the playhead -> seek

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum class Drag { None, InHandle, OutHandle, Playhead };

    [[nodiscard]] int trackWidth() const;            // width minus the handle insets
    [[nodiscard]] qint64 msAt(const QPoint &pos) const;
    [[nodiscard]] int xFor(qint64 ms) const;         // widget-local x for a time
    [[nodiscard]] Drag hitTest(const QPoint &pos) const;

    TrimState m_state;
    qint64 m_playheadMs = 0;
    Drag m_drag = Drag::None;
    bool m_interactive = true;

    static constexpr int kHandleHalfW = 7;   // grab zone (and inset) around handles
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_TRIMTIMELINE_H
