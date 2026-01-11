#include <QtTest>
#include <QStandardPaths>
#include <QGraphicsItem>
#include <QGraphicsRectItem>

#include "editor/annotations/StepNumbering.h"
#include "editor/annotations/Layer.h"
#include "editor/annotations/tools/StepTool.h"

using namespace Editor;

// The next step badge's number is derived from the layer stack (topmost badge + 1)
// rather than a counter, which is what makes undo and a restarted sequence work.
class tst_StepNumbering : public QObject
{
    Q_OBJECT

    QObject *owner = nullptr;              // owns the Layers, mirroring the editor
    QList<QGraphicsItem*> items;           // Layer does not own its item

    Layer* makeStep(int number)
    {
        auto *s = new Tools::StepTool();
        s->setNumber(number);
        items.append(s);
        auto *l = new Layer(QString("Step %1").arg(number), Layer::Step, owner);
        l->setItem(s);
        return l;
    }

    Layer* makeRect()
    {
        auto *r = new QGraphicsRectItem(0, 0, 10, 10);
        items.append(r);
        auto *l = new Layer("Rect 1", Layer::Rectangle, owner);
        l->setItem(r);
        return l;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("NiceshotTest");
        QCoreApplication::setApplicationName("tst_stepnumbering");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        owner = new QObject();
    }

    void cleanup()
    {
        delete owner; owner = nullptr;      // frees Layers
        qDeleteAll(items);                  // then the items they pointed at
        items.clear();
    }

    void emptyStack_startsAtOne()
    {
        QCOMPARE(nextStepNumber({}), 1);
    }

    void sequentialBadges_continue()
    {
        QList<Layer*> layers{makeStep(1), makeStep(2), makeStep(3)};
        QCOMPARE(nextStepNumber(layers), 4);
    }

    void restampedOne_restartsTheSequence()
    {
        // Topmost badge wins, not the global maximum: stamping a fresh "1" on top
        // means the next badge is a 2.
        QList<Layer*> layers{makeStep(1), makeStep(2), makeStep(3), makeStep(1)};
        QCOMPARE(nextStepNumber(layers), 2);
    }

    void nonStepLayersAreSkipped()
    {
        QList<Layer*> layers{makeStep(1), makeStep(2), makeRect()};
        QCOMPARE(nextStepNumber(layers), 3);
    }

    void badgeInsideTopmostGroupIsFound()
    {
        auto *group = new Layer("Group 1", Layer::Group, owner);
        group->addChild(makeStep(4));
        group->addChild(makeStep(5));
        QList<Layer*> layers{makeStep(1), group};
        QCOMPARE(nextStepNumber(layers), 6);
    }

    void removingTheTopLayerStepsBack()
    {
        QList<Layer*> layers{makeStep(1), makeStep(2), makeStep(3)};
        QCOMPARE(nextStepNumber(layers), 4);
        layers.removeLast();
        QCOMPARE(nextStepNumber(layers), 3);
    }
};

QTEST_MAIN(tst_StepNumbering)
#include "tst_stepnumbering.moc"
