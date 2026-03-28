#include <QtTest>

#include <QPainter>
#include <QPixmap>
#include <QSignalSpy>
#include <memory>

#include "capture/OverlayAnnotations.h"
#include "editor/annotations/ToolRegistry.h"
#include "editor/annotations/tools/ArrowTool.h"
#include "editor/annotations/tools/HighlightTool.h"
#include "editor/annotations/tools/RectangleTool.h"
#include "editor/annotations/tools/StepTool.h"

using namespace Capture;

// The area selector's annotation session, driven headless in virtual-desktop coords.
// The desktop origin is non-zero so any missed virtual->scene conversion shows.
class tst_OverlayAnnotations : public QObject
{
    Q_OBJECT

private:
    const QRect kVirtual{-1920, 100, 800, 600};
    const QRect kArea{-1800, 200, 400, 300};   // selection; scene (120,100) 400x300
    const QColor kGrey{128, 128, 128};
    QColor m_stroke;

    static QPixmap frame(qreal dpr)
    {
        QPixmap pm(QSize(800, 600) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(QColor(128, 128, 128));
        return pm;
    }

    static bool near(const QColor &a, const QColor &b)
    {
        return qAbs(a.red() - b.red()) < 40 && qAbs(a.green() - b.green()) < 40
               && qAbs(a.blue() - b.blue()) < 40;
    }

    static QColor at(const QPixmap &pm, int x, int y)
    {
        return pm.toImage().pixelColor(x, y);
    }

    static bool allGrey(const QPixmap &pm)
    {
        const QImage img = pm.toImage();
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixelColor(x, y) != QColor(128, 128, 128))
                    return false;
        return true;
    }

    // Rectangle at virtual (-1700,250)-(-1600,350): crop-local (100,50)-(200,150).
    static void drawRect(OverlayAnnotations &s, const QPoint &from = {-1700, 250},
                         const QPoint &to = {-1600, 350})
    {
        s.setActiveTool("rectangle");
        QVERIFY(s.press(from));
        QVERIFY(s.isDrawing());
        QVERIFY(s.move(to));
        QVERIFY(s.release(to));
        QVERIFY(!s.isDrawing());
    }

    static QList<int> stepNumbers(const Editor::AnnotationSet &set)
    {
        QList<int> out;
        for (const auto &e : set.entries())
            if (auto *step = dynamic_cast<Editor::Tools::StepTool*>(e.prototype.get()))
                out.append(step->number());
        return out;
    }

    // The highlight template's colour laid over the grey frame.
    static QColor highlightOverGrey()
    {
        std::unique_ptr<Editor::Tools::ITool> tmpl(
            Editor::ToolRegistry::find("highlight")->makeTemplate());
        const QColor c = dynamic_cast<Editor::Tools::HighlightTool*>(tmpl.get())->color();
        const qreal a = Editor::Tools::HighlightTool::HIGHLIGHT_OPACITY;
        const auto mix = [a](int fg) { return qRound(128 + a * (fg - 128)); };
        return QColor(mix(c.red()), mix(c.green()), mix(c.blue()));
    }

    static bool approx(const QColor &a, const QColor &b)
    {
        return qAbs(a.red() - b.red()) <= 3 && qAbs(a.green() - b.green()) <= 3
               && qAbs(a.blue() - b.blue()) <= 3;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_overlayannotations");
        QStandardPaths::setTestModeEnabled(true);
        std::unique_ptr<Editor::Tools::ITool> tmpl(
            Editor::ToolRegistry::find("rectangle")->makeTemplate());
        m_stroke = dynamic_cast<Editor::Tools::RectangleTool*>(tmpl.get())->pen().color();
        QVERIFY(!near(m_stroke, kGrey));
    }

    void rectangleIsFlattenedIntoTheCrop()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        drawRect(s);
        QVERIFY(s.hasItems());

        const QPixmap out = s.flattenedCrop(kArea);
        QCOMPARE(out.size(), kArea.size());
        QVERIFY(near(at(out, 100, 100), m_stroke));
        QVERIFY(near(at(out, 200, 100), m_stroke));
        QCOMPARE(at(out, 150, 100), kGrey);
    }

    void undoRemovesAndRedoRestores()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        QVERIFY(!s.canUndo());
        drawRect(s);
        QVERIFY(s.canUndo());
        QVERIFY(!s.canRedo());

        s.undo();
        QVERIFY(!s.canUndo());
        QVERIFY(s.canRedo());
        QVERIFY(!s.hasItems());
        QVERIFY(allGrey(s.flattenedCrop(kArea)));

        s.redo();
        QVERIFY(s.canUndo());
        QVERIFY(near(at(s.flattenedCrop(kArea), 100, 100), m_stroke));
    }

    void toolStaysArmedAfterAShape()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        drawRect(s);
        QCOMPARE(s.activeTool(), QStringLiteral("rectangle"));
        drawRect(s, {-1750, 220}, {-1650, 300});
        s.undo();
        QVERIFY(s.canUndo());   // two separate undo steps
    }

    void hiDpiFrameGivesNativeResolution()
    {
        OverlayAnnotations s(frame(2), kVirtual);
        s.setSelection(kArea);
        drawRect(s);

        const QPixmap out = s.flattenedCrop(kArea);
        QCOMPARE(out.size(), kArea.size() * 2);
        QCOMPARE(out.devicePixelRatio(), 2.0);
        QVERIFY(near(at(out, 200, 200), m_stroke));
        QVERIFY(near(at(out, 400, 200), m_stroke));
        QCOMPARE(at(out, 300, 200), kGrey);
    }

    void singleScreenAreaFlattensOntoThatScreensGrab()
    {
        // The frame is composited at 2x; the screen holding the area was grabbed at 1x.
        OverlayAnnotations s(frame(2), kVirtual);
        QPixmap screen(800, 600);
        screen.fill(kGrey);
        s.setScreenGrabs({ { kVirtual, screen } });
        s.setSelection(kArea);
        drawRect(s);

        const QPixmap out = s.flattenedCrop(kArea);
        QCOMPARE(out.size(), kArea.size());
        QCOMPARE(out.devicePixelRatio(), 1.0);
        QVERIFY(near(at(out, 100, 100), m_stroke));
        QCOMPARE(at(out, 150, 100), kGrey);
    }

    void strokeIsClampedToTheSelection()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        drawRect(s, {-1700, 250}, {-1300, 600});   // end lies outside the selection

        // Wider crop: virtual (-1900,150), so the selection's right edge is crop x 499.
        const QRect wide(-1900, 150, 700, 500);
        const QPixmap out = s.flattenedCrop(wide);
        QVERIFY(near(at(out, 499, 225), m_stroke));
        QCOMPARE(at(out, 600, 225), kGrey);   // where the unclamped edge would be
    }

    void pressOutsideTheSelectionDoesNotDraw()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("rectangle");
        QVERIFY(!s.press({-1900, 150}));
        QVERIFY(!s.isDrawing());
    }

    void clearEmptiesEverything()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        drawRect(s);
        drawRect(s, {-1750, 220}, {-1650, 300});
        s.undo();   // one redo-able item too

        s.clear();
        QVERIFY(!s.hasItems());
        QVERIFY(!s.canUndo());
        QVERIFY(!s.canRedo());
        QVERIFY(allGrey(s.flattenedCrop(kArea)));
    }

    void typedTextCountsAndCommitsOnTheNextPress()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("text");
        QVERIFY(s.press({-1700, 250}));
        QVERIFY(s.isEditingText());
        QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
        s.forwardKey(&key);
        QVERIFY(s.hasItems());
        QVERIFY(!s.canUndo());

        QVERIFY(s.press({-1650, 300}));   // ends typing instead of placing a second box
        QVERIFY(!s.isEditingText());
        QVERIFY(s.canUndo());
        s.undo();
        QVERIFY(!s.hasItems());
    }

    void clearDiscardsTextBeingTyped()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("text");
        QVERIFY(s.press({-1700, 250}));
        QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
        s.forwardKey(&key);

        s.clear();
        QVERIFY(!s.isEditingText());
        QVERIFY(!s.hasItems());
        QVERIFY(!s.canRedo());
        QTest::qWait(10);   // any deferred focus-out finalization must find nothing
        QVERIFY(!s.canUndo());
    }

    void drawingEmitsChanged()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("rectangle");
        QSignalSpy spy(&s, &OverlayAnnotations::changed);
        QVERIFY(s.press({-1700, 250}));
        QVERIFY(s.move({-1600, 350}));
        QVERIFY(spy.wait(1000));   // the live preview repaints before anything is committed
        const int before = spy.count();
        QVERIFY(s.release({-1600, 350}));
        QVERIFY(spy.count() > before);
    }

    void blurSamplesTheFrameUnderTheStroke()
    {
        // Black left of frame x 320 (virtual -1600), white right of it.
        QPixmap pm = frame(1);
        {
            QPainter p(&pm);
            p.fillRect(QRect(0, 0, 320, 600), Qt::black);
            p.fillRect(QRect(320, 0, 480, 600), Qt::white);
        }
        OverlayAnnotations s(pm, kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("blur");
        QVERIFY(s.press({-1650, 350}));
        for (int x = -1640; x <= -1550; x += 10)
            QVERIFY(s.move({x, 350}));
        QVERIFY(s.release({-1550, 350}));

        // Crop x 200 is the edge: blurring the right pixels softens both sides of it,
        // while a patch sampled from the wrong place would show the grey placeholder.
        const QPixmap out = s.flattenedCrop(kArea);
        QVERIFY(at(out, 196, 150).lightness() > 60);
        QVERIFY(at(out, 204, 150).lightness() < 195);
        QVERIFY(at(out, 170, 150).lightness() < 10);
        QVERIFY(at(out, 230, 150).lightness() > 245);
        QCOMPARE(at(out, 150, 20), QColor(Qt::black));   // far from the stroke
    }

    void blurSurvivesTeardownMidStroke()
    {
        Editor::AnnotationSet set;
        {
            OverlayAnnotations s(frame(1), kVirtual);
            s.setSelection(kArea);
            s.setActiveTool("blur");
            QVERIFY(s.press({-1700, 250}));
            QVERIFY(s.move({-1650, 260}));
            QVERIFY(s.release({-1650, 260}));
            set = s.snapshot(kArea);
            QVERIFY(s.press({-1700, 300}));
            QVERIFY(s.move({-1650, 310}));
        }
        QTest::qWait(120);   // past the blur's coalescing interval
        QCOMPARE(set.size(), 1);
        QCOMPARE(set.entries().at(0).toolId, QStringLiteral("blur"));
    }

    void stepsCountOnAndReuseAnUndoneNumber()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("step");
        QVERIFY(s.press({-1750, 250}));
        QVERIFY(!s.isDrawing());   // a click stamps, there is no stroke to finish
        QVERIFY(s.press({-1700, 250}));
        QCOMPARE(stepNumbers(s.snapshot(kArea)), QList<int>({1, 2}));

        s.undo();
        QVERIFY(s.press({-1650, 250}));
        QCOMPARE(stepNumbers(s.snapshot(kArea)), QList<int>({1, 2}));
        QCOMPARE(s.activeTool(), QStringLiteral("step"));
    }

    void highlightIsBlendedIntoTheCrop()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("highlight");
        QVERIFY(s.press({-1700, 250}));
        for (int x = -1690; x <= -1600; x += 10)
            QVERIFY(s.move({x, 250}));
        QVERIFY(s.release({-1600, 250}));
        QVERIFY(s.canUndo());

        const QPixmap out = s.flattenedCrop(kArea);
        const QColor blended = highlightOverGrey();
        QVERIFY(blended != kGrey);
        QVERIFY(approx(at(out, 150, 50), blended));
        QVERIFY(approx(at(out, 150, 58), blended));   // inside the wide stroke
        QCOMPARE(at(out, 150, 90), kGrey);
    }

    void snapshotCarriesStepsAndHighlights()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("step");
        QVERIFY(s.press({-1750, 250}));
        s.setActiveTool("highlight");
        QVERIFY(s.press({-1700, 300}));
        QVERIFY(s.move({-1650, 300}));
        QVERIFY(s.release({-1650, 300}));
        s.setActiveTool("step");
        QVERIFY(s.press({-1600, 250}));

        const Editor::AnnotationSet set = s.snapshot(kArea);
        QCOMPARE(set.size(), 3);
        QCOMPARE(set.entries().at(0).toolId, QStringLiteral("step"));
        QCOMPARE(set.entries().at(1).toolId, QStringLiteral("highlight"));
        QCOMPARE(set.entries().at(2).toolId, QStringLiteral("step"));
        QCOMPARE(stepNumbers(set), QList<int>({1, 2}));
        QCOMPARE(set.entries().at(2).pos, QPointF(200, 50));
        auto *hl = dynamic_cast<Editor::Tools::HighlightTool*>(set.entries().at(1).prototype.get());
        QVERIFY(hl);
        QCOMPARE(set.entries().at(1).pos + hl->points().first(), QPointF(100, 100));
    }

    void snapshotIsRelativeToTheArea()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        drawRect(s);
        s.setActiveTool("step");
        QVERIFY(s.press({-1750, 400}));
        s.setActiveTool("arrow");
        QVERIFY(s.press({-1780, 220}));
        QVERIFY(s.move({-1700, 300}));
        QVERIFY(s.release({-1700, 300}));

        const Editor::AnnotationSet set = s.snapshot(kArea);
        QCOMPARE(set.size(), 3);
        const auto &e = set.entries();
        QCOMPARE(e.at(0).toolId, QStringLiteral("rectangle"));
        QCOMPARE(e.at(1).toolId, QStringLiteral("step"));
        QCOMPARE(e.at(2).toolId, QStringLiteral("arrow"));

        // Geometry is scene-absolute with pos 0, so pos carries the area offset.
        auto *rect = dynamic_cast<Editor::Tools::RectangleTool*>(e.at(0).prototype.get());
        QVERIFY(rect);
        QCOMPARE(e.at(0).pos + rect->shapeRect().topLeft(), QPointF(100, 50));
        QCOMPARE(rect->shapeRect().size(), QSizeF(101, 101));   // QRect corners are inclusive

        auto *step = dynamic_cast<Editor::Tools::StepTool*>(e.at(1).prototype.get());
        QVERIFY(step);
        QCOMPARE(e.at(1).pos, QPointF(50, 200));   // a badge's pos is its centre
        QCOMPARE(step->number(), 1);

        auto *arrow = dynamic_cast<Editor::Tools::ArrowTool*>(e.at(2).prototype.get());
        QVERIFY(arrow);
        QCOMPARE(e.at(2).pos + arrow->startPoint(), QPointF(20, 20));
        QCOMPARE(e.at(2).pos + arrow->endPoint(), QPointF(100, 100));
    }

    void snapshotDropsItemsOutsideTheArea()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        drawRect(s);
        QCOMPARE(s.snapshot(QRect(-1500, 450, 200, 200)).size(), 0);
        QCOMPARE(s.snapshot(kArea).size(), 1);
    }

    void snapshotSkipsUndoneItems()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        QVERIFY(s.snapshot(kArea).isEmpty());

        s.setSelection(kArea);
        drawRect(s);
        drawRect(s, {-1750, 220}, {-1650, 300});
        s.undo();
        const Editor::AnnotationSet set = s.snapshot(kArea);
        QCOMPARE(set.size(), 1);
        auto *rect = dynamic_cast<Editor::Tools::RectangleTool*>(set.entries().at(0).prototype.get());
        QVERIFY(rect);
        QCOMPARE(rect->shapeRect().size(), QSizeF(101, 101));
    }

    void cropAwayFromTheDrawingIsPlain()
    {
        OverlayAnnotations s(frame(1), kVirtual);
        s.setSelection(kArea);
        drawRect(s);
        QVERIFY(allGrey(s.flattenedCrop(QRect(-1500, 450, 200, 200))));
    }
};

QTEST_MAIN(tst_OverlayAnnotations)
#include "tst_overlayannotations.moc"
