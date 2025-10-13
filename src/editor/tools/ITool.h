#ifndef ITOOL_H
#define ITOOL_H

#include <QVariant>
#include <QString>
#include <QList>
#include <QVariantMap>

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
};

} // namespace ImageEditor::Tools

#endif // ITOOL_H

