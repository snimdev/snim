#include <QtTest>

#include <QPainter>
#include <QPixmap>
#include <QSignalSpy>
#include <memory>

#include "capture/OverlayAnnotations.h"
#include "editor/annotations/ToolRegistry.h"
#include "editor/annotations/tools/RectangleTool.h"

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
