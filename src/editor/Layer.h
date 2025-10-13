#ifndef IMAGEEDITOR_LAYER_H
#define IMAGEEDITOR_LAYER_H

#include <QObject>
#include <QGraphicsItem>

namespace ImageEditor {

class Layer : public QObject
{
    Q_OBJECT

public:
    enum LayerType {
        Background,
        Arrow,
        Text
    };

    explicit Layer(const QString &name, LayerType type, QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);

    LayerType type() const { return m_type; }

    QGraphicsItem* item() const { return m_item; }
    void setItem(QGraphicsItem *item);

    QGraphicsItem* getTool() const;

signals:
    void visibilityChanged(bool visible);
    void nameChanged(const QString &name);

private:
    QString m_name;
    bool m_visible;
    LayerType m_type;
    QGraphicsItem *m_item;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_LAYER_H
