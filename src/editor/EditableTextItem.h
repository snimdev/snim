#ifndef IMAGEEDITOR_EDITABLETEXTITEM_H
#define IMAGEEDITOR_EDITABLETEXTITEM_H

#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>
#include <QGraphicsSceneWheelEvent>

namespace ImageEditor {

class EditableTextItem : public QGraphicsTextItem
{
    Q_OBJECT

public:
    explicit EditableTextItem(const QString &text = "", QGraphicsItem *parent = nullptr);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;

signals:
    void textChanged();
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_EDITABLETEXTITEM_H
