#include "TextTool.h"
#include <QFont>
#include <QFontDatabase>
#include <QColor>

namespace Editor::Tools {

TextTool::TextTool(const QString &text, QGraphicsItem *parent)
    : QGraphicsTextItem(text, parent)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setTextInteractionFlags(Qt::NoTextInteraction);
}

QList<ToolProperty> TextTool::getProperties() const
{
    const QFont f = font();
    int size = f.pointSize();
    if (size <= 0)
        size = (f.pixelSize() > 0 ? f.pixelSize() : 12);
    const QString style = f.bold() ? (f.italic() ? "Bold Italic" : "Bold")
                                   : (f.italic() ? "Italic" : "Regular");
    return {
        {"color", "Color", defaultTextColor(), "color"},
        {"fontFamily", "Font", f.family(), "dropdown", {{"items", QFontDatabase::families()}}},
        {"fontSize", "Size", size, "slider", {{"min", 6}, {"max", 96}}},
        {"fontStyle", "Style", style, "dropdown",
         {{"items", QStringList{"Regular", "Bold", "Italic", "Bold Italic"}}}},
    };
}

void TextTool::setProperty(const QString& propertyId, const QVariant& value)
{
    if (propertyId == "color") {
        if (value.canConvert<QColor>()) {
            setDefaultTextColor(value.value<QColor>());
            emit textChanged();
        }
        return;
    }

    QFont f = font();
    if (propertyId == "fontFamily") {
        f.setFamily(value.toString());
    } else if (propertyId == "fontSize") {
        f.setPointSize(qMax(1, value.toInt()));
    } else if (propertyId == "fontStyle") {
        const QString style = value.toString();
        f.setBold(style.contains("Bold"));
        f.setItalic(style.contains("Italic"));
    } else {
        return;
    }
    setFont(f);
    emit textChanged();
}

QGraphicsItem* TextTool::clone() const
{
    auto* copy = new TextTool(toPlainText());
    copy->applyStyleFrom(this);
    return copy;
}

void TextTool::applyStyleFrom(const ITool* other)
{
    if (const auto* o = dynamic_cast<const TextTool*>(other)) {
        setDefaultTextColor(o->defaultTextColor());
        setFont(o->font());
    }
}

void TextTool::startEditing()
{
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFocus();
}

void TextTool::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    startEditing();
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void TextTool::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setSelected(false); // Deselect the text item when editing is finished
    emit textChanged();
    emit editingFinished();
    QGraphicsTextItem::focusOutEvent(event);
}

} // namespace Editor
