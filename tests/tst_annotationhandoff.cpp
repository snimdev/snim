#include <QtTest>
#include <QClipboard>
#include <QGuiApplication>
#include <QPixmap>
#include <QStandardPaths>

#include "capture/CaptureGeometry.h"
#include "capture/OverlayAnnotations.h"
#include "capture/strategies/CaptureStrategy.h"
#include "editor/annotations/Layer.h"
#include "editor/annotations/LayerManager.h"
#include "editor/annotations/tools/RectangleTool.h"
#include "editor/annotations/tools/StepTool.h"
#include "editor/annotations/tools/TextTool.h"
#include "editor/image/ImageEditor.h"

using Capture::OverlayAnnotations;

namespace {

// Exposes the terminal actions every real strategy calls after area selection.
class ProbeStrategy : public Capture::CaptureStrategy
{
public:
    using CaptureStrategy::copyAreaToClipboard;
    using CaptureStrategy::emitSelection;

    void captureFullScreen() override {}
    void captureArea() override {}
    void captureWindow() override {}
    QString name() const override { return QStringLiteral("probe"); }
};

struct Emitted {
    int count = 0;
    QPixmap pixmap;
    Editor::AnnotationSet annotations;
    QString error;
};

void record(ProbeStrategy &strategy, Emitted *out)
{
    QObject::connect(&strategy, &Capture::CaptureStrategy::screenshotReady, &strategy,
                     [out](const QPixmap &pm, const Editor::AnnotationSet &set) {
                         ++out->count;
                         out->pixmap = pm;
                         out->annotations = set;
                     });
    QObject::connect(&strategy, &Capture::CaptureStrategy::screenshotFailed, &strategy,
                     [out](const QString &error) { out->error = error; });
}

} // namespace

// Overlay drawing -> snapshot -> editor layers, with a non-zero desktop origin.
class tst_AnnotationHandoff : public QObject
{
    Q_OBJECT

    const QRect kVirtual{-1920, 100, 800, 600};
    const QRect kArea{-1800, 200, 400, 300};   // scene (120,100), crop-local origin
    const QColor kGrey{128, 128, 128};

    static QPixmap frame()
    {
        QPixmap pm(800, 600);
        pm.fill(QColor(128, 128, 128));
        return pm;
    }

    static bool near(const QColor &a, const QColor &b)
    {
        return qAbs(a.red() - b.red()) < 40 && qAbs(a.green() - b.green()) < 40
               && qAbs(a.blue() - b.blue()) < 40;
    }

    // Rectangle virtual (-1700,250)-(-1600,350) is crop (100,50)-(200,150); step at crop (50,200).
    static void draw(OverlayAnnotations &s)
    {
        s.setActiveTool("rectangle");
        QVERIFY(s.press({-1700, 250}));
        QVERIFY(s.move({-1600, 350}));
        QVERIFY(s.release({-1600, 350}));
        s.setActiveTool("step");
        QVERIFY(s.press({-1750, 400}));
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_annotationhandoff");
        QStandardPaths::setTestModeEnabled(true);   // no default backdrop preset
    }

    void editorShowsTheOverlayDrawing()
    {
        OverlayAnnotations s(frame(), kVirtual);
        s.setSelection(kArea);
        draw(s);
        const QPixmap flattened = s.flattenedCrop(kArea);

        Editor::Image::ImageEditor editor(Capture::cropVirtualArea(frame(), kVirtual, kArea));
        editor.importAnnotations(s.snapshot(kArea));

        auto *manager = editor.findChild<Editor::LayerManager*>();
        QVERIFY(manager);
        const QList<Editor::Layer*> layers = manager->layers();
        QCOMPARE(layers.size(), 3);

        auto *rect = dynamic_cast<Editor::Tools::RectangleTool*>(layers.at(1)->item());
        QVERIFY(rect);
        QCOMPARE(rect->mapRectToScene(rect->shapeRect()), QRectF(100, 50, 101, 101));
        // The painted outline, not boundingRect(), which pads for the resize handles.
        const QRectF bounds = rect->mapToScene(rect->shape()).boundingRect();
        const qreal slack = rect->pen().widthF() / 2 + 2;
        QVERIFY(qAbs(bounds.left() - 100) <= slack && qAbs(bounds.top() - 50) <= slack);
        QVERIFY(qAbs(bounds.right() - 201) <= slack && qAbs(bounds.bottom() - 151) <= slack);

        auto *step = dynamic_cast<Editor::Tools::StepTool*>(layers.at(2)->item());
        QVERIFY(step);
        QCOMPARE(step->pos(), QPointF(50, 200));
        QCOMPARE(step->number(), 1);

        editor.copyToClipboard();
        const QImage out = QGuiApplication::clipboard()->pixmap().toImage();
        QCOMPARE(out.size(), kArea.size());
        const QColor stroke = rect->pen().color();
        QVERIFY(near(out.pixelColor(100, 100), stroke));
        QVERIFY(near(out.pixelColor(200, 100), stroke));
        QCOMPARE(out.pixelColor(150, 100), kGrey);
        QVERIFY(near(out.pixelColor(39, 200), step->color()));   // off the numeral

        // The layers render the same as the flattened overlay at every sampled pixel.
        const QImage flat = flattened.toImage();
        for (const QPoint p : {QPoint(100, 100), QPoint(200, 100), QPoint(150, 50),
                               QPoint(150, 100), QPoint(39, 200), QPoint(50, 200),
                               QPoint(300, 250)})
            QVERIFY2(near(out.pixelColor(p), flat.pixelColor(p)),
                     qPrintable(QStringLiteral("at %1,%2").arg(p.x()).arg(p.y())));
    }

    void editGetsPlainCropAndLayers()
    {
        OverlayAnnotations s(frame(), kVirtual);
        s.setSelection(kArea);
        draw(s);
        const QSharedPointer<OverlayAnnotations> session(&s, [](OverlayAnnotations *) {});

        ProbeStrategy strategy;
        Emitted got;
        record(strategy, &got);
        strategy.emitSelection(frame(), kVirtual, kArea, session);

        QCOMPARE(got.count, 1);
        QCOMPARE(got.pixmap.size(), kArea.size());
        QCOMPARE(got.pixmap.toImage().pixelColor(100, 100), kGrey);   // nothing burned in
        QCOMPARE(got.annotations.size(), 2);
        QCOMPARE(got.annotations.entries().at(0).toolId, QStringLiteral("rectangle"));
        QCOMPARE(got.annotations.entries().at(1).toolId, QStringLiteral("step"));
    }

    void copyStaysFlattened()
    {
        OverlayAnnotations s(frame(), kVirtual);
        s.setSelection(kArea);
        draw(s);
        const QSharedPointer<OverlayAnnotations> session(&s, [](OverlayAnnotations *) {});

        ProbeStrategy strategy;
        strategy.copyAreaToClipboard(frame(), kVirtual, kArea, session);
        const QImage out = QGuiApplication::clipboard()->pixmap().toImage();
        QCOMPARE(out.size(), kArea.size());
        QVERIFY(!near(out.pixelColor(100, 100), kGrey));
    }

    void typedTextBecomesATextLayer()
    {
        OverlayAnnotations s(frame(), kVirtual);
        s.setSelection(kArea);
        s.setActiveTool("text");
        QVERIFY(s.press({-1700, 250}));
        for (const QChar c : QStringLiteral("Hi\ryo")) {
            QKeyEvent key(QEvent::KeyPress, c == u'\r' ? int(Qt::Key_Return) : int(c.toUpper().unicode()),
                          c.isUpper() ? Qt::ShiftModifier : Qt::NoModifier, QString(c));
            s.forwardKey(&key);
        }
        QVERIFY(s.isEditingText());   // Edit is chosen mid-typing
        const QSharedPointer<OverlayAnnotations> session(&s, [](OverlayAnnotations *) {});

        ProbeStrategy strategy;
        Emitted got;
        record(strategy, &got);
        strategy.emitSelection(frame(), kVirtual, kArea, session);
        QCOMPARE(got.count, 1);
        QCOMPARE(got.annotations.size(), 1);

        Editor::Image::ImageEditor editor(got.pixmap);
        editor.importAnnotations(got.annotations);
        auto *manager = editor.findChild<Editor::LayerManager*>();
        QVERIFY(manager);
        const QList<Editor::Layer*> layers = manager->layers();
        QCOMPARE(layers.size(), 2);
        auto *text = dynamic_cast<Editor::Tools::TextTool*>(layers.at(1)->item());
        QVERIFY(text);
        QCOMPARE(text->toPlainText(), QStringLiteral("Hi\nyo"));
        QCOMPARE(text->pos(), QPointF(100, 50));
        QCOMPARE(text->textInteractionFlags(), Qt::NoTextInteraction);
        QCOMPARE(layers.at(1)->type(), Editor::Layer::Text);
    }

    void editWithoutDrawingHandsNoLayers()
    {
        OverlayAnnotations s(frame(), kVirtual);
        const QSharedPointer<OverlayAnnotations> session(&s, [](OverlayAnnotations *) {});

        ProbeStrategy strategy;
        Emitted got;
        record(strategy, &got);
        strategy.emitSelection(frame(), kVirtual, kArea, session);
        strategy.emitSelection(frame(), kVirtual, kArea, {});
        QCOMPARE(got.count, 2);
        QVERIFY(got.annotations.isEmpty());
        QVERIFY(got.error.isEmpty());
    }

    void editOfNothingFails()
    {
        ProbeStrategy strategy;
        Emitted got;
        record(strategy, &got);
        strategy.emitSelection(QPixmap(), kVirtual, kArea, {});
        QCOMPARE(got.count, 0);
        QVERIFY(!got.error.isEmpty());
    }
};

QTEST_MAIN(tst_AnnotationHandoff)
#include "tst_annotationhandoff.moc"
