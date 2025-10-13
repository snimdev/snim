#include "TextTool.h"
#include <QFont>

namespace ImageEditor::Tools {

TextTool::TextTool(const QString &text, QGraphicsItem *parent)
    : QGraphicsTextItem(text, parent)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setTextInteractionFlags(Qt::NoTextInteraction);
}

void TextTool::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFocus();
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void TextTool::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setSelected(false); // Deselect the text item when editing is finished
    emit textChanged();
    QGraphicsTextItem::focusOutEvent(event);
}

void TextTool::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier && isSelected()) {
        QFont currentFont = font();
        qreal currentSize = currentFont.pointSizeF();

        if (currentSize <= 0) {
            currentSize = currentFont.pixelSize();
            if (currentSize <= 0) {
                currentSize = 12;
            }
        }

        qreal sizeChange = event->delta() / 120.0;
        qreal newSize = currentSize + sizeChange;
        newSize = qMax(6.0, qMin(72.0, newSize));

        currentFont.setPointSizeF(newSize);
        setFont(currentFont);
        emit textChanged();
        event->accept();
    } else {
        QGraphicsTextItem::wheelEvent(event);
    }
}

} // namespace ImageEditor
