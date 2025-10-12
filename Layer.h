#ifndef LAYER_H
#define LAYER_H

#include <QObject>
#include <QGraphicsItem>

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

signals:
    void visibilityChanged(bool visible);
    void nameChanged(const QString &name);

private:
    QString m_name;
    bool m_visible;
    LayerType m_type;
    QGraphicsItem *m_item;
};

#endif // LAYER_H
