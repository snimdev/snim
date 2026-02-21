#include <QtTest>
#include <QGraphicsScene>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <memory>

#include "editor/annotations/AnnotationSet.h"
#include "editor/annotations/tools/ArrowTool.h"
#include "editor/annotations/tools/BlurTool.h"
#include "editor/annotations/tools/RectangleTool.h"
#include "editor/annotations/tools/StepTool.h"
#include "editor/annotations/tools/TextTool.h"

using namespace Editor;

namespace {

using Owned = std::vector<std::unique_ptr<QGraphicsItem>>;

Owned own(const QList<AnnotationSet::Instance> &instances)
{
    Owned out;
    for (const AnnotationSet::Instance &i : instances)
        out.emplace_back(i.item);
    return out;
}

// Paints one item on its own over a transparent canvas the size of the source.
QImage renderAlone(QGraphicsItem *item, const QSize &size)
{
    QGraphicsScene scene;
    scene.setSceneRect(QRectF(QPointF(0, 0), size));
    scene.addItem(item);
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        scene.render(&p, QRectF(img.rect()), scene.sceneRect());
    }
    scene.removeItem(item);
    return img;
}

// Blue left of x 100, red right of it.
QPixmap splitSource()
{
    QPixmap pm(200, 120);
    pm.fill(Qt::blue);
    QPainter p(&pm);
    p.fillRect(QRect(100, 0, 100, 120), Qt::red);
    return pm;
}

} // namespace

// The overlay-to-editor handoff: prototypes cloned in, fresh items cloned out.
class tst_AnnotationSet : public QObject
{
    Q_OBJECT

    // Rectangle, arrow, step 3, text and blur, each at a distinct pos.
    static AnnotationSet sample()
    {
        AnnotationSet set;

        Tools::RectangleTool rect(QRectF(0, 0, 40, 20));
        rect.setPen(QPen(Qt::green, 5));
        set.add("rectangle", rect, QPointF(10, 15));

        Tools::ArrowTool arrow(QPointF(0, 0), QPointF(50, 30));
        arrow.setPen(QPen(Qt::magenta, 4));
        set.add("arrow", arrow, QPointF(20, 25));

        Tools::StepTool step;
        step.setNumber(3);
        step.setColor(Qt::cyan);
        set.add("step", step, QPointF(60, 70));

        Tools::TextTool text(QStringLiteral("hello"));
        text.setDefaultTextColor(Qt::yellow);
        set.add("text", text, QPointF(5, 90));

        // Local x 10..40 at pos x 100 lands in the red half of splitSource().
        Tools::BlurTool blur;
        blur.setSourcePixmap(QPixmap(200, 120));
        for (int x = 10; x <= 40; x += 5)
            blur.addPoint(QPointF(x, 10));
        blur.finishPath();
        set.add("blur", blur, QPointF(100, 50));

        return set;
    }

private slots:
    void emptyByDefault()
    {
        const AnnotationSet set;
        QVERIFY(set.isEmpty());
        QCOMPARE(set.size(), 0);
        QVERIFY(set.instantiate(QPixmap()).isEmpty());
    }

    void nonToolItemsAreSkipped()
    {
        AnnotationSet set;
        QGraphicsRectItem plain(0, 0, 10, 10);
        set.add("rectangle", plain, QPointF());
        QVERIFY(set.isEmpty());
    }

    void instancesCarryPosAndStyle()
    {
        const AnnotationSet set = sample();
        QCOMPARE(set.size(), 5);

        const Owned items = own(set.instantiate(splitSource()));
        QCOMPARE(items.size(), size_t(5));

        auto *rect = dynamic_cast<Tools::RectangleTool*>(items[0].get());
        QVERIFY(rect);
        QCOMPARE(rect->pos(), QPointF(10, 15));
        QCOMPARE(rect->shapeRect(), QRectF(0, 0, 40, 20));
        QCOMPARE(rect->pen().color(), QColor(Qt::green));

        auto *arrow = dynamic_cast<Tools::ArrowTool*>(items[1].get());
        QVERIFY(arrow);
        QCOMPARE(arrow->pos(), QPointF(20, 25));
        QCOMPARE(arrow->endPoint(), QPointF(50, 30));
        QCOMPARE(arrow->pen().color(), QColor(Qt::magenta));

        auto *step = dynamic_cast<Tools::StepTool*>(items[2].get());
        QVERIFY(step);
        QCOMPARE(step->pos(), QPointF(60, 70));
        QCOMPARE(step->number(), 3);
        QCOMPARE(step->color(), QColor(Qt::cyan));

        auto *text = dynamic_cast<Tools::TextTool*>(items[3].get());
        QVERIFY(text);
        QCOMPARE(text->pos(), QPointF(5, 90));
        QCOMPARE(text->toPlainText(), QStringLiteral("hello"));
        QCOMPARE(text->defaultTextColor(), QColor(Qt::yellow));

        QVERIFY(dynamic_cast<Tools::BlurTool*>(items[4].get()));
        QCOMPARE(items[4]->pos(), QPointF(100, 50));

        const QList<QString> ids{"rectangle", "arrow", "step", "text", "blur"};
        for (int i = 0; i < ids.size(); ++i)
            QCOMPARE(set.entries().at(i).toolId, ids.at(i));
    }

    void copiesInstantiateIndependentItems()
    {
        const AnnotationSet set = sample();
        const AnnotationSet copy = set;
        QCOMPARE(copy.size(), set.size());

        const Owned first = own(set.instantiate(splitSource()));
        const Owned second = own(copy.instantiate(splitSource()));
        for (size_t i = 0; i < first.size(); ++i) {
            QVERIFY(first[i].get() != second[i].get());
            QVERIFY(first[i].get() != set.entries().at(int(i)).prototype.get());
        }

        first[0]->setPos(0, 0);
        dynamic_cast<Tools::StepTool*>(first[2].get())->setNumber(9);
        QCOMPARE(second[0]->pos(), QPointF(10, 15));
        QCOMPARE(dynamic_cast<Tools::StepTool*>(second[2].get())->number(), 3);
    }

    void blurSamplesTheGivenSourceAtItsPos()
    {
        const AnnotationSet set = sample();
        Owned items = own(set.instantiate(splitSource()));
        const QImage img = renderAlone(items[4].get(), QSize(200, 120));

        // Scene (125, 60) is mid-stroke; sampling at pos (0,0) would have read blue.
        const QColor c = img.pixelColor(125, 60);
        QVERIFY2(c.red() > 200 && c.blue() < 50 && c.alpha() == 255,
                 qPrintable(c.name(QColor::HexArgb)));
    }

    void instancesOutliveTheSet()
    {
        Owned items;
        {
            const AnnotationSet set = sample();
            items = own(set.instantiate(splitSource()));
        }
        auto *text = dynamic_cast<Tools::TextTool*>(items[3].get());
        QVERIFY(text);
        QCOMPARE(text->toPlainText(), QStringLiteral("hello"));
        QCOMPARE(renderAlone(items[4].get(), QSize(200, 120)).pixelColor(125, 60).red(), 255);
    }
};

QTEST_MAIN(tst_AnnotationSet)
#include "tst_annotationset.moc"
