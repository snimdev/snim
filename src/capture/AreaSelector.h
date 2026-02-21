// AreaSelector.h
#ifndef AREASELECTOR_H
#define AREASELECTOR_H

#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPixmap>
#include <QImage>
#include <QVector>
#include <QHash>
#include <QIcon>
#include <QSharedPointer>
#include <memory>
#include <vector>
#include "capture/WindowEnumerator.h"

class QEvent;
class QShowEvent;
class QResizeEvent;

namespace Capture {

    class OverlayAnnotations;

    /**
     * Full-screen overlay that lets the user interactively select a region of a
     * frozen screenshot, CleanShot-style: crosshair guides, a magnifier loupe
     * with live color/coordinate readout, a live W×H dimension HUD, and a
     * two-phase flow (drag to draw, then adjust with handles before confirming).
     *
     * Coordinate contract (unchanged): areaSelected() carries the selection in
     * VIRTUAL-DESKTOP LOGICAL coordinates. Capture strategies convert that to
     * physical pixmap coordinates via (area - virtualGeometry) * devicePixelRatio.
     */
    class AreaSelector : public QWidget
    {
        Q_OBJECT

    public:
        enum class Mode { AreaSelect, WindowPick };

        explicit AreaSelector(QWidget *parent = nullptr);
        ~AreaSelector() override;

        void setScreenshot(const QPixmap &screenshot);

        // Full virtual-desktop bounds (origin may be non-zero on multi-monitor).
        void setVirtualGeometry(const QRect &virtualRect) { m_virtualGeometry = virtualRect; }

        // This widget's top-left within the virtual desktop (its screen origin).
        void setScreenOffset(const QPoint &offset) { m_screenOffset = offset; }

        void setMode(Mode mode) { m_mode = mode; }

        // Candidate window rects (virtual-desktop logical coords, front-to-back)
        // used by WindowPick mode. Empty -> falls back to screen-under-cursor.
        void setWindows(const QVector<QRect> &windows) { m_windows = windows; }

        // Like setWindows, but also keeps each window's platform id so a WindowPick
        // emits windowPicked(rect, id) for true window capture.
        void setWindowInfos(const QVector<WindowInfo> &infos);

        // Show the floating action toolbar (Edit/Copy/Save/Cancel) during the
        // adjust phase. Off by default; the normal capture path enables it, OCR
        // leaves it off.
        void setActionsEnabled(bool enabled) { m_actionsEnabled = enabled; }

        // Quick-annotation session shared by every overlay of one area capture; null
        // (the default) for recording, OCR and window pick.
        void setAnnotations(QSharedPointer<OverlayAnnotations> session);

        // Multi-monitor: mirror the live selection from a peer overlay on another
        // screen so this overlay renders its portion of a spanning selection.
        void applyPeerState(const QRect &selectionVirt, int phase, int mode, const QPoint &cursorVirt);

        // External accept/cancel hooks for the recording options bar. Committing
        // fires only while a non-empty selection is being adjusted (returns whether
        // it did); cancelling mirrors Esc (areaSelected with an empty rect).
        bool commitCurrentSelection();
        void cancelSelection() { cancel(); }

    signals:
        void areaSelected(const QRect &area);
        void copyRequested(const QRect &area);
        void saveRequested(const QRect &area);
        // WindowPick only: the picked window's rect AND its platform id (for true
        // window capture). Emitted just before areaSelected; consumers that only
        // want the rect (the screenshot path) can keep using areaSelected.
        void windowPicked(const QRect &area, quint64 windowId);
        // Emitted whenever this overlay's live selection/cursor changes, so peer
        // overlays on other monitors can mirror it (see applyPeerState).
        void liveStateChanged(const QRect &selectionVirt, int phase, int mode, const QPoint &cursorVirt);

    protected:
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;
        void paintEvent(QPaintEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void showEvent(QShowEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void leaveEvent(QEvent *event) override;

    private:
        // Input chain of responsibility, defined in AreaSelectorInput.h.
        class InputHandler;
        class ToolbarHandler;
        class TextEditingHandler;
        class StrokeHandler;
        class ToolHandler;
        class SelectionHandler;

        enum class Phase { Idle, Dragging, Adjusting };
        enum class Handle { None, TopLeft, Top, TopRight, Right,
                            BottomRight, Bottom, BottomLeft, Left, Interior };

        // virtual-desktop logical <-> this widget's local coordinates
        QPoint toLocal(const QPoint &virt) const { return virt - m_screenOffset; }
        QPoint toVirt(const QPoint &local) const { return local + m_screenOffset; }
        QRect  localSelection() const;                 // selection in local widget coords
        QRect  virtToSource(const QRect &virt) const;  // virtual-logical -> physical pixmap source

        Handle hitTest(const QPoint &local) const;
        QRect  handleRect(Handle h, const QRect &localSel) const;
        void   updateHoverWindow();                    // WindowPick: refresh highlighted rect + id
        void   broadcastState();                       // emit liveStateChanged for peer overlays
        // Is the (shared) cursor over THIS overlay's screen? Gates crosshair/magnifier
        // so they appear on the right monitor, even during a grabbed cross-monitor drag.
        bool   cursorOnThisScreen() const { return QRect(m_screenOffset, size()).contains(m_cursorVirt); }

        // Floating toolbar shown during Adjusting: [tools] | [undo redo] | [actions].
        enum ToolButton { BtnEdit = 0, BtnCopy, BtnSave, BtnCancel, BtnCount };
        struct BarSlot {
            enum class Kind { Tool, Undo, Redo, Action };
            Kind    kind = Kind::Action;
            QString toolId;     // Tool
            int     action = -1; // Action: a ToolButton
            QRect   rect;
        };
        // No anchor test, so the keyboard path works on whichever overlay has focus.
        bool   actionsAvailable() const;
        bool   toolbarVisible() const;
        // The one layout shared by paint and hit-test; wraps to two rows when too wide.
        QVector<BarSlot> toolbarSlots(QRect *bar = nullptr) const;
        int    toolbarButtonAt(const QPoint &local) const;   // index into toolbarSlots()
        void   triggerToolbarButton(int index);              // a ToolButton action
        void   activateBarSlot(const BarSlot &slot);
        void   toggleTool(const QString &id);
        QIcon  barIcon(const QString &path);
        QString barTooltip(const BarSlot &slot) const;
        void   paintBarGlyph(QPainter &p, const BarSlot &slot, const QRect &r);
        void   paintToolbar(QPainter &p);
        void   updateCursorShape(const QPoint &local);
        bool   toolArmed() const;
        void   applyHandleDrag(const QPoint &cursorVirt);
        void   commitSelection();
        void   cancel();
        void   rebuildBackgroundCache();
        template <typename Event>
        bool   dispatchInput(bool (InputHandler::*handle)(Event *), Event *event);

        // painters
        void paintBackground(QPainter &p);
        void paintSelection(QPainter &p);
        void paintHandles(QPainter &p, const QRect &localSel);
        void paintCrosshair(QPainter &p);
        void paintMagnifier(QPainter &p);
        void paintDimensionHud(QPainter &p);
        void paintInstructions(QPainter &p);

        Phase   m_phase = Phase::Idle;
        Mode    m_mode = Mode::AreaSelect;
        QRect   m_selectionVirt;        // selection (virtual-desktop logical coords)
        QPoint  m_cursorVirt;           // last known cursor pos (virtual-logical)
        QPoint  m_dragAnchorVirt;       // fixed corner while Dragging
        QRect   m_resizeBaseVirt;       // selection snapshot when a handle drag starts
        Handle  m_activeHandle = Handle::None;
        QPoint  m_moveGrabOffsetVirt;   // cursorVirt - selection.topLeft() while moving
        bool    m_hasCursor = false;

        QPixmap m_screenshot;           // full virtual-desktop frozen frame
        QImage  m_screenshotImage;      // cached copy for pixel color sampling
        qreal   m_dpr = 1.0;            // m_screenshot.devicePixelRatio()
        QRect   m_virtualGeometry;
        QPoint  m_screenOffset;
        QPixmap m_dimmedBg;             // cached dimmed screenshot slice for this widget

        QVector<QRect> m_windows;       // WindowPick candidates (virtual-logical)
        QVector<quint64> m_windowIds;   // parallel to m_windows (0 where unknown)
        quint64 m_hoverWindowId = 0;    // id of the window currently under the cursor

        bool   m_actionsEnabled = false; // show the action toolbar
        QSharedPointer<OverlayAnnotations> m_annotations;
        int    m_hoveredButton = -1;     // toolbar slot under cursor (-1 = none)
        QHash<QString, QIcon> m_barIcons; // white toolbar icons by resource path

        std::vector<std::unique_ptr<InputHandler>> m_inputChain;

        static constexpr int kHandleSize = 10;
        static constexpr int kHandleHit  = 12;
    };

} // namespace Capture

#endif // AREASELECTOR_H
