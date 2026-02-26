#include "capture/AreaSelectorInput.h"

#include "capture/OverlayAnnotations.h"
#include "editor/annotations/ToolRegistry.h"

#include <QKeySequence>
#include <algorithm>

namespace Capture {

namespace {
    constexpr int kClickThreshold = 4;        // px before an interior press becomes a drag
}

// ---- ToolbarHandler --------------------------------------------------------

bool AreaSelector::ToolbarHandler::mousePress(QMouseEvent *event)
{
    auto &s = m_sel;
    if (event->button() != Qt::LeftButton) return false;
    const QPoint local = event->pos();
    const int b = s.toolbarButtonAt(local);
    if (b < 0) return false;
    s.m_cursorVirt = s.toVirt(local);
    s.m_hasCursor = true;
    s.activateBarSlot(s.toolbarSlots().at(b));
    return true;
}

// ---- TextEditingHandler ----------------------------------------------------

bool AreaSelector::TextEditingHandler::keyPress(QKeyEvent *event)
{
    auto &s = m_sel;
    if (!s.m_annotations || !s.m_annotations->isEditingText()) return false;
    if (event->key() == Qt::Key_Escape)
        s.m_annotations->commitPendingText();
    else
        s.m_annotations->forwardKey(event);
    return true;
}

// ---- StrokeHandler ---------------------------------------------------------

bool AreaSelector::StrokeHandler::armed() const
{
    const auto &s = m_sel;
    return s.toolArmed() && s.m_mode == Mode::AreaSelect && s.m_phase == Phase::Adjusting &&
           !s.m_selectionVirt.isEmpty();
}

bool AreaSelector::StrokeHandler::keyPress(QKeyEvent *event)
{
    auto &s = m_sel;
    if (event->key() != Qt::Key_Escape || !s.m_annotations || !s.m_annotations->isDrawing())
        return false;
    s.m_annotations->cancelStroke();
    return true;
}

bool AreaSelector::StrokeHandler::mousePress(QMouseEvent *event)
{
    auto &s = m_sel;
    if (event->button() != Qt::LeftButton || !armed()) return false;
    const QPoint local = event->pos();
    const Handle h = s.hitTest(local);
    if (h != Handle::Interior && h != Handle::None) return false;   // handles still resize
    s.m_cursorVirt = s.toVirt(local);
    s.m_hasCursor = true;
    // Outside the selection the press only ends typing: no fresh selection while drawing.
    const bool reachesSession = h == Handle::Interior || s.m_annotations->isEditingText();
    if (reachesSession && s.m_annotations->press(s.m_cursorVirt))
        s.setCursor(s.m_annotations->cursor());
    return true;
}

bool AreaSelector::StrokeHandler::mouseMove(QMouseEvent *event)
{
    auto &s = m_sel;
    if (!s.m_annotations || !s.m_annotations->isDrawing()) return false;
    s.m_cursorVirt = s.toVirt(event->pos());
    s.m_annotations->move(s.m_cursorVirt);
    return true;
}

bool AreaSelector::StrokeHandler::mouseRelease(QMouseEvent *event)
{
    auto &s = m_sel;
    if (event->button() != Qt::LeftButton || !s.m_annotations || !s.m_annotations->isDrawing())
        return false;
    s.m_cursorVirt = s.toVirt(event->pos());
    s.m_annotations->release(s.m_cursorVirt);
    s.updateCursorShape(event->pos());
    return true;
}

bool AreaSelector::StrokeHandler::mouseDoubleClick(QMouseEvent *event)
{
    // The platform already delivered this click's press, so only keep it from committing.
    return event->button() == Qt::LeftButton && armed();
}

// ---- ToolHandler -----------------------------------------------------------

bool AreaSelector::ToolHandler::keyPress(QKeyEvent *event)
{
    auto &s = m_sel;
    if (!s.m_annotations || !s.actionsAvailable()) return false;
    OverlayAnnotations &session = *s.m_annotations;

    if (event->matches(QKeySequence::Undo)) {
        session.undo();
        return true;
    }
    if (event->matches(QKeySequence::Redo)) {
        session.redo();
        return true;
    }

    if (event->key() == Qt::Key_Escape) {
        if (session.activeTool().isEmpty()) return false;
        session.setActiveTool({});
    } else {
        if (event->modifiers() != Qt::NoModifier || event->key() < Qt::Key_A || event->key() > Qt::Key_Z)
            return false;
        const Editor::ToolSpec *spec = Editor::ToolRegistry::findByShortcut(QChar(event->key()));
        if (!spec || std::find(std::begin(kOverlayTools), std::end(kOverlayTools), spec->id)
                         == std::end(kOverlayTools))
            return false;
        if (event->isAutoRepeat()) return true;
        s.toggleTool(spec->id);
    }
    if (s.m_hoveredButton < 0)
        s.updateCursorShape(s.toLocal(s.m_cursorVirt));
    return true;
}

// ---- SelectionHandler ------------------------------------------------------

bool AreaSelector::SelectionHandler::mousePress(QMouseEvent *event)
{
    auto &s = m_sel;
    if (event->button() != Qt::LeftButton) return true;
    const QPoint local = event->pos();
    s.m_cursorVirt = s.toVirt(local);
    s.m_hasCursor = true;

    if (s.m_mode == Mode::WindowPick) {
        s.updateHoverWindow();
        if (!s.m_selectionVirt.isEmpty())
            s.commitSelection();
        return true;
    }

    if (s.m_phase == Phase::Adjusting) {
        const Handle h = s.hitTest(local);
        if (h == Handle::Interior) {
            s.m_activeHandle = Handle::Interior;
            s.m_moveGrabOffsetVirt = s.m_cursorVirt - s.m_selectionVirt.topLeft();
            m_interiorPressLocal = local;   // track for click-vs-drag
            m_interiorMoved = false;
            return true;
        }
        if (h != Handle::None) {
            s.m_activeHandle = h;
            s.m_resizeBaseVirt = s.m_selectionVirt;
            return true;
        }
        // clicked outside the selection -> start a fresh one
    }
    if (s.m_annotations)
        s.m_annotations->clear();

    s.m_phase = Phase::Dragging;
    s.m_activeHandle = Handle::None;
    s.m_dragAnchorVirt = s.m_cursorVirt;
    s.m_selectionVirt = QRect(s.m_dragAnchorVirt, QSize(0, 0));
    s.broadcastState();
    s.update();
    return true;
}

bool AreaSelector::SelectionHandler::mouseMove(QMouseEvent *event)
{
    auto &s = m_sel;
    const QPoint local = event->pos();
    s.m_cursorVirt = s.toVirt(local);
    s.m_hasCursor = true;

    if (s.m_mode == Mode::WindowPick) {
        s.updateHoverWindow();
        s.broadcastState();
        s.update();
        return true;
    }

    if (s.m_phase == Phase::Dragging) {
        s.m_selectionVirt = QRect(s.m_dragAnchorVirt, s.m_cursorVirt).normalized();
    } else if (s.m_phase == Phase::Adjusting && s.m_activeHandle != Handle::None) {
        // An interior press only becomes a move once it passes the click threshold,
        // so a plain click-inside stays a "confirm" gesture (handled on release).
        if (s.m_activeHandle == Handle::Interior && !m_interiorMoved &&
            (local - m_interiorPressLocal).manhattanLength() < kClickThreshold) {
            // pending click, don't move yet
        } else {
            if (s.m_activeHandle == Handle::Interior)
                m_interiorMoved = true;
            s.applyHandleDrag(s.m_cursorVirt);
        }
    } else if (s.m_phase == Phase::Adjusting) {
        const int b = s.toolbarButtonAt(local);
        s.m_hoveredButton = b;
        if (b >= 0)
            s.setCursor(Qt::PointingHandCursor);
        else
            s.updateCursorShape(local);
    }
    s.broadcastState();
    s.update();
    return true;
}

bool AreaSelector::SelectionHandler::mouseRelease(QMouseEvent *event)
{
    auto &s = m_sel;
    if (event->button() != Qt::LeftButton) return true;

    if (s.m_phase == Phase::Dragging) {
        if (s.m_selectionVirt.width() < 4 || s.m_selectionVirt.height() < 4) {
            // treat as an accidental click: discard and return to idle
            s.m_phase = Phase::Idle;
            s.m_selectionVirt = QRect();
        } else {
            s.m_phase = Phase::Adjusting;
        }
        s.broadcastState();
        s.update();
    } else if (s.m_phase == Phase::Adjusting) {
        if (s.m_activeHandle == Handle::Interior && !m_interiorMoved) {
            // clicked inside without dragging -> confirm (open editor)
            s.commitSelection();
            return true;
        }
        s.m_activeHandle = Handle::None;
        s.updateCursorShape(event->pos());
        s.broadcastState();
        s.update();
    }
    return true;
}

bool AreaSelector::SelectionHandler::mouseDoubleClick(QMouseEvent *event)
{
    auto &s = m_sel;
    if (event->button() != Qt::LeftButton) return true;
    if (s.m_phase == Phase::Adjusting && s.m_selectionVirt.contains(s.toVirt(event->pos())))
        s.commitSelection();
    return true;
}

bool AreaSelector::SelectionHandler::keyPress(QKeyEvent *event)
{
    auto &s = m_sel;
    switch (event->key()) {
        case Qt::Key_Escape:
            if (s.m_phase == Phase::Dragging || s.m_phase == Phase::Adjusting) {
                s.m_phase = Phase::Idle;
                s.m_selectionVirt = QRect();
                s.m_activeHandle = Handle::None;
                if (s.m_annotations)
                    s.m_annotations->clear();
                s.setCursor(Qt::CrossCursor);
                s.broadcastState();
                s.update();
            } else {
                s.cancel();
            }
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (s.m_phase == Phase::Adjusting)
                s.commitSelection();
            return true;
        default:
            break;
    }

    // matches() wants the exact platform chord (Cmd on macOS), so a bare C or S never lands here.
    if (event->matches(QKeySequence::Copy) && s.actionsAvailable()) {
        s.triggerToolbarButton(BtnCopy);
        return true;
    }
    if (event->matches(QKeySequence::Save) && s.actionsAvailable()) {
        s.triggerToolbarButton(BtnSave);
        return true;
    }

    if (s.m_phase == Phase::Adjusting && !s.m_selectionVirt.isEmpty()) {
        const int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
        QRect r = s.m_selectionVirt;
        switch (event->key()) {
            case Qt::Key_Left:  r.translate(-step, 0); break;
            case Qt::Key_Right: r.translate(step, 0);  break;
            case Qt::Key_Up:    r.translate(0, -step); break;
            case Qt::Key_Down:  r.translate(0, step);  break;
            default: return false;
        }
        s.m_selectionVirt = r;
        s.broadcastState();
        s.update();
        return true;
    }
    return false;
}

} // namespace Capture
