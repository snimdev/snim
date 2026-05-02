#ifndef SCREEN_SELECTIONLAYER_H
#define SCREEN_SELECTIONLAYER_H

#include <QCursor>
#include <QObject>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QVariant>
#include <QVector>
#include <optional>

class QInputMethodEvent;
class QKeyEvent;
class QPainter;

namespace Screen {

/**
 * What a SelectionLayer may read from, or ask of, the area selector while handling
 * input. Points are VIRTUAL-DESKTOP LOGICAL coords.
 */
class SelectionContext
{
public:
    enum class Hit { Outside, Handle, Inside };

    virtual ~SelectionContext() = default;

    // An area selection is being adjusted and is not empty.
    [[nodiscard]] virtual bool adjusting() const = 0;
    // adjusting() and the action toolbar is offered.
    [[nodiscard]] virtual bool actionsAvailable() const = 0;
    [[nodiscard]] virtual Hit hitTest(const QPoint &virt) const = 0;
    virtual void setCursor(const QCursor &cursor) = 0;
    // Re-derive the cursor for where it is, leaving a hovered toolbar button alone.
    virtual void refreshCursor() = 0;
};

/**
 * Something drawn and edited on top of the area selector's selection, such as the
 * capture's quick annotations. A Strategy plugged into the selector (dependency
 * inversion): the screen module owns this interface, higher modules implement it,
 * so the selector never reaches up into them. Every overlay of one capture may
 * share a single layer. Coordinates are VIRTUAL-DESKTOP LOGICAL.
 */
class SelectionLayer : public QObject
{
    Q_OBJECT

public:
    enum class MouseAction { Press, Move, Release, DoubleClick };

    // One layer button on the selector's floating toolbar.
    struct ToolbarSlot {
        QString id;
        QString iconPath;        // tried first
        QString glyph;           // text fallback when the icon cannot load
        QPainterPath glyphPath;  // stroked fallback in a 24-unit box, preferred over glyph
        QString tooltip;
        int  group = 0;          // separators fall between groups
        bool checked = false;
        bool enabled = true;
    };

    using QObject::QObject;

    // Strokes stay inside this; empty means the whole desktop.
    virtual void setSelection(const QRect &virt) = 0;
    // Drops everything drawn, as a fresh selection does.
    virtual void clear() = 0;
    // Paints the layer over virtSource into target.
    virtual void render(QPainter *painter, const QRectF &target, const QRect &virtSource) = 0;

    // A tool is armed, so presses inside the selection belong to the layer.
    [[nodiscard]] virtual bool isArmed() const = 0;
    [[nodiscard]] virtual QCursor cursor() const = 0;
    // Every key and input method event goes to the layer (text being typed).
    [[nodiscard]] virtual bool capturesKeyboard() const = 0;
    virtual void forwardInputMethod(QInputMethodEvent *event) = 0;
    [[nodiscard]] virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const = 0;

    // Instruction bar text while adjusting; nullopt keeps the selector's own.
    [[nodiscard]] virtual std::optional<QString> hint() const = 0;

    [[nodiscard]] virtual QVector<ToolbarSlot> toolbarSlots() const = 0;
    virtual void activateSlot(const QString &id) = 0;

    // Chain links ahead of the selector's own selection handling; true consumes the
    // event. Esc falls through once the layer has nothing of its own to end.
    virtual bool handleKey(QKeyEvent *event, SelectionContext &context) = 0;
    // The selector refreshes its cursor after a consumed release.
    virtual bool handleMouse(MouseAction action, const QPoint &virt, Qt::MouseButton button,
                             SelectionContext &context) = 0;

signals:
    // Anything painted, the toolbar state or keyboard capture changed.
    void changed();
};

} // namespace Screen

#endif // SCREEN_SELECTIONLAYER_H
