#ifndef IMAGEEDITOR_EDITORCOMMANDS_H
#define IMAGEEDITOR_EDITORCOMMANDS_H

#include <QUndoCommand>
#include <QString>
#include <QVariant>
#include <QPointF>
#include "editor/tools/BackdropMemento.h"   // by-value member of BackdropChangeCommand

class QGraphicsScene;
class QGraphicsItem;

namespace ImageEditor {

class Layer;
class LayerManager;
class LayerProperties;

namespace Tools { class ITool; class BackdropItem; }

namespace Commands {

// Unique merge ids (so a slider drag / key burst collapses to one step per id+target).
enum CommandId { PropertyChangeId = 1, BackdropChangeId = 2, MoveLayerId = 3 };

/**
 * Add a layer (its QGraphicsItem + Layer record) to the scene and manager, undoably.
 *
 * Ownership: the Layer object is owned by the editor (QObject parent) for its
 * whole life and is never deleted here. This command owns the QGraphicsItem only
 * while it is OFF the scene (i.e. after undo); its destructor frees the item only
 * in that state, so closing the editor or trimming the stack never double-frees
 * (when the item is on the scene, the scene owns it).
 */
class AddLayerCommand : public QUndoCommand {
public:
    AddLayerCommand(QGraphicsScene *scene, LayerManager *manager, Layer *layer,
                    const QString &text);
    ~AddLayerCommand() override;
    void redo() override;
    void undo() override;

private:
    QGraphicsScene *m_scene;
    LayerManager *m_manager;
    Layer *m_layer;
    bool m_itemOffScene = true;   // true => this command owns the (off-scene) item
};

/** Remove a layer, undoably. The inverse lifecycle of Add. */
class RemoveLayerCommand : public QUndoCommand {
public:
    RemoveLayerCommand(QGraphicsScene *scene, LayerManager *manager,
                       LayerProperties *properties, Layer *layer, const QString &text);
    ~RemoveLayerCommand() override;
    void redo() override;
    void undo() override;

private:
    QGraphicsScene *m_scene;
    LayerManager *m_manager;
    LayerProperties *m_properties;
    Layer *m_layer;
    bool m_itemOffScene = false;
};

/**
 * A single tool property edit (color/width/font/…), undoable.
 * Consecutive edits of the SAME tool+property merge (mergeWith) so dragging a
 * slider collapses into one undo step. The tool is a non-owning pointer.
 */
class PropertyChangeCommand : public QUndoCommand {
public:
    PropertyChangeCommand(Tools::ITool *tool, QString propertyId,
                          QVariant oldValue, QVariant newValue, const QString &text);
    void redo() override;
    void undo() override;
    [[nodiscard]] int id() const override { return PropertyChangeId; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    Tools::ITool *m_tool;
    QString m_propertyId;
    QVariant m_oldValue;
    QVariant m_newValue;
};

/**
 * Move a layer's item by a delta, undoably. Consecutive moves of the
 * same item merge (mergeWith) so a burst of arrow-key nudges (or a drag, if routed
 * here) collapses into one undo step. The item is a non-owning pointer (the scene
 * owns it while it's on the scene).
 */
class MoveLayerCommand : public QUndoCommand {
public:
    MoveLayerCommand(QGraphicsItem *item, QPointF delta, const QString &text);
    void redo() override;
    void undo() override;
    [[nodiscard]] int id() const override { return MoveLayerId; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    QGraphicsItem *m_item;
    QPointF m_delta;
};

/** Toggle a layer's visibility, undoably. (The layer list refresh is
 *  driven separately by ImageEditor on the stack's indexChanged.) */
class VisibilityChangeCommand : public QUndoCommand {
public:
    VisibilityChangeCommand(Layer *layer, bool newVisible, const QString &text);
    void redo() override;
    void undo() override;

private:
    Layer *m_layer;
    bool m_newVisible;
};

/**
 * Move the given top-level leaf layers under a new
 * group layer, undoably. The group is owned by the editor; this command only
 * re-parents within the manager/model, never touching the scene (grouping is
 * organizational, so z-order is unchanged).
 */
class GroupLayersCommand : public QUndoCommand {
public:
    GroupLayersCommand(LayerManager *manager, Layer *group, QList<Layer*> members,
                       const QString &text);
    void redo() override;
    void undo() override;

private:
    LayerManager *m_manager;
    Layer *m_group;
    QList<Layer*> m_members;
};

/** Dissolve a group, returning its children to the top level. */
class UngroupLayersCommand : public QUndoCommand {
public:
    UngroupLayersCommand(LayerManager *manager, Layer *group, const QString &text);
    void redo() override;
    void undo() override;

private:
    LayerManager *m_manager;
    Layer *m_group;
    QList<Layer*> m_members;
};

/**
 * One undoable backdrop property edit. `redo()` applies the edit; `undo()` restores
 * the captured `BackdropMemento` (the command holds the opaque snapshot and never
 * inspects it). Consecutive edits of the same backdrop + property merge, so a slider
 * drag is a single undo step.
 */
class BackdropChangeCommand : public QUndoCommand {
public:
    BackdropChangeCommand(Tools::BackdropItem *backdrop, QString propertyId,
                          QVariant value, Tools::BackdropMemento before, const QString &text);
    void redo() override;
    void undo() override;
    [[nodiscard]] int id() const override { return BackdropChangeId; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    Tools::BackdropItem *m_backdrop;
    QString m_propertyId;
    QVariant m_value;
    Tools::BackdropMemento m_before;   // opaque snapshot used by undo()
};

} // namespace Commands
} // namespace ImageEditor

#endif // IMAGEEDITOR_EDITORCOMMANDS_H
