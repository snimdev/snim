#include <QtTest>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QUndoStack>

#include "editor/annotations/commands/EditorCommands.h"
#include "editor/annotations/Layer.h"
#include "editor/annotations/LayerManager.h"
#include "editor/annotations/tools/ArrowTool.h"

using namespace Editor;
using namespace Editor::Commands;

// The undoable editor operations. Exercised against a real
// QGraphicsScene + LayerManager + QUndoStack (offscreen, no Editor).
class tst_EditorCommands : public QObject
{
    Q_OBJECT

    QGraphicsScene *scene = nullptr;
    LayerManager *manager = nullptr;
    QObject *owner = nullptr;     // owns Layers, mirroring Editor's QObject parent
    QUndoStack *stack = nullptr;

    Layer* makeLeaf(const QString &name)
    {
        auto *l = new Layer(name, Layer::Rectangle, owner);
        l->setItem(new QGraphicsRectItem(0, 0, 10, 10));
        return l;
    }

private slots:
    void init()
    {
        scene = new QGraphicsScene();
        manager = new LayerManager();
        owner = new QObject();
        stack = new QUndoStack();
    }

    void cleanup()
    {
        // Destroy the stack FIRST so commands free any off-scene items they own,
        // while the scene + layers are still alive (no double-free).
        delete stack;   stack = nullptr;
        delete owner;   owner = nullptr;     // frees Layers
        delete manager; manager = nullptr;
        delete scene;   scene = nullptr;
    }

    void addLayer_redoAddsUndoRemoves()
    {
        Layer *l = makeLeaf("R1");
        QGraphicsItem *item = l->item();
        stack->push(new AddLayerCommand(scene, manager, l, "Add"));
        QVERIFY(scene->items().contains(item));
        QVERIFY(manager->layers().contains(l));

        stack->undo();
        QVERIFY(!scene->items().contains(item));
        QVERIFY(!manager->layers().contains(l));

        stack->redo();
        QVERIFY(scene->items().contains(item));
        QVERIFY(manager->layers().contains(l));
    }

    void removeLayer_redoRemovesUndoRestores()
    {
        Layer *l = makeLeaf("R1");
        scene->addItem(l->item());
        manager->addLayer(l);

        stack->push(new RemoveLayerCommand(scene, manager, nullptr, l, "Delete"));
        QVERIFY(!scene->items().contains(l->item()));
        QVERIFY(!manager->layers().contains(l));

        stack->undo();
        QVERIFY(scene->items().contains(l->item()));
        QVERIFY(manager->layers().contains(l));
    }

    void propertyChange_redoUndo()
    {
        Tools::ArrowTool arrow(QPointF(0, 0), QPointF(10, 10));
        arrow.setProperty("width", 2.0);
        stack->push(new PropertyChangeCommand(&arrow, "width", 2.0, 8.0, "Change width"));
        QCOMPARE(arrow.pen().widthF(), 8.0);
        stack->undo();
        QCOMPARE(arrow.pen().widthF(), 2.0);
        stack->redo();
        QCOMPARE(arrow.pen().widthF(), 8.0);
        stack->clear();   // drop commands while `arrow` is still alive
    }

    void propertyChange_consecutiveEditsMergeToOneStep()
    {
        Tools::ArrowTool arrow(QPointF(0, 0), QPointF(10, 10));
        arrow.setProperty("width", 1.0);
        stack->push(new PropertyChangeCommand(&arrow, "width", 1.0, 5.0, "Change width"));
        stack->push(new PropertyChangeCommand(&arrow, "width", 5.0, 9.0, "Change width"));
        QCOMPARE(stack->count(), 1);            // merged (same tool + property)
        QCOMPARE(arrow.pen().widthF(), 9.0);    // latest value applied
        stack->undo();
        QCOMPARE(arrow.pen().widthF(), 1.0);    // single undo restores the pre-drag value
        stack->clear();
    }

    void visibilityChange_redoUndo()
    {
        Layer *l = makeLeaf("V");
        scene->addItem(l->item());
        manager->addLayer(l);
        stack->push(new VisibilityChangeCommand(l, false, "Hide"));
        QVERIFY(!l->isVisible());
        QVERIFY(!l->item()->isVisible());
        stack->undo();
        QVERIFY(l->isVisible());
        QVERIFY(l->item()->isVisible());
    }

    void moveLayer_redoUndo_andMerge()
    {
        Layer *l = makeLeaf("M");
        scene->addItem(l->item());
        manager->addLayer(l);
        const QPointF start = l->item()->pos();

        stack->push(new MoveLayerCommand(l->item(), QPointF(5, 0), "Nudge"));
        QCOMPARE(l->item()->pos(), start + QPointF(5, 0));

        stack->push(new MoveLayerCommand(l->item(), QPointF(0, 3), "Nudge"));  // same item -> merges
        QCOMPARE(stack->count(), 1);
        QCOMPARE(l->item()->pos(), start + QPointF(5, 3));

        stack->undo();                       // one undo reverses the whole burst
        QCOMPARE(l->item()->pos(), start);
    }

    void group_then_ungroup_undoable()
    {
        Layer *a = makeLeaf("A");
        Layer *b = makeLeaf("B");
        scene->addItem(a->item());
        scene->addItem(b->item());
        manager->addLayer(a);
        manager->addLayer(b);
        QCOMPARE(manager->layers().size(), 2);

        auto *group = new Layer("Group 1", Layer::Group, owner);
        stack->push(new GroupLayersCommand(manager, group, {a, b}, "Group"));
        QVERIFY(manager->layers().contains(group));
        QVERIFY(!manager->layers().contains(a));
        QVERIFY(!manager->layers().contains(b));
        QCOMPARE(group->children().size(), 2);

        stack->undo();
        QVERIFY(!manager->layers().contains(group));
        QVERIFY(manager->layers().contains(a));
        QVERIFY(manager->layers().contains(b));
        QCOMPARE(group->children().size(), 0);

        stack->redo();   // grouped again
        QVERIFY(manager->layers().contains(group));

        stack->push(new UngroupLayersCommand(manager, group, "Ungroup"));
        QVERIFY(!manager->layers().contains(group));
        QVERIFY(manager->layers().contains(a));
        QVERIFY(manager->layers().contains(b));
        QCOMPARE(group->children().size(), 0);
    }

    void cleanState_tracksSavePoint()
    {
        QVERIFY(stack->isClean());   // empty stack is clean
        Layer *l = makeLeaf("C");
        stack->push(new AddLayerCommand(scene, manager, l, "Add"));
        QVERIFY(!stack->isClean());
        stack->setClean();
        QVERIFY(stack->isClean());
        stack->undo();
        QVERIFY(!stack->isClean());
    }
};

QTEST_MAIN(tst_EditorCommands)
#include "tst_editorcommands.moc"
