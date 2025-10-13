#ifndef IMAGEEDITOR_TEXTTOOL_H
#define IMAGEEDITOR_TEXTTOOL_H

#include "ITool.h"
#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>
#include <QGraphicsSceneWheelEvent>

namespace ImageEditor::Tools {

class TextTool : public QGraphicsTextItem, public ITool
{
    Q_OBJECT

public:
    explicit TextTool(const QString &text = "", QGraphicsItem *parent = nullptr);

    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;

signals:
    void textChanged();
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_TEXTTOOL_H
