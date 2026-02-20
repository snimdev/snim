#include "editor/annotations/commands/EditorCommands.h"

#include "editor/annotations/Layer.h"
#include "editor/annotations/LayerManager.h"
#include "editor/annotations/LayerProperties.h"
#include "editor/annotations/tools/ITool.h"
#include "editor/image/BackdropItem.h"

#include <QGraphicsScene>
#include <QGraphicsItem>

namespace Editor::Commands {

// ---------------------------------------------------------------- AddLayerCommand
AddLayerCommand::AddLayerCommand(QGraphicsScene *scene, LayerManager *manager,
                                 Layer *layer, const QString &text)
    : m_scene(scene), m_manager(manager), m_layer(layer)
{
    setText(text);
}

AddLayerCommand::~AddLayerCommand()
{
    // Only when the item sits off the scene is this command its owner.
    if (m_itemOffScene && m_layer && m_layer->item())
        delete m_layer->item();
}

void AddLayerCommand::redo()
{
    if (m_layer->item())
        m_scene->addItem(m_layer->item());
    m_manager->addLayer(m_layer);
    m_manager->selectLayer(m_layer);
    m_scene->clearSelection();   // use the pointer tool to re-select
    m_itemOffScene = false;
}

void AddLayerCommand::undo()
{
    if (m_layer->item())
        m_scene->removeItem(m_layer->item());
    m_manager->removeLayer(m_layer);
    m_itemOffScene = true;
}

// ----------------------------------------------------------------- AddItemCommand
AddItemCommand::AddItemCommand(QGraphicsScene *scene, QGraphicsItem *item,
                               const QString &toolId, const QString &text,
                               QUndoCommand *parent)
    : QUndoCommand(text, parent), m_scene(scene), m_item(item), m_toolId(toolId)
{
}

AddItemCommand::~AddItemCommand()
{
    // Only when the item sits off the scene is this command its owner.
    if (m_itemOffScene)
        delete m_item;
}

void AddItemCommand::redo()
{
    m_scene->addItem(m_item);
    m_itemOffScene = false;
}

void AddItemCommand::undo()
{
    m_scene->removeItem(m_item);
    m_itemOffScene = true;
}

// ------------------------------------------------------------- RemoveLayerCommand
RemoveLayerCommand::RemoveLayerCommand(QGraphicsScene *scene, LayerManager *manager,
                                       LayerProperties *properties, Layer *layer,
                                       const QString &text)
    : m_scene(scene), m_manager(manager), m_properties(properties), m_layer(layer)
{
    setText(text);
}

RemoveLayerCommand::~RemoveLayerCommand()
{
    if (m_itemOffScene && m_layer && m_layer->item())
        delete m_layer->item();
}

void RemoveLayerCommand::redo()
{
    if (m_layer->item())
        m_scene->removeItem(m_layer->item());
    m_manager->removeLayer(m_layer);
    if (m_properties)
        m_properties->removeLayer(m_layer);
    m_itemOffScene = true;
}

void RemoveLayerCommand::undo()
{
    if (m_layer->item())
        m_scene->addItem(m_layer->item());
    m_manager->addLayer(m_layer);
    m_manager->selectLayer(m_layer);
    m_itemOffScene = false;
}

// --------------------------------------------------------- PropertyChangeCommand
PropertyChangeCommand::PropertyChangeCommand(Tools::ITool *tool, QString propertyId,
                                             QVariant oldValue, QVariant newValue,
                                             const QString &text)
    : m_tool(tool), m_propertyId(std::move(propertyId)),
      m_oldValue(std::move(oldValue)), m_newValue(std::move(newValue))
{
    setText(text);
}

void PropertyChangeCommand::redo()
{
    if (m_tool)
        m_tool->setProperty(m_propertyId, m_newValue);
}

void PropertyChangeCommand::undo()
{
    if (m_tool)
        m_tool->setProperty(m_propertyId, m_oldValue);
}

bool PropertyChangeCommand::mergeWith(const QUndoCommand *other)
{
    const auto *o = dynamic_cast<const PropertyChangeCommand*>(other);
    if (!o || o->m_tool != m_tool || o->m_propertyId != m_propertyId)
        return false;
    m_newValue = o->m_newValue;   // keep the original old value, adopt the latest new value
    return true;
}

// -------------------------------------------------------- VisibilityChangeCommand
VisibilityChangeCommand::VisibilityChangeCommand(Layer *layer, bool newVisible, const QString &text)
    : m_layer(layer), m_newVisible(newVisible)
{
    setText(text);
}

void VisibilityChangeCommand::redo()
{
    if (m_layer)
        m_layer->setVisible(m_newVisible);
}

void VisibilityChangeCommand::undo()
{
    if (m_layer)
        m_layer->setVisible(!m_newVisible);
}

// --------------------------------------------------------------- MoveLayerCommand
MoveLayerCommand::MoveLayerCommand(QGraphicsItem *item, QPointF delta, const QString &text)
    : m_item(item), m_delta(delta)
{
    setText(text);
}

void MoveLayerCommand::redo()
{
    if (m_item)
        m_item->moveBy(m_delta.x(), m_delta.y());
}

void MoveLayerCommand::undo()
{
    if (m_item)
        m_item->moveBy(-m_delta.x(), -m_delta.y());
}

bool MoveLayerCommand::mergeWith(const QUndoCommand *other)
{
    const auto *o = dynamic_cast<const MoveLayerCommand*>(other);
    if (!o || o->m_item != m_item)
        return false;
    m_delta += o->m_delta;   // accumulate; a burst of nudges = one undo step
    return true;
}

// ------------------------------------------------------------ GroupLayersCommand
GroupLayersCommand::GroupLayersCommand(LayerManager *manager, Layer *group,
                                       QList<Layer*> members, const QString &text)
    : m_manager(manager), m_group(group), m_members(std::move(members))
{
    setText(text);
}

void GroupLayersCommand::redo()
{
    for (Layer *member : m_members) {
        m_manager->removeLayer(member);   // detach from the top level
        m_group->addChild(member);
    }
    m_manager->addLayer(m_group);         // add the group at the top level
    m_manager->selectLayer(m_group);
}

void GroupLayersCommand::undo()
{
    m_manager->removeLayer(m_group);
    for (Layer *member : m_members) {
        m_group->removeChild(member);
        m_manager->addLayer(member);      // restore members to the top level
    }
}

// ---------------------------------------------------------- UngroupLayersCommand
UngroupLayersCommand::UngroupLayersCommand(LayerManager *manager, Layer *group, const QString &text)
    : m_manager(manager), m_group(group), m_members(group ? group->children() : QList<Layer*>{})
{
    setText(text);
}

void UngroupLayersCommand::redo()
{
    m_manager->removeLayer(m_group);
    for (Layer *member : m_members) {
        m_group->removeChild(member);
        m_manager->addLayer(member);
    }
}

void UngroupLayersCommand::undo()
{
    for (Layer *member : m_members) {
        m_manager->removeLayer(member);
        m_group->addChild(member);
    }
    m_manager->addLayer(m_group);
    m_manager->selectLayer(m_group);
}

// --------------------------------------------------------- BackdropChangeCommand
BackdropChangeCommand::BackdropChangeCommand(Image::BackdropItem *backdrop, QString propertyId,
                                             QVariant value, Image::BackdropMemento before,
                                             const QString &text)
    : m_backdrop(backdrop), m_propertyId(std::move(propertyId)),
      m_value(std::move(value)), m_before(std::move(before))
{
    setText(text);
}

void BackdropChangeCommand::redo()
{
    if (m_backdrop)
        m_backdrop->setProperty(m_propertyId, m_value);   // apply the edit (fires onChanged)
}

void BackdropChangeCommand::undo()
{
    if (m_backdrop)
        m_backdrop->restore(m_before);   // restore the captured snapshot
}

bool BackdropChangeCommand::mergeWith(const QUndoCommand *other)
{
    const auto *o = dynamic_cast<const BackdropChangeCommand*>(other);
    if (!o || o->m_backdrop != m_backdrop || o->m_propertyId != m_propertyId)
        return false;
    m_value = o->m_value;   // adopt the latest value; keep our (pre-drag) snapshot
    return true;
}

} // namespace Editor::Commands
