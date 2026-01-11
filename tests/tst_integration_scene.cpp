#include <QtTest>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QUndoStack>
#include <QImage>
#include <QPainter>
#include <QColor>

#include "editor/commands/EditorCommands.h"
#include "editor/Layer.h"
#include "editor/LayerManager.h"
#include "editor/tools/RectangleTool.h"
#include "editor/tools/EllipseTool.h"
#include "editor/tools/StepTool.h"
#include "editor/StepNumbering.h"

using namespace Editor;
using namespace Editor::Commands;

// End-to-end through the same pieces Editor wires (scene + manager + undo
// stack + real tool items), without depending on real screen capture. Renders
// the scene to a QImage and asserts pixels track edits and undo/redo.
class tst_IntegrationScene : public QObject
{
    Q_OBJECT

    QGraphicsScene *scene = nullptr;
    LayerManager *manager = nullptr;
    QObject *owner = nullptr;
    QUndoStack *stack = nullptr;

    QImage render()
    {
        QImage img(100, 100, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        scene->render(&p, QRectF(0, 0, 100, 100), QRectF(0, 0, 100, 100));
        p.end();
        return img;
    }

    Layer* addBackground()
    {
        auto *bgItem = new QGraphicsRectItem(0, 0, 100, 100);
        bgItem->setBrush(QBrush(Qt::white));
        bgItem->setPen(Qt::NoPen);
        auto *bg = new Layer("Background", Layer::Background, owner);
        bg->setItem(bgItem);
        stack->push(new AddLayerCommand(scene, manager, bg, "Add Background"));
        return bg;
    }

private slots:
    void init()
    {
        scene = new QGraphicsScene();
        scene->setSceneRect(0, 0, 100, 100);
        manager = new LayerManager();
        owner = new QObject();
        stack = new QUndoStack();
    }

    void cleanup()
    {
        delete stack;   stack = nullptr;     // free commands (and off-scene items) first
        delete owner;   owner = nullptr;
        delete manager; manager = nullptr;
        delete scene;   scene = nullptr;
    }

    void draw_recolor_undo_redo_rendersCorrectly()
    {
        addBackground();

        // A red-filled rectangle covering the centre.
        auto *rect = new Tools::RectangleTool(QRectF(20, 20, 60, 60));
        rect->setProperty("fillColor", QColor(255, 0, 0));
        auto *rl = new Layer("Rect 1", Layer::Rectangle, owner);
        rl->setItem(rect);
        stack->push(new AddLayerCommand(scene, manager, rl, "Add Rectangle"));
        QCOMPARE(manager->layers().size(), 2);

        QColor c = render().pixelColor(50, 50);
        QVERIFY2(c.red() > 150 && c.green() < 100 && c.blue() < 100, "centre should be red");

        // Recolor to blue (undoable).
        stack->push(new PropertyChangeCommand(rect, "fillColor",
                                              QColor(255, 0, 0), QColor(0, 0, 255), "Recolor"));
        c = render().pixelColor(50, 50);
        QVERIFY2(c.blue() > 150 && c.red() < 100, "centre should be blue after recolor");

        // Undo recolor -> red again.
        stack->undo();
        c = render().pixelColor(50, 50);
        QVERIFY2(c.red() > 150 && c.blue() < 100, "centre should be red after undo");

        // Undo the rectangle add -> centre shows the white background.
        stack->undo();
        c = render().pixelColor(50, 50);
        QVERIFY2(c.red() > 200 && c.green() > 200 && c.blue() > 200, "centre should be white");

        // Redo the rectangle add -> red rectangle back.
        stack->redo();
        c = render().pixelColor(50, 50);
        QVERIFY2(c.red() > 150 && c.blue() < 100, "centre should be red after redo");
    }

    void grouping_updatesManager_andUndoes()
    {
        addBackground();

        auto *r = new Tools::RectangleTool(QRectF(10, 10, 20, 20));
        auto *rl = new Layer("Rect 1", Layer::Rectangle, owner);
        rl->setItem(r);
        stack->push(new AddLayerCommand(scene, manager, rl, "Add Rectangle"));

        auto *e = new Tools::EllipseTool(QRectF(50, 50, 20, 20));
        auto *el = new Layer("Ellipse 1", Layer::Ellipse, owner);
        el->setItem(e);
        stack->push(new AddLayerCommand(scene, manager, el, "Add Ellipse"));

        QCOMPARE(manager->layers().size(), 3);   // bg + rect + ellipse

        auto *group = new Layer("Group 1", Layer::Group, owner);
        stack->push(new GroupLayersCommand(manager, group, {rl, el}, "Group"));
        QVERIFY(manager->layers().contains(group));
        QVERIFY(!manager->layers().contains(rl));
        QCOMPARE(group->children().size(), 2);
        QCOMPARE(manager->layers().size(), 2);    // bg + group

        // Both annotations are still on the scene (grouping is organizational).
        QVERIFY(scene->items().contains(r));
        QVERIFY(scene->items().contains(e));

        stack->undo();   // ungroup back to top level
        QVERIFY(!manager->layers().contains(group));
        QVERIFY(manager->layers().contains(rl));
        QVERIFY(manager->layers().contains(el));
        QCOMPARE(manager->layers().size(), 3);
    }

    void stepBadges_sequence_respectsUndo()
    {
        addBackground();

        // Each badge takes its number the way the editor's stamp lambda does.
        for (int expected = 1; expected <= 3; ++expected) {
            const int n = nextStepNumber(manager->layers());
            QCOMPARE(n, expected);
            auto *badge = new Tools::StepTool();
            badge->setNumber(n);
            badge->setPos(10 * n, 10 * n);
            auto *sl = new Layer(QString("Step %1").arg(n), Layer::Step, owner);
            sl->setItem(badge);
            stack->push(new AddLayerCommand(scene, manager, sl, "Add Step"));
        }
        QCOMPARE(nextStepNumber(manager->layers()), 4);

        stack->undo();   // the "3" badge leaves the manager, so the next one is a 3 again
        QCOMPARE(nextStepNumber(manager->layers()), 3);

        stack->redo();
        QCOMPARE(nextStepNumber(manager->layers()), 4);
    }
};

QTEST_MAIN(tst_IntegrationScene)
#include "tst_integration_scene.moc"
