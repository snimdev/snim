#include "Layer.h"

Layer::Layer(const QString &name, LayerType type, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_visible(true)
    , m_type(type)
    , m_item(nullptr)
{
}

void Layer::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        if (m_item) {
            m_item->setVisible(visible);
        }
        emit visibilityChanged(visible);
    }
}

void Layer::setItem(QGraphicsItem *item)
{
    m_item = item;
    if (m_item) {
        m_item->setVisible(m_visible);
    }
}
