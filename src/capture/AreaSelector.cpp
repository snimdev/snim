#include "AreaSelector.h"
#include "capture/AreaSelectorInput.h"
#include "capture/OverlayAnnotations.h"
#include "core/IconUtil.h"
#include "core/Perf.h"
#include "editor/annotations/ToolRegistry.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QCursor>
#include <QFontMetrics>
#include <QKeySequence>
#include <QShowEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QInputMethodEvent>
#include <QFile>
#include <QPolygonF>

namespace Capture {

namespace {
    constexpr int kDimAlpha = 120;            // darkening of the un-selected area
    const QColor  kAccent(0, 150, 255);       // selection / handle accent color
    constexpr int kBtnSize = 38;              // action toolbar button size
    constexpr int kBtnPad  = 6;               // padding around toolbar buttons
    constexpr int kSepGap  = 9;               // extra gap holding a group separator

    QString keyText(QKeySequence::StandardKey k)
    {
        return QKeySequence(k).toString(QKeySequence::NativeText);
    }
}

AreaSelector::AreaSelector(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint |
                   Qt::Tool | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Order is behaviour: the first handler that consumes an event wins.
    m_inputChain.push_back(std::make_unique<ToolbarHandler>(*this));
    m_inputChain.push_back(std::make_unique<TextEditingHandler>(*this));
    m_inputChain.push_back(std::make_unique<StrokeHandler>(*this));
    m_inputChain.push_back(std::make_unique<ToolHandler>(*this));
    m_inputChain.push_back(std::make_unique<SelectionHandler>(*this));
}

AreaSelector::~AreaSelector() = default;

void AreaSelector::setScreenshot(const QPixmap &screenshot)
{
    m_screenshot = screenshot;
    m_dpr = screenshot.devicePixelRatio();
    m_screenshotImage = screenshot.toImage();  // cache once; never per-frame
    m_dimmedBg = QPixmap();                     // invalidate cache
    update();
}

void AreaSelector::setAnnotations(QSharedPointer<OverlayAnnotations> session)
{
    if (m_annotations)
        m_annotations->disconnect(this);
    m_annotations = std::move(session);
    if (m_annotations) {
        connect(m_annotations.data(), &OverlayAnnotations::changed,
                this, &AreaSelector::onAnnotationsChanged);
        m_annotations->setSelection(m_selectionVirt);
    }
    update();
}

void AreaSelector::onAnnotationsChanged()
{
    // Input methods only compose for a text box; tool letters must reach us raw.
    setAttribute(Qt::WA_InputMethodEnabled, m_annotations && m_annotations->isEditingText());
    update();
}

// ---- coordinate helpers ---------------------------------------------------

QRect AreaSelector::localSelection() const
{
    return QRect(toLocal(m_selectionVirt.topLeft()), m_selectionVirt.size());
}

QRect AreaSelector::virtToSource(const QRect &virt) const
{
    QRect src(int((virt.x() - m_virtualGeometry.x()) * m_dpr),
              int((virt.y() - m_virtualGeometry.y()) * m_dpr),
              int(virt.width()  * m_dpr),
              int(virt.height() * m_dpr));
    return src.intersected(m_screenshot.rect());
}

// ---- handle hit-testing / geometry ----------------------------------------

QRect AreaSelector::handleRect(Handle h, const QRect &s) const
{
    QPoint c;
    switch (h) {
        case Handle::TopLeft:     c = s.topLeft();                          break;
        case Handle::Top:         c = QPoint(s.center().x(), s.top());      break;
        case Handle::TopRight:    c = s.topRight();                         break;
        case Handle::Right:       c = QPoint(s.right(), s.center().y());    break;
        case Handle::BottomRight: c = s.bottomRight();                      break;
        case Handle::Bottom:      c = QPoint(s.center().x(), s.bottom());   break;
        case Handle::BottomLeft:  c = s.bottomLeft();                       break;
        case Handle::Left:        c = QPoint(s.left(), s.center().y());     break;
        default:                  return QRect();
    }
    return QRect(c.x() - kHandleSize / 2, c.y() - kHandleSize / 2, kHandleSize, kHandleSize);
}

AreaSelector::Handle AreaSelector::hitTest(const QPoint &local) const
{
    const QRect s = localSelection();
    static const Handle handles[] = {
        Handle::TopLeft, Handle::Top, Handle::TopRight, Handle::Right,
        Handle::BottomRight, Handle::Bottom, Handle::BottomLeft, Handle::Left
    };
    for (Handle h : handles) {
        if (handleRect(h, s).adjusted(-kHandleHit, -kHandleHit, kHandleHit, kHandleHit).contains(local))
            return h;
    }
    if (s.contains(local))
        return Handle::Interior;
    return Handle::None;
}

void AreaSelector::setWindowInfos(const QVector<WindowInfo> &infos)
{
    m_windows.clear();
    m_windowIds.clear();
    m_windows.reserve(infos.size());
    m_windowIds.reserve(infos.size());
    for (const WindowInfo &info : infos) {
        m_windows.append(info.rect);
        m_windowIds.append(info.id);
    }
}

void AreaSelector::updateHoverWindow()
{
    // Topmost window under the cursor (front-to-back: first hit wins), tracking its
    // id so a pick can carry it for true window capture. Falls back to this screen.
    m_hoverWindowId = 0;
    QRect w;
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].contains(m_cursorVirt)) {
            w = m_windows[i];
            m_hoverWindowId = m_windowIds.value(i, 0);
            break;
        }
    }
    if (w.isEmpty())
        w = QRect(m_screenOffset, size()); // fallback: highlight this screen
    m_selectionVirt = w;                   // reuse selection rect for rendering/HUD
}

// ---- action toolbar --------------------------------------------------------

bool AreaSelector::actionsAvailable() const
{
    return m_actionsEnabled && m_mode == Mode::AreaSelect &&
           m_phase == Phase::Adjusting && !m_selectionVirt.isEmpty();
}

bool AreaSelector::toolbarVisible() const
{
    if (!actionsAvailable())
        return false;
    // Draw (and hit-test) the toolbar on exactly one overlay: the screen that
    // contains its anchor (the selection's bottom-center), so a spanning
    // selection doesn't show duplicate toolbars across monitors.
    const QPoint anchor(m_selectionVirt.center().x(), m_selectionVirt.bottom());
    return QRect(m_screenOffset, size()).contains(anchor);
}

QVector<AreaSelector::BarSlot> AreaSelector::toolbarSlots(QRect *barOut) const
{
    // Groups in bar order; tools and history only exist with a session.
    QVector<QVector<BarSlot>> groups;
    if (m_annotations) {
        QVector<BarSlot> tools;
        for (QLatin1StringView id : kOverlayTools)
            tools.append({BarSlot::Kind::Tool, QString(id), -1, {}});
        groups.append(tools);
        groups.append({{BarSlot::Kind::Undo, {}, -1, {}}, {BarSlot::Kind::Redo, {}, -1, {}}});
    }
    QVector<BarSlot> actions;
    for (int i = 0; i < BtnCount; ++i)
        actions.append({BarSlot::Kind::Action, {}, i, {}});
    groups.append(actions);

    const auto rowWidth = [&groups](int from, int to) {
        int n = 0;
        for (int g = from; g < to; ++g)
            n += int(groups[g].size());
        return kBtnPad + n * (kBtnSize + kBtnPad) + (to - from - 1) * kSepGap;
    };
    const int count = int(groups.size());
    QVector<QPair<int, int>> rows{{0, count}};
    if (count > 1 && rowWidth(0, count) > width() - 8)
        rows = {{0, count - 1}, {count - 1, count}};   // actions on their own row

    int w = 0;
    for (const auto &row : rows)
        w = qMax(w, rowWidth(row.first, row.second));
    const int h = int(rows.size()) * (kBtnSize + kBtnPad) + kBtnPad;
    const QRect sel = localSelection();

    int x = sel.center().x() - w / 2;
    x = qBound(4, x, qMax(4, width() - w - 4));

    int y = sel.bottom() + 12;              // below the selection by default
    if (y + h > height() - 4)
        y = sel.top() - 12 - h;             // flip above if no room below
    if (y < 4)
        y = qBound(4, sel.bottom() + 12, qMax(4, height() - h - 4));
    const QRect bar(x, y, w, h);
    if (barOut)
        *barOut = bar;

    QVector<BarSlot> out;
    int top = bar.top() + kBtnPad;
    for (const auto &row : rows) {
        int left = bar.left() + (w - rowWidth(row.first, row.second)) / 2 + kBtnPad;
        for (int g = row.first; g < row.second; ++g) {
            if (g > row.first)
                left += kSepGap;
            for (BarSlot slot : groups[g]) {
                slot.rect = QRect(left, top, kBtnSize, kBtnSize);
                out.append(slot);
                left += kBtnSize + kBtnPad;
            }
        }
        top += kBtnSize + kBtnPad;
    }
    return out;
}

int AreaSelector::toolbarButtonAt(const QPoint &local) const
{
    if (!toolbarVisible()) return -1;
    const QVector<BarSlot> bar = toolbarSlots();
    for (int i = 0; i < bar.size(); ++i)
        if (bar[i].rect.contains(local)) return i;
    return -1;
}

void AreaSelector::triggerToolbarButton(int index)
{
    switch (index) {
        case BtnEdit:   commitSelection();                       break; // -> editor
        case BtnCopy:   emit copyRequested(m_selectionVirt); close(); break;
        case BtnSave:   emit saveRequested(m_selectionVirt); close(); break;
        case BtnCancel: cancel();                                break;
        default: break;
    }
}

void AreaSelector::activateBarSlot(const BarSlot &slot)
{
    switch (slot.kind) {
        case BarSlot::Kind::Tool:   toggleTool(slot.toolId);             break;
        case BarSlot::Kind::Undo:   m_annotations->undo();               break;
        case BarSlot::Kind::Redo:   m_annotations->redo();               break;
        case BarSlot::Kind::Action: triggerToolbarButton(slot.action);   break;
    }
}

void AreaSelector::toggleTool(const QString &id)
{
    m_annotations->setActiveTool(m_annotations->activeTool() == id ? QString() : id);
}

QIcon AreaSelector::barIcon(const QString &path)
{
    auto it = m_barIcons.constFind(path);
    if (it == m_barIcons.constEnd()) {
        // Without the qrc (tests) the glyph falls back to hand painting.
        const QIcon icon = QFile::exists(path) ? Core::themedSvgIcon(path, Qt::white, 20) : QIcon();
        it = m_barIcons.insert(path, icon);
    }
    return *it;
}

QString AreaSelector::barTooltip(const BarSlot &slot) const
{
    switch (slot.kind) {
        case BarSlot::Kind::Tool:
            if (const Editor::ToolSpec *spec = Editor::ToolRegistry::find(slot.toolId))
                return QStringLiteral("%1 (%2)").arg(spec->tooltip, QString(spec->shortcut));
            return {};
        case BarSlot::Kind::Undo: return QStringLiteral("Undo (%1)").arg(keyText(QKeySequence::Undo));
        case BarSlot::Kind::Redo: return QStringLiteral("Redo (%1)").arg(keyText(QKeySequence::Redo));
        case BarSlot::Kind::Action: break;
    }
    switch (slot.action) {
        case BtnEdit:   return QStringLiteral("Open in editor (Enter)");
        case BtnCopy:   return QStringLiteral("Copy to clipboard (%1)").arg(keyText(QKeySequence::Copy));
        case BtnSave:   return QStringLiteral("Save to file… (%1)").arg(keyText(QKeySequence::Save));
        case BtnCancel: return QStringLiteral("Cancel (Esc)");
        default:        return {};
    }
}

void AreaSelector::paintBarGlyph(QPainter &p, const BarSlot &slot, const QRect &r)
{
    const QColor glyphColor = (slot.kind == BarSlot::Kind::Action && slot.action == BtnCancel)
                              ? QColor(255, 120, 120) : QColor(Qt::white);
    p.setPen(QPen(glyphColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const QRectF g = QRectF(r).adjusted(11, 11, -11, -11); // glyph box
    QRect iconRect(0, 0, 20, 20);
    iconRect.moveCenter(r.center());

    if (slot.kind == BarSlot::Kind::Tool) {
        const Editor::ToolSpec *spec = Editor::ToolRegistry::find(slot.toolId);
        const QIcon icon = spec ? barIcon(spec->iconPath) : QIcon();
        if (!icon.isNull()) {
            icon.paint(&p, iconRect);
        } else if (spec) {
            p.save();
            QFont f = p.font();
            f.setPointSize(11);
            f.setBold(true);
            p.setFont(f);
            p.drawText(r, Qt::AlignCenter, QString(spec->shortcut));
            p.restore();
        }
        return;
    }

    if (slot.kind == BarSlot::Kind::Undo || slot.kind == BarSlot::Kind::Redo) {
        const bool redo = slot.kind == BarSlot::Kind::Redo;
        const QIcon icon = barIcon(redo ? QStringLiteral(":/icons/icons/redo.svg")
                                        : QStringLiteral(":/icons/icons/undo.svg"));
        if (!icon.isNull()) {
            icon.paint(&p, iconRect);
            return;
        }
        // Hook arrow in the SVG's 24-unit box, mirrored for redo.
        p.save();
        p.translate(QRectF(r).center());
        p.scale(redo ? -20.0 / 24.0 : 20.0 / 24.0, 20.0 / 24.0);
        p.translate(-12, -12);
        p.drawPolyline(QPolygonF{QPointF(9, 7), QPointF(4, 12), QPointF(9, 17)});
        QPainterPath tail;
        tail.moveTo(4, 12);
        tail.lineTo(15, 12);
        tail.cubicTo(17.76, 12, 20, 14.24, 20, 17);
        tail.lineTo(20, 18);
        p.drawPath(tail);
        p.restore();
        return;
    }

    switch (slot.action) {
        case BtnEdit: {            // checkmark
            QPolygonF chk;
            chk << QPointF(g.left(), g.center().y())
                << QPointF(g.left() + g.width() * 0.38, g.bottom())
                << QPointF(g.right(), g.top());
            p.drawPolyline(chk);
            break;
        }
        case BtnCopy: {            // two stacked rounded rects
            QRectF back(g.left() + g.width() * 0.25, g.top(),
                        g.width() * 0.75, g.height() * 0.75);
            QRectF front(g.left(), g.top() + g.height() * 0.25,
                         g.width() * 0.75, g.height() * 0.75);
            p.drawRoundedRect(back, 2, 2);
            p.drawRoundedRect(front, 2, 2);
            break;
        }
        case BtnSave: {            // down arrow into a tray
            const qreal cx = g.center().x();
            p.drawLine(QPointF(cx, g.top()), QPointF(cx, g.bottom() - g.height() * 0.30));
            QPolygonF arrow;
            arrow << QPointF(cx - g.width() * 0.24, g.bottom() - g.height() * 0.48)
                  << QPointF(cx, g.bottom() - g.height() * 0.18)
                  << QPointF(cx + g.width() * 0.24, g.bottom() - g.height() * 0.48);
            p.drawPolyline(arrow);
            p.drawLine(QPointF(g.left(), g.bottom()), QPointF(g.right(), g.bottom()));
            break;
        }
        case BtnCancel: {          // X
            p.drawLine(g.topLeft(), g.bottomRight());
            p.drawLine(g.topRight(), g.bottomLeft());
            break;
        }
        default: break;
    }
}

void AreaSelector::paintToolbar(QPainter &p)
{
    QRect bar;
    const QVector<BarSlot> buttons = toolbarSlots(&bar);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath bg;
    bg.addRoundedRect(bar, 9, 9);
    p.fillPath(bg, QColor(28, 28, 30, 235));
    p.setPen(QPen(QColor(255, 255, 255, 40), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bg);

    const QString armedTool = m_annotations ? m_annotations->activeTool() : QString();
    const auto group = [](const BarSlot &slot) {
        return slot.kind == BarSlot::Kind::Redo ? int(BarSlot::Kind::Undo) : int(slot.kind);
    };

    for (int i = 0; i < buttons.size(); ++i) {
        const BarSlot &slot = buttons[i];
        const QRect r = slot.rect;

        if (i > 0 && buttons[i - 1].rect.top() == r.top() && group(buttons[i - 1]) != group(slot)) {
            const qreal sx = (buttons[i - 1].rect.right() + r.left() + 1) / 2.0;
            p.setPen(QPen(QColor(255, 255, 255, 50), 1));
            p.drawLine(QPointF(sx, r.top() + 8), QPointF(sx, r.bottom() - 8));
        }

        bool enabled = true;
        if (slot.kind == BarSlot::Kind::Undo) enabled = m_annotations->canUndo();
        if (slot.kind == BarSlot::Kind::Redo) enabled = m_annotations->canRedo();
        const bool primary = (slot.kind == BarSlot::Kind::Action && slot.action == BtnEdit) ||
                             (slot.kind == BarSlot::Kind::Tool && slot.toolId == armedTool);
        const bool hovered = enabled && (i == m_hoveredButton);

        if (primary || hovered) {
            QPainterPath hp;
            hp.addRoundedRect(r.adjusted(2, 2, -2, -2), 6, 6);
            QColor fill = primary ? kAccent : QColor(255, 255, 255, 38);
            if (primary && hovered) fill = fill.lighter(120);
            p.fillPath(hp, fill);
        }

        p.setOpacity(enabled ? 1.0 : 0.35);
        paintBarGlyph(p, slot, r);
        p.setOpacity(1.0);
    }

    // Hover tooltip for the focused button (painted ourselves; a real QToolTip
    // would appear behind this shielding-level overlay window).
    if (m_hoveredButton >= 0 && m_hoveredButton < buttons.size()) {
        const QString label = barTooltip(buttons[m_hoveredButton]);
        if (!label.isEmpty()) {
            QFont f = p.font();
            f.setPointSize(10);
            p.setFont(f);
            const QRect btn = buttons[m_hoveredButton].rect;
            QRect tip = p.fontMetrics().boundingRect(label).adjusted(-8, -4, 8, 4);
            tip.moveCenter(QPoint(btn.center().x(), bar.top() - 6 - tip.height() / 2));
            if (tip.top() < 4)             tip.moveTop(bar.bottom() + 6); // no room above -> below
            if (tip.right() > width() - 4) tip.moveRight(width() - 4);
            if (tip.left() < 4)            tip.moveLeft(4);

            QPainterPath tipPath;
            tipPath.addRoundedRect(tip, 5, 5);
            p.fillPath(tipPath, QColor(20, 20, 22, 240));
            p.setPen(QPen(QColor(255, 255, 255, 45), 1));
            p.setBrush(Qt::NoBrush);
            p.drawPath(tipPath);
            p.setPen(Qt::white);
            p.drawText(tip, Qt::AlignCenter, label);
        }
    }
}

void AreaSelector::updateCursorShape(const QPoint &local)
{
    if (m_phase != Phase::Adjusting) { setCursor(Qt::CrossCursor); return; }
    switch (hitTest(local)) {
        case Handle::TopLeft:
        case Handle::BottomRight: setCursor(Qt::SizeFDiagCursor); break;
        case Handle::TopRight:
        case Handle::BottomLeft:  setCursor(Qt::SizeBDiagCursor); break;
        case Handle::Top:
        case Handle::Bottom:      setCursor(Qt::SizeVerCursor);   break;
        case Handle::Left:
        case Handle::Right:       setCursor(Qt::SizeHorCursor);   break;
        case Handle::Interior:
            if (toolArmed()) setCursor(m_annotations->cursor());
            else             setCursor(Qt::SizeAllCursor);
            break;
        default:                  setCursor(Qt::CrossCursor);     break;
    }
}

bool AreaSelector::toolArmed() const
{
    return m_annotations && !m_annotations->activeTool().isEmpty();
}

void AreaSelector::applyHandleDrag(const QPoint &c)
{
    if (m_activeHandle == Handle::Interior) {
        QRect r = m_selectionVirt;
        r.moveTopLeft(c - m_moveGrabOffsetVirt);
        m_selectionVirt = r;
        return;
    }
    const QRect b = m_resizeBaseVirt;
    int L = b.left(), T = b.top(), R = b.right(), B = b.bottom();
    switch (m_activeHandle) {
        case Handle::TopLeft:     L = c.x(); T = c.y(); break;
        case Handle::Top:         T = c.y();            break;
        case Handle::TopRight:    R = c.x(); T = c.y(); break;
        case Handle::Right:       R = c.x();            break;
        case Handle::BottomRight: R = c.x(); B = c.y(); break;
        case Handle::Bottom:      B = c.y();            break;
        case Handle::BottomLeft:  L = c.x(); B = c.y(); break;
        case Handle::Left:        L = c.x();            break;
        default: break;
    }
    QRect r;
    r.setCoords(L, T, R, B);
    m_selectionVirt = r.normalized();
}

// ---- mouse / keyboard ------------------------------------------------------

template <typename Event>
bool AreaSelector::dispatchInput(bool (InputHandler::*handle)(Event *), Event *event)
{
    for (const auto &handler : m_inputChain)
        if (((*handler).*handle)(event))
            return true;
    return false;
}

void AreaSelector::mousePressEvent(QMouseEvent *event)
{
    if (!dispatchInput(&InputHandler::mousePress, event))
        QWidget::mousePressEvent(event);
}

void AreaSelector::mouseMoveEvent(QMouseEvent *event)
{
    if (!dispatchInput(&InputHandler::mouseMove, event))
        QWidget::mouseMoveEvent(event);
}

void AreaSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (!dispatchInput(&InputHandler::mouseRelease, event))
        QWidget::mouseReleaseEvent(event);
}

void AreaSelector::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!dispatchInput(&InputHandler::mouseDoubleClick, event))
        QWidget::mouseDoubleClickEvent(event);
}

void AreaSelector::keyPressEvent(QKeyEvent *event)
{
    if (!dispatchInput(&InputHandler::keyPress, event))
        QWidget::keyPressEvent(event);
}

void AreaSelector::inputMethodEvent(QInputMethodEvent *event)
{
    if (m_annotations && m_annotations->isEditingText())
        m_annotations->forwardInputMethod(event);
    else
        QWidget::inputMethodEvent(event);
}

QVariant AreaSelector::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImEnabled || !m_annotations || !m_annotations->isEditingText())
        return QWidget::inputMethodQuery(query);
    const QVariant value = m_annotations->inputMethodQuery(query);
    if (value.typeId() == QMetaType::QRectF)
        return value.toRectF().translated(-m_screenOffset);
    return value;
}

void AreaSelector::commitSelection()
{
    m_phase = Phase::Idle;
    // Emit the window id FIRST (before areaSelected) so a recording consumer can act
    // on windowPicked and tear the overlay down; the later areaSelected is then
    // suppressed by that teardown. The screenshot path ignores windowPicked and
    // still receives areaSelected normally.
    if (m_mode == Mode::WindowPick)
        emit windowPicked(m_selectionVirt, m_hoverWindowId);
    emit areaSelected(m_selectionVirt);
    close();
}

void AreaSelector::cancel()
{
    emit areaSelected(QRect()); // empty rect = cancellation
    close();
}

bool AreaSelector::commitCurrentSelection()
{
    if (m_phase != Phase::Adjusting || m_selectionVirt.isEmpty())
        return false;
    commitSelection();
    return true;
}

void AreaSelector::broadcastState()
{
    if (m_annotations)
        m_annotations->setSelection(m_selectionVirt);
    emit liveStateChanged(m_selectionVirt, static_cast<int>(m_phase),
                          static_cast<int>(m_mode), m_cursorVirt);
}

void AreaSelector::applyPeerState(const QRect &selectionVirt, int phase, int mode,
                                  const QPoint &cursorVirt)
{
    // Mirror a peer overlay (another monitor) so this one renders its portion of a
    // spanning selection. Interaction members (m_activeHandle, etc.) are untouched;
    // only the active overlay drives input.
    m_selectionVirt = selectionVirt;
    m_phase = static_cast<Phase>(phase);
    m_mode = static_cast<Mode>(mode);
    m_cursorVirt = cursorVirt;
    if (m_annotations)
        m_annotations->setSelection(m_selectionVirt);
    update();
}

// ---- painting --------------------------------------------------------------

void AreaSelector::rebuildBackgroundCache()
{
    if (m_screenshot.isNull() || width() <= 0 || height() <= 0) {
        m_dimmedBg = QPixmap();
        return;
    }
    const qreal wdpr = devicePixelRatioF();
    QPixmap bg(QSize(int(width() * wdpr), int(height() * wdpr)));
    bg.setDevicePixelRatio(wdpr);
    bg.fill(Qt::transparent);

    QPainter p(&bg);
    const QRect src = virtToSource(QRect(toVirt(QPoint(0, 0)), size()));
    p.drawPixmap(QRect(0, 0, width(), height()), m_screenshot, src);
    p.fillRect(QRect(0, 0, width(), height()), QColor(0, 0, 0, kDimAlpha));
    m_dimmedBg = bg;
}

void AreaSelector::paintBackground(QPainter &p)
{
    if (m_dimmedBg.isNull())
        rebuildBackgroundCache();

    if (!m_dimmedBg.isNull()) {
        p.drawPixmap(0, 0, m_dimmedBg);
    } else {
        const QRect src = virtToSource(QRect(toVirt(QPoint(0, 0)), size()));
        p.drawPixmap(rect(), m_screenshot, src);
        p.fillRect(rect(), QColor(0, 0, 0, kDimAlpha));
    }
}

void AreaSelector::paintSelection(QPainter &p)
{
    const QRect localSel = localSelection();
    if (localSel.width() < 1 || localSel.height() < 1) return;

    // restore full brightness inside the selection
    p.drawPixmap(localSel, m_screenshot, virtToSource(m_selectionVirt));

    if (m_annotations) {
        // Every overlay renders its own screen's part of the one shared scene.
        p.save();
        p.setClipRect(localSel);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        m_annotations->render(&p, QRectF(rect()), QRect(m_screenOffset, size()));
        p.restore();
    }

    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(kAccent, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(localSel.adjusted(0, 0, -1, -1));

    if (m_phase == Phase::Adjusting)
        paintHandles(p, localSel);
}

void AreaSelector::paintHandles(QPainter &p, const QRect &localSel)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(Qt::white, 1));
    p.setBrush(kAccent);
    static const Handle handles[] = {
        Handle::TopLeft, Handle::Top, Handle::TopRight, Handle::Right,
        Handle::BottomRight, Handle::Bottom, Handle::BottomLeft, Handle::Left
    };
    for (Handle h : handles)
        p.drawRect(handleRect(h, localSel));
}

void AreaSelector::paintCrosshair(QPainter &p)
{
    if (!cursorOnThisScreen()) return;
    const QPoint c = toLocal(m_cursorVirt);
    if (!rect().contains(c)) return;
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(QColor(255, 255, 255, 120), 1));
    p.drawLine(0, c.y(), width(), c.y());
    p.drawLine(c.x(), 0, c.x(), height());
}

void AreaSelector::paintMagnifier(QPainter &p)
{
    const QPoint cLocal = toLocal(m_cursorVirt);
    if (!rect().contains(cLocal)) return;

    constexpr int loupe      = 130;  // loupe size in logical px
    constexpr int srcLogical = 17;   // logical px sampled across the loupe (odd -> center pixel)
    constexpr int gap        = 24;

    QPoint tl(cLocal.x() + gap, cLocal.y() + gap);
    if (tl.x() + loupe > width())  tl.setX(cLocal.x() - gap - loupe);
    if (tl.y() + loupe > height()) tl.setY(cLocal.y() - gap - loupe);
    const QRect loupeRect(tl, QSize(loupe, loupe));

    const int srcPx = int(srcLogical * m_dpr);
    const QPoint centerPhys(int((m_cursorVirt.x() - m_virtualGeometry.x()) * m_dpr),
                            int((m_cursorVirt.y() - m_virtualGeometry.y()) * m_dpr));
    const QRect srcRect(centerPhys.x() - srcPx / 2, centerPhys.y() - srcPx / 2, srcPx, srcPx);

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath clip;
    clip.addRoundedRect(loupeRect, 8, 8);
    p.setClipPath(clip);
    p.fillRect(loupeRect, Qt::black);

    const QRect validSrc = srcRect.intersected(m_screenshot.rect());
    if (!validSrc.isEmpty()) {
        const double scale = double(loupe) / double(srcPx);
        const QRectF dst(loupeRect.x() + (validSrc.x() - srcRect.x()) * scale,
                         loupeRect.y() + (validSrc.y() - srcRect.y()) * scale,
                         validSrc.width()  * scale,
                         validSrc.height() * scale);
        p.drawPixmap(dst, m_screenshot, validSrc);
    }
    p.setClipping(false);

    // center-pixel marker
    const double cell = double(loupe) / double(srcLogical);
    const QRectF centerCell(loupeRect.center().x() - cell / 2.0,
                            loupeRect.center().y() - cell / 2.0, cell, cell);
    p.setPen(QPen(kAccent, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(centerCell);

    // crosshair + border
    p.setPen(QPen(QColor(255, 255, 255, 160), 1));
    p.drawLine(loupeRect.left(), loupeRect.center().y(), loupeRect.right(), loupeRect.center().y());
    p.drawLine(loupeRect.center().x(), loupeRect.top(), loupeRect.center().x(), loupeRect.bottom());
    p.setPen(QPen(QColor(255, 255, 255, 220), 2));
    p.drawRoundedRect(loupeRect, 8, 8);

    // hex color + coordinate readout under the loupe
    const bool inImg = centerPhys.x() >= 0 && centerPhys.y() >= 0 &&
                       centerPhys.x() < m_screenshotImage.width() &&
                       centerPhys.y() < m_screenshotImage.height();
    const QColor col = inImg ? m_screenshotImage.pixelColor(centerPhys) : QColor();
    const QString hex = col.isValid()
        ? QString("#%1").arg(col.rgb() & 0xFFFFFF, 6, 16, QChar('0')).toUpper()
        : QStringLiteral("--");
    const QString info = QString("%1   %2, %3").arg(hex)
        .arg(m_cursorVirt.x()).arg(m_cursorVirt.y());

    QFont f = p.font();
    f.setPointSize(10);
    p.setFont(f);
    QRect ib = p.fontMetrics().boundingRect(info).adjusted(-8, -4, 8, 4);
    ib.moveTopLeft(QPoint(loupeRect.left(), loupeRect.bottom() + 6));
    if (ib.right() > width())   ib.moveRight(width() - 2);
    if (ib.bottom() > height()) ib.moveBottom(loupeRect.top() - 6);
    QPainterPath ibp;
    ibp.addRoundedRect(ib, 4, 4);
    p.fillPath(ibp, QColor(0, 0, 0, 205));
    p.setPen(Qt::white);
    p.drawText(ib, Qt::AlignCenter, info);
    p.restore();
}

void AreaSelector::paintDimensionHud(QPainter &p)
{
    if (m_selectionVirt.isEmpty()) return;
    const QString text = QString("%1 × %2")
        .arg(m_selectionVirt.width()).arg(m_selectionVirt.height());

    p.setRenderHint(QPainter::Antialiasing, true);
    QFont f = p.font();
    f.setPointSize(11);
    f.setBold(true);
    p.setFont(f);

    const QRect localSel = localSelection();
    QRect badge = p.fontMetrics().boundingRect(text).adjusted(-8, -4, 8, 4);
    if (m_phase == Phase::Adjusting) {
        // Sit above the selection so it doesn't collide with the toolbar below.
        badge.moveBottomLeft(QPoint(localSel.left(), localSel.top() - 8));
        if (badge.top() < 4) badge.moveTop(localSel.top() + 8);
    } else {
        badge.moveTopLeft(QPoint(localSel.left(), localSel.bottom() + 8));
        if (badge.bottom() > height() - 4) badge.moveBottom(localSel.top() - 8);
        if (badge.top() < 4)               badge.moveTop(localSel.top() + 8);
    }
    if (badge.right() > width() - 4)   badge.moveRight(width() - 4);
    if (badge.left() < 4)              badge.moveLeft(4);

    QPainterPath path;
    path.addRoundedRect(badge, 4, 4);
    p.fillPath(path, QColor(0, 0, 0, 200));
    p.setPen(Qt::white);
    p.drawText(badge, Qt::AlignCenter, text);
}

void AreaSelector::paintInstructions(QPainter &p)
{
    QString text;
    if (m_mode == Mode::WindowPick) {
        text = QStringLiteral("Click a window to capture  ·  Esc to cancel");
    } else {
        switch (m_phase) {
            case Phase::Idle:      text = QStringLiteral("Drag to select  ·  Esc to cancel"); break;
            case Phase::Dragging:  text = QStringLiteral("Release to adjust"); break;
            case Phase::Adjusting:
                if (m_annotations && m_annotations->isEditingText()) {
                    text = QStringLiteral("Type your text  ·  Esc to finish");
                    break;
                }
                if (toolArmed()) {
                    text = QStringLiteral("Drag to draw  ·  %1 to undo  ·  Esc to stop drawing")
                               .arg(keyText(QKeySequence::Undo));
                    break;
                }
                text = m_actionsEnabled
                       ? QStringLiteral("Click inside or ✓ to capture  ·  %1 to copy  ·  %2 to save  ·  Esc to cancel")
                             .arg(keyText(QKeySequence::Copy), keyText(QKeySequence::Save))
                       : QStringLiteral("Click inside or ✓ to capture  ·  drag handles to adjust  ·  Esc to cancel");
                break;
        }
    }
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont f = p.font();
    f.setPointSize(12);
    p.setFont(f);

    QRect tb = p.fontMetrics().boundingRect(text);
    tb.moveCenter(QPoint(width() / 2, 34));
    QRect bg = tb.adjusted(-12, -7, 12, 7);
    QPainterPath path;
    path.addRoundedRect(bg, 6, 6);
    p.fillPath(path, QColor(0, 0, 0, 180));
    p.setPen(Qt::white);
    p.drawText(tb, Qt::AlignCenter, text);
}

void AreaSelector::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (m_screenshot.isNull()) {
        painter.fillRect(rect(), QColor(0, 0, 0, 150));
        return;
    }

    paintBackground(painter);

    if (m_mode == Mode::WindowPick) {
        if (!m_selectionVirt.isEmpty()) {
            paintSelection(painter);     // bright highlight + border (no handles in Idle)
            paintDimensionHud(painter);
        }
        paintInstructions(painter);
        return;
    }

    const bool haveSel = (m_phase != Phase::Idle) && !m_selectionVirt.isEmpty();
    if (haveSel)
        paintSelection(painter);

    if (m_phase == Phase::Idle || m_phase == Phase::Dragging)
        paintCrosshair(painter);

    if (haveSel)
        paintDimensionHud(painter);

    if (cursorOnThisScreen() && m_phase != Phase::Adjusting)
        paintMagnifier(painter);

    if (toolbarVisible())
        paintToolbar(painter);

    paintInstructions(painter);
}

// ---- lifecycle -------------------------------------------------------------

void AreaSelector::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    activateWindow();
    setFocus();
    m_cursorVirt = QCursor::pos();   // global == virtual-desktop logical
    m_hasCursor = rect().contains(toLocal(m_cursorVirt));
    if (m_mode == Mode::WindowPick)
        updateHoverWindow();
    rebuildBackgroundCache();
    update();
    Core::Perf::reportCaptureShown("overlay shown");
}

void AreaSelector::resizeEvent(QResizeEvent *event)
{
    m_dimmedBg = QPixmap(); // size changed -> rebuild lazily on next paint
    QWidget::resizeEvent(event);
}

void AreaSelector::leaveEvent(QEvent *event)
{
    if (m_phase != Phase::Dragging) {
        m_hasCursor = false;
        update();
    }
    QWidget::leaveEvent(event);
}

} // namespace Capture
