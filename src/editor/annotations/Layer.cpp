#include "Layer.h"

namespace Editor {

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
        // A group's visibility cascades to its children.
        for (Layer *child : m_children) {
            child->setVisible(visible);
        }
        emit visibilityChanged(visible);
    }
}

void Layer::addChild(Layer *child)
{
    if (child && !m_children.contains(child))
        m_children.append(child);
}

void Layer::removeChild(Layer *child)
{
    m_children.removeAll(child);
}

void Layer::setItem(QGraphicsItem *item)
{
    m_item = item;
    if (m_item) {
        m_item->setVisible(m_visible);
    }
}

} // namespace Editor
