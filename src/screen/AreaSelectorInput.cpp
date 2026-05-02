#include "screen/AreaSelectorInput.h"

#include <QKeySequence>

namespace Screen {

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

// ---- LayerHandler ----------------------------------------------------------

bool AreaSelector::LayerHandler::keyPress(QKeyEvent *event)
{
    return m_sel.m_layer && m_sel.m_layer->handleKey(event, *this);
}

bool AreaSelector::LayerHandler::forward(SelectionLayer::MouseAction action, QMouseEvent *event)
{
    auto &s = m_sel;
    const QPoint virt = s.toVirt(event->pos());
    return s.m_layer && s.m_layer->handleMouse(action, virt, event->button(), *this);
}

bool AreaSelector::LayerHandler::mousePress(QMouseEvent *event)
{
    if (!forward(SelectionLayer::MouseAction::Press, event)) return false;
    m_sel.m_cursorVirt = m_sel.toVirt(event->pos());
    m_sel.m_hasCursor = true;
    return true;
}

bool AreaSelector::LayerHandler::mouseMove(QMouseEvent *event)
{
    if (!forward(SelectionLayer::MouseAction::Move, event)) return false;
    m_sel.m_cursorVirt = m_sel.toVirt(event->pos());
    return true;
}

bool AreaSelector::LayerHandler::mouseRelease(QMouseEvent *event)
{
    if (!forward(SelectionLayer::MouseAction::Release, event)) return false;
    m_sel.m_cursorVirt = m_sel.toVirt(event->pos());
    m_sel.updateCursorShape(event->pos());
    return true;
}

bool AreaSelector::LayerHandler::mouseDoubleClick(QMouseEvent *event)
{
    return forward(SelectionLayer::MouseAction::DoubleClick, event);
}

bool AreaSelector::LayerHandler::adjusting() const
{
    const auto &s = m_sel;
    return s.m_mode == Mode::AreaSelect && s.m_phase == Phase::Adjusting && !s.m_selectionVirt.isEmpty();
}

bool AreaSelector::LayerHandler::actionsAvailable() const
{
    return m_sel.actionsAvailable();
}

SelectionContext::Hit AreaSelector::LayerHandler::hitTest(const QPoint &virt) const
{
    switch (m_sel.hitTest(m_sel.toLocal(virt))) {
        case Handle::Interior: return Hit::Inside;
        case Handle::None:     return Hit::Outside;
        default:               return Hit::Handle;
    }
}

void AreaSelector::LayerHandler::setCursor(const QCursor &cursor)
{
    m_sel.setCursor(cursor);
}

void AreaSelector::LayerHandler::refreshCursor()
{
    if (m_sel.m_hoveredButton < 0)
        m_sel.updateCursorShape(m_sel.toLocal(m_sel.m_cursorVirt));
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
    if (s.m_layer)
        s.m_layer->clear();

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
                if (s.m_layer)
                    s.m_layer->clear();
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

} // namespace Screen
