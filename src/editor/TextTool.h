#ifndef IMAGEEDITOR_TEXTTOOL_H
#define IMAGEEDITOR_TEXTTOOL_H

#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>
#include <QGraphicsSceneWheelEvent>

namespace ImageEditor {

class TextTool : public QGraphicsTextItem
{
    Q_OBJECT

public:
    explicit TextTool(const QString &text = "", QGraphicsItem *parent = nullptr);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;

signals:
    void textChanged();
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_TEXTTOOL_H
