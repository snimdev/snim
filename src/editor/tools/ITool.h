#ifndef ITOOL_H
#define ITOOL_H

#include <QVariant>
#include <QString>
#include <QList>
#include <QVariantMap>

class QGraphicsItem;

namespace ImageEditor::Tools {

struct ToolProperty {
    QString id;
    QString name;
    QVariant value;
    QString controlType; // e.g., "color", "slider"
    QVariantMap options; // For sliders: min, max, step. For dropdowns: items.
};

class ITool {
public:
    virtual ~ITool() = default;
    [[nodiscard]] virtual QList<ToolProperty> getProperties() const = 0;
    virtual void setProperty(const QString& propertyId, const QVariant& value) = 0;

    // A full duplicate of this tool (geometry + style). Returns the new item (caller
    // owns it) or nullptr for tools that aren't duplicable (e.g. the backdrop, of
    // which there is only one). Concrete drawing tools override.
    [[nodiscard]] virtual QGraphicsItem* clone() const { return nullptr; }

    // Copy only the *style* (pen/brush/color/font/…), never geometry, from
    // another tool of the same kind. Used to seed a freshly-drawn item from the
    // active tool template. No-op by default; same-kind tools override.
    virtual void applyStyleFrom(const ITool* /*other*/) {}
};

} // namespace ImageEditor::Tools

#endif // ITOOL_H

