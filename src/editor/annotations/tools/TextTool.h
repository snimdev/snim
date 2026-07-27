#ifndef IMAGEEDITOR_TEXTTOOL_H
#define IMAGEEDITOR_TEXTTOOL_H

#include "ITool.h"
#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QFocusEvent>

namespace Editor::Tools {

class TextTool : public QGraphicsTextItem, public ITool
{
    Q_OBJECT

public:
    explicit TextTool(const QString &text = "", QGraphicsItem *parent = nullptr);

    [[nodiscard]] QList<ToolProperty> getProperties() const override;
    void setProperty(const QString& propertyId, const QVariant& value) override;
    [[nodiscard]] QGraphicsItem* clone() const override;
    void applyStyleFrom(const ITool* other) override;   // copies text color + font

    // Enter inline editing programmatically (same path as a double-click). Used to
    // edit a freshly-placed text box without a popup.
    void startEditing();

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

signals:
    void textChanged();
    void editingFinished();   // emitted when inline editing ends (focus lost)
};

} // namespace Editor

#endif // IMAGEEDITOR_TEXTTOOL_H
