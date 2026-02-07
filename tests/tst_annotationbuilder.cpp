#include <QtTest>
#include <QGraphicsScene>
#include <QPointer>
#include <QPen>

#include "editor/annotations/AnnotationBuilder.h"
#include "editor/annotations/IAnnotationSink.h"
#include "editor/annotations/interactions/IDrawingInteraction.h"
#include "editor/annotations/interactions/ArrowDrawingInteraction.h"
#include "editor/annotations/tools/ArrowTool.h"
#include "editor/annotations/tools/RectangleTool.h"
#include "editor/annotations/tools/EllipseTool.h"
#include "editor/annotations/tools/FreehandTool.h"
#include "editor/annotations/tools/StepTool.h"
#include "editor/annotations/tools/TextTool.h"

using namespace Editor;

namespace {

struct RecordingSink : IAnnotationSink
{
    struct Entry { QGraphicsItem *item; QString toolId; };
    QList<Entry> entries;

    void commit(QGraphicsItem *item, const QString &toolId) override
    {
        entries.append({item, toolId});
    }
    ~RecordingSink() override
    {
        for (const Entry &e : std::as_const(entries))
            delete e.item;
    }
};

} // namespace

// The builder turns each tool's gesture into one styled item handed to the sink,
// with no view, layers or panels involved.
class tst_AnnotationBuilder : public QObject
{
    Q_OBJECT

    QGraphicsScene *scene = nullptr;
    RecordingSink *sink = nullptr;
    AnnotationBuilder *builder = nullptr;

    void drag(const QString &toolId, const QList<QPointF> &points)
    {
        Interactions::IDrawingInteraction *i = builder->interaction(toolId);
        QVERIFY(i);
        i->onMousePress(points.first(), scene);
        for (int k = 1; k < points.size() - 1; ++k)
            i->onMouseMove(points.at(k), scene);
        i->onMouseRelease(points.last(), scene);
    }

    Tools::TextTool *placeText(const QPointF &at)
    {
        Tools::TextTool *placed = nullptr;
        auto c = connect(builder, &AnnotationBuilder::textPlaced, this,
                         [&placed](Tools::TextTool *item) { placed = item; });
        builder->interaction("text")->onMousePress(at, scene);
        disconnect(c);
        return placed;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_annotationbuilder");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        scene = new QGraphicsScene();
        scene->setSceneRect(0, 0, 200, 200);
        // Text focus only lands in an active scene; a view would normally activate it.
        QEvent activate(QEvent::WindowActivate);
        QCoreApplication::sendEvent(scene, &activate);
        sink = new RecordingSink();
        builder = new AnnotationBuilder(scene, sink);
        builder->setImageBounds(QRect(0, 0, 200, 200));
    }

    void cleanup()
    {
        delete builder;
        delete sink;
        delete scene;
    }

    void rectangle_dragReachesSinkStyled()
    {
        auto *tmpl = dynamic_cast<Tools::RectangleTool*>(builder->templateFor("rectangle"));
        QVERIFY(tmpl);
        tmpl->setPen(QPen(Qt::magenta, 7));

        drag("rectangle", {QPointF(10, 10), QPointF(40, 30), QPointF(60, 50)});

        QCOMPARE(sink->entries.size(), 1);
        QCOMPARE(sink->entries.first().toolId, QStringLiteral("rectangle"));
        auto *rect = dynamic_cast<Tools::RectangleTool*>(sink->entries.first().item);
        QVERIFY(rect);
        QCOMPARE(rect->pen().color(), QColor(Qt::magenta));
        QCOMPARE(rect->pen().widthF(), 7.0);
        QVERIFY(!rect->scene());
    }

    void ellipse_dragReachesSinkStyled()
    {
        auto *tmpl = dynamic_cast<Tools::EllipseTool*>(builder->templateFor("ellipse"));
        QVERIFY(tmpl);
        tmpl->setPen(QPen(Qt::cyan, 4));

        drag("ellipse", {QPointF(10, 10), QPointF(50, 40)});

        QCOMPARE(sink->entries.size(), 1);
        QCOMPARE(sink->entries.first().toolId, QStringLiteral("ellipse"));
        auto *ellipse = dynamic_cast<Tools::EllipseTool*>(sink->entries.first().item);
        QVERIFY(ellipse);
        QCOMPARE(ellipse->pen().color(), QColor(Qt::cyan));
    }

    void arrow_dragReachesSinkStyled()
    {
        auto *tmpl = dynamic_cast<Tools::ArrowTool*>(builder->templateFor("arrow"));
        QVERIFY(tmpl);
        tmpl->setPen(QPen(Qt::green, 5));

        drag("arrow", {QPointF(10, 10), QPointF(30, 20), QPointF(80, 60)});

        QCOMPARE(sink->entries.size(), 1);
        QCOMPARE(sink->entries.first().toolId, QStringLiteral("arrow"));
        auto *arrow = dynamic_cast<Tools::ArrowTool*>(sink->entries.first().item);
        QVERIFY(arrow);
        QCOMPARE(arrow->pen().color(), QColor(Qt::green));
    }

    void arrow_shortIsRejected()
    {
        drag("arrow", {QPointF(10, 10), QPointF(15, 10)});
        QVERIFY(sink->entries.isEmpty());

        // The builder's own 10 px minimum, independent of the interaction's.
        auto *arrow = dynamic_cast<Interactions::ArrowDrawingInteraction*>(builder->interaction("arrow"));
        QVERIFY(arrow);
        emit arrow->arrowDrawn(QPoint(10, 10), QPoint(15, 10));
        QVERIFY(sink->entries.isEmpty());
        QVERIFY(scene->items().isEmpty());   // no preview left behind
    }

    void freehand_dragReachesSinkWithPoints()
    {
        auto *tmpl = dynamic_cast<Tools::FreehandTool*>(builder->templateFor("freehand"));
        QVERIFY(tmpl);
        tmpl->setPen(QPen(Qt::blue, 9));

        drag("freehand", {QPointF(10, 10), QPointF(20, 20), QPointF(30, 25), QPointF(40, 30)});

        QCOMPARE(sink->entries.size(), 1);
        QCOMPARE(sink->entries.first().toolId, QStringLiteral("freehand"));
        auto *stroke = dynamic_cast<Tools::FreehandTool*>(sink->entries.first().item);
        QVERIFY(stroke);
        QCOMPARE(stroke->pen().color(), QColor(Qt::blue));
        QCOMPARE(stroke->points().size(), 3);
        QCOMPARE(stroke->points().first(), QPointF(10, 10));
    }

    void step_numbersComeFromProviderAndTemplateAdvances()
    {
        int next = 5;
        builder->setStepNumberProvider([&next] { return next++; });
        auto *tmpl = dynamic_cast<Tools::StepTool*>(builder->templateFor("step"));
        QVERIFY(tmpl);

        builder->interaction("step")->onMousePress(QPointF(20, 20), scene);
        builder->interaction("step")->onMousePress(QPointF(60, 20), scene);

        QCOMPARE(sink->entries.size(), 2);
        auto *first = dynamic_cast<Tools::StepTool*>(sink->entries.at(0).item);
        auto *second = dynamic_cast<Tools::StepTool*>(sink->entries.at(1).item);
        QVERIFY(first && second);
        QCOMPARE(sink->entries.at(0).toolId, QStringLiteral("step"));
        QCOMPARE(first->number(), 5);
        QCOMPARE(second->number(), 6);
        QCOMPARE(second->pos(), QPointF(60, 20));
        QCOMPARE(tmpl->number(), 7);
    }

    void text_placedBeforeEditingStarts()
    {
        bool placedInScene = false;
        bool editingAlready = true;
        connect(builder, &AnnotationBuilder::textPlaced, this, [&](Tools::TextTool *item) {
            placedInScene = item->scene() == scene;
            editingAlready = item->textInteractionFlags() != Qt::NoTextInteraction;
        });
        Tools::TextTool *item = placeText(QPointF(30, 40));

        QVERIFY(item);
        QVERIFY(placedInScene);
        QVERIFY(!editingAlready);
        QVERIFY(item->textInteractionFlags() != Qt::NoTextInteraction);
        QCOMPARE(item->pos(), QPointF(30, 40));
        QVERIFY(builder->isEditingText());
        QVERIFY(sink->entries.isEmpty());
    }

    void text_emptyIsDeletedWithoutSink()
    {
        QPointer<Tools::TextTool> item = placeText(QPointF(30, 40));
        QVERIFY(item);

        builder->commitPendingText();
        QCoreApplication::processEvents();   // the deferred focus-out finalize is a no-op

        QVERIFY(item.isNull());
        QVERIFY(sink->entries.isEmpty());
        QVERIFY(!builder->isEditingText());
    }

    void text_typedCommitsOnce()
    {
        Tools::TextTool *item = placeText(QPointF(30, 40));
        QVERIFY(item);
        item->setPlainText(QStringLiteral("Hello"));

        builder->commitPendingText();
        QCoreApplication::processEvents();

        QCOMPARE(sink->entries.size(), 1);
        QCOMPARE(sink->entries.first().item, static_cast<QGraphicsItem*>(item));
        QCOMPARE(sink->entries.first().toolId, QStringLiteral("text"));
        QVERIFY(!item->scene());
        QCOMPARE(item->textInteractionFlags(), Qt::NoTextInteraction);
    }

    void text_focusLossCommitsAfterATick()
    {
        Tools::TextTool *item = placeText(QPointF(30, 40));
        QVERIFY(item);
        item->setPlainText(QStringLiteral("Later"));

        item->clearFocus();
        QVERIFY(sink->entries.isEmpty());
        QCoreApplication::processEvents();

        QCOMPARE(sink->entries.size(), 1);
        QCOMPARE(sink->entries.first().item, static_cast<QGraphicsItem*>(item));
    }
};

QTEST_MAIN(tst_AnnotationBuilder)
#include "tst_annotationbuilder.moc"
