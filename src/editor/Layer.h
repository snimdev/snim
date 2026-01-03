#ifndef IMAGEEDITOR_LAYER_H
#define IMAGEEDITOR_LAYER_H

#include <QObject>
#include <QGraphicsItem>
#include <QList>

namespace ImageEditor {

/**
 * A layer is either a leaf (wraps a single QGraphicsItem) or a group (type Group,
 * wraps no item but contains child layers). Both are the one Layer class (groups just
 * add child-management ops on the base), so the rest of the editor treats everything
 * as Layer*. Shared operations like visibility cascade to a group's children.
 */
class Layer : public QObject
{
    Q_OBJECT

public:
    enum LayerType {
        Background,
        Backdrop,   // CleanShot-style beautify background (behind Background)
        Group,      // contains child layers, has no QGraphicsItem
        Arrow,
        Text,
        Rectangle,
        Ellipse,
        Freehand,
        Highlight,
        Blur
    };

    explicit Layer(const QString &name, LayerType type, QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);   // cascades to children when this is a group

    LayerType type() const { return m_type; }
    [[nodiscard]] bool isGroup() const { return m_type == Group; }

    QGraphicsItem* item() const { return m_item; }
    void setItem(QGraphicsItem *item);

    QGraphicsItem* getTool() const;

    // Child management (meaningful for Group layers).
    void addChild(Layer *child);
    void removeChild(Layer *child);
    [[nodiscard]] QList<Layer*> children() const { return m_children; }

signals:
    void visibilityChanged(bool visible);
    void nameChanged(const QString &name);

private:
    QString m_name;
    bool m_visible;
    LayerType m_type;
    QGraphicsItem *m_item;
    QList<Layer*> m_children;   // non-empty only for Group layers
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_LAYER_H
