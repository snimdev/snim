#ifndef EDITABLETEXTITEM_H
#define EDITABLETEXTITEM_H

#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>
#include <QGraphicsSceneWheelEvent>

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

#endif // EDITABLETEXTITEM_H
