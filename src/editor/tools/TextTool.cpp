#include "TextTool.h"
#include <QFont>
#include <QFontDatabase>
#include <QColor>

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

QList<ToolProperty> TextTool::getProperties() const
{
    QList<ToolProperty> properties;

    ToolProperty colorProp;
    colorProp.id = "color";
    colorProp.name = "Color";
    colorProp.value = defaultTextColor();
    colorProp.controlType = "color";
    properties.append(colorProp);

    const QFont f = font();

    ToolProperty familyProp;
    familyProp.id = "fontFamily";
    familyProp.name = "Font";
    familyProp.controlType = "dropdown";
    familyProp.options["items"] = QFontDatabase::families();
    familyProp.value = f.family();
    properties.append(familyProp);

    ToolProperty sizeProp;
    sizeProp.id = "fontSize";
    sizeProp.name = "Size";
    sizeProp.controlType = "slider";
    sizeProp.options["min"] = 6;
    sizeProp.options["max"] = 96;
    int size = f.pointSize();
    if (size <= 0)
        size = (f.pixelSize() > 0 ? f.pixelSize() : 12);
    sizeProp.value = size;
    properties.append(sizeProp);

    ToolProperty styleProp;
    styleProp.id = "fontStyle";
    styleProp.name = "Style";
    styleProp.controlType = "dropdown";
    styleProp.options["items"] = QStringList{"Regular", "Bold", "Italic", "Bold Italic"};
    styleProp.value = f.bold() ? (f.italic() ? "Bold Italic" : "Bold")
                               : (f.italic() ? "Italic" : "Regular");
    properties.append(styleProp);

    return properties;
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
