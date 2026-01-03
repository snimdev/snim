#include <QtTest>
#include <QPen>
#include <QColor>
#include <QPixmap>

#include "editor/tools/ITool.h"
#include "editor/tools/ArrowTool.h"
#include "editor/tools/RectangleTool.h"
#include "editor/tools/EllipseTool.h"
#include "editor/tools/FreehandTool.h"
#include "editor/tools/HighlightTool.h"
#include "editor/tools/BlurTool.h"
#include "editor/tools/TextTool.h"

using namespace Editor::Tools;

// clone() is a full duplicate (geometry + style); applyStyleFrom() copies only the
// style. Also exercises the dynamic getProperties/setProperty.
class tst_Tools : public QObject
{
    Q_OBJECT

private:
    static ToolProperty propById(const ITool *t, const QString &id)
    {
        for (const ToolProperty &p : t->getProperties())
            if (p.id == id)
                return p;
        return {};
    }

private slots:
    void arrow_clone_copiesGeometryAndStyle()
    {
        ArrowTool a(QPointF(1, 2), QPointF(30, 40));
        a.setPen(QPen(QColor("#0000ff"), 6));
        a.setArrowHeadType(ArrowTool::Filled);

        auto *c = dynamic_cast<ArrowTool*>(a.clone());
        QVERIFY(c);
        QCOMPARE(c->startPoint(), QPointF(1, 2));
        QCOMPARE(c->endPoint(), QPointF(30, 40));
        QCOMPARE(c->pen().color(), QColor("#0000ff"));
        QCOMPARE(c->pen().widthF(), 6.0);
        QCOMPARE(c->arrowHeadType(), ArrowTool::Filled);
        delete c;
    }

    void arrow_applyStyleFrom_copiesStyleNotGeometry()
    {
        ArrowTool src(QPointF(0, 0), QPointF(10, 10));
        src.setPen(QPen(QColor("#00ff00"), 9));
        src.setArrowHeadType(ArrowTool::Filled);

        ArrowTool dst(QPointF(100, 100), QPointF(200, 200));
        dst.applyStyleFrom(&src);

        QCOMPARE(dst.pen().color(), QColor("#00ff00"));
        QCOMPARE(dst.pen().widthF(), 9.0);
        QCOMPARE(dst.arrowHeadType(), ArrowTool::Filled);
        QCOMPARE(dst.startPoint(), QPointF(100, 100));   // geometry untouched
        QCOMPARE(dst.endPoint(), QPointF(200, 200));
    }

    void arrow_propertyRoundTrip()
    {
        ArrowTool a(QPointF(0, 0), QPointF(10, 10));
        a.setProperty("color", QColor("#123456"));
        a.setProperty("width", 7.0);
        a.setProperty("headType", QString("Filled"));
        QCOMPARE(propById(&a, "color").value.value<QColor>(), QColor("#123456"));
        QCOMPARE(propById(&a, "width").value.toReal(), 7.0);
        QCOMPARE(propById(&a, "headType").value.toString(), QStringLiteral("Filled"));
    }

    void shape_clone_and_applyStyle()
    {
        RectangleTool r(QRectF(5, 5, 50, 30));
        r.setPen(QPen(QColor("#abcdef"), 4));
        r.setBrush(QBrush(QColor("#fedcba")));
        r.setOpacity(0.5);

        auto *c = dynamic_cast<RectangleTool*>(r.clone());
        QVERIFY(c);
        QCOMPARE(c->shapeRect(), QRectF(5, 5, 50, 30));
        QCOMPARE(c->pen().color(), QColor("#abcdef"));
        QCOMPARE(c->brush().color(), QColor("#fedcba"));
        QCOMPARE(c->getOpacity(), 0.5);
        delete c;

        EllipseTool dst(QRectF(0, 0, 1, 1));
        dst.applyStyleFrom(&r);
        QCOMPARE(dst.pen().color(), QColor("#abcdef"));
        QCOMPARE(dst.getOpacity(), 0.5);
        QCOMPARE(dst.shapeRect(), QRectF(0, 0, 1, 1));   // geometry untouched

        // Opacity property is expressed as a 0..100 percentage.
        RectangleTool r2(QRectF(0, 0, 10, 10));
        r2.setProperty("opacity", 40);
        QCOMPARE(r2.getOpacity(), 0.4);
    }

    void freehand_clone_copiesPointsAndPen()
    {
        FreehandTool f;
        f.setPen(QPen(QColor("#222222"), 5));
        f.addPoint(QPointF(0, 0));
        f.addPoint(QPointF(10, 10));
        f.addPoint(QPointF(20, 5));
        f.finishPath();

        auto *c = dynamic_cast<FreehandTool*>(f.clone());
        QVERIFY(c);
        QCOMPARE(c->points().size(), 3);
        QCOMPARE(c->points().last(), QPointF(20, 5));
        QCOMPARE(c->pen().color(), QColor("#222222"));
        delete c;
    }

    void highlight_clone_copiesColorWidthPoints()
    {
        HighlightTool h;
        h.setColor(QColor("#b3ff61"));
        h.setWidth(HighlightTool::HIGHLIGHT_WIDTH_LARGE);
        h.addPoint(QPointF(1, 1));
        h.addPoint(QPointF(2, 2));
        h.finishPath();

        auto *c = dynamic_cast<HighlightTool*>(h.clone());
        QVERIFY(c);
        QCOMPARE(c->color(), QColor("#b3ff61"));
        QCOMPARE(c->width(), HighlightTool::HIGHLIGHT_WIDTH_LARGE);
        QCOMPARE(c->points().size(), 2);
        delete c;
    }

    void blur_clone_copiesParamsAndPoints()
    {
        BlurTool b;
        b.setBlurRadius(20);
        b.setBrushWidth(40);
        b.setSourcePixmap(QPixmap(32, 32));
        b.addPoint(QPointF(0, 0));
        b.addPoint(QPointF(5, 5));
        b.finishPath();

        auto *c = dynamic_cast<BlurTool*>(b.clone());
        QVERIFY(c);
        QCOMPARE(c->blurRadius(), 20.0);
        QCOMPARE(c->brushWidth(), 40.0);
        QCOMPARE(c->points().size(), 2);
        delete c;
    }

    void text_clone_copiesTextColorFont()
    {
        TextTool t("Hello");
        t.setDefaultTextColor(QColor("#777777"));
        QFont f("Helvetica", 22);
        f.setBold(true);
        t.setFont(f);

        auto *c = dynamic_cast<TextTool*>(t.clone());
        QVERIFY(c);
        QCOMPARE(c->toPlainText(), QStringLiteral("Hello"));
        QCOMPARE(c->defaultTextColor(), QColor("#777777"));
        QCOMPARE(c->font().family(), QStringLiteral("Helvetica"));
        QVERIFY(c->font().bold());
        delete c;
    }
};

QTEST_MAIN(tst_Tools)
#include "tst_tools.moc"
