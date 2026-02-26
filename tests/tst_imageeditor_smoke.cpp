#include <QtTest>
#include <QStandardPaths>
#include <QPixmap>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>

#include "editor/image/ImageEditor.h"
#include "editor/annotations/DrawingGraphicsView.h"
#include "editor/annotations/LayerManager.h"
#include "editor/annotations/Layer.h"
#include "editor/annotations/ToolRegistry.h"
#include "editor/annotations/AnnotationSet.h"
#include "editor/annotations/tools/BlurTool.h"
#include "editor/annotations/tools/RectangleTool.h"
#include "editor/annotations/tools/StepTool.h"
#include "editor/annotations/tools/TextTool.h"

// The editor window is Editor::Image::ImageEditor; alias it for brevity (a plain
// `using namespace` would clash with the Editor namespace).
using ImageEditorWindow = Editor::Image::ImageEditor;

// Smoke test: the full editor window constructs offscreen (loads its qss + themed
// SVG icons from the qrc that's linked into this test target) without crashing.
class tst_ImageEditorSmoke : public QObject
{
    Q_OBJECT

    static QAction *toolAction(QToolBar *toolbar, const QString &id)
    {
        const Editor::ToolSpec *spec = Editor::ToolRegistry::find(id);
        const QString tip = QStringLiteral("%1 (%2)").arg(spec->tooltip, QString(spec->shortcut));
        for (QAction *a : toolbar->actions())
            if (a->toolTip() == tip)
                return a;
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_imageeditor_smoke");
        QStandardPaths::setTestModeEnabled(true);   // no real default-backdrop preset applied
    }

    void constructsWithExpectedChildren()
    {
        QPixmap shot(400, 300);
        shot.fill(Qt::darkGray);

        ImageEditorWindow editor(shot);   // not shown; just constructed
        QVERIFY(editor.findChild<Editor::DrawingGraphicsView*>() != nullptr);
        QVERIFY(editor.findChild<Editor::LayerManager*>() != nullptr);
    }

    // Export feedback goes to the status bar, so it must never block the window.
    void copyToClipboardIsNonModal()
    {
        QPixmap shot(400, 300);
        shot.fill(Qt::darkGray);

        ImageEditorWindow editor(shot);
        editor.copyToClipboard();   // public slot; must not open a dialog
        QVERIFY(QApplication::activeModalWidget() == nullptr);
        QVERIFY(editor.statusBar() != nullptr);
        QVERIFY(!editor.statusBar()->currentMessage().isEmpty());
    }

    // A drag with the Rectangle tool goes view -> interaction -> builder -> layer sink.
    void drawRectangleThroughView()
    {
        QPixmap shot(400, 300);
        shot.fill(Qt::darkGray);

        ImageEditorWindow editor(shot);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));

        auto *view = editor.findChild<Editor::DrawingGraphicsView*>();
        auto *layers = editor.findChild<Editor::LayerManager*>();
        auto *stack = editor.findChild<QUndoStack*>();
        auto *toolbar = editor.findChild<QToolBar*>();
        QVERIFY(view && layers && stack && toolbar);
        QCOMPARE(layers->layers().size(), 1);
        QCOMPARE(stack->count(), 0);

        QAction *rectAction = toolAction(toolbar, "rectangle");
        QVERIFY(rectAction);
        rectAction->trigger();
        QVERIFY(rectAction->isChecked());
        QTest::qWait(50);   // the tool reveals the side panel, which re-lays out the view

        const QPoint from = view->mapFromScene(QPointF(50, 50));
        const QPoint to = view->mapFromScene(QPointF(150, 120));
        QWidget *vp = view->viewport();
        QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(vp, (from + to) / 2);
        QTest::mouseMove(vp, to);
        QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, to);

        QCOMPARE(layers->layers().size(), 2);
        int rectangles = 0;
        for (Editor::Layer *l : layers->layers())
            rectangles += l->type() == Editor::Layer::Rectangle;
        QCOMPARE(rectangles, 1);
        QCOMPARE(stack->count(), 1);
    }

    // Bare letters pick tools; the rectangle key then draws through the view.
    void letterKeyPicksATool()
    {
        QPixmap shot(400, 300);
        shot.fill(Qt::darkGray);

        ImageEditorWindow editor(shot);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        auto *view = editor.findChild<Editor::DrawingGraphicsView*>();
        auto *layers = editor.findChild<Editor::LayerManager*>();
        auto *toolbar = editor.findChild<QToolBar*>();
        QVERIFY(view && layers && toolbar);
        QAction *pointer = toolAction(toolbar, "pointer");
        QAction *rect = toolAction(toolbar, "rectangle");
        QVERIFY(pointer && rect);
        QCOMPARE(pointer->toolTip(), QStringLiteral("Pointer (V)"));

        QTest::keyClick(view->viewport(), Qt::Key_R);
        QVERIFY(rect->isChecked());
        QTest::qWait(50);   // the tool reveals the side panel, which re-lays out the view

        const QPoint from = view->mapFromScene(QPointF(50, 50));
        const QPoint to = view->mapFromScene(QPointF(150, 120));
        QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(view->viewport(), to);
        QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, to);
        QCOMPARE(layers->layers().size(), 2);
        QCOMPARE(layers->layers().last()->type(), Editor::Layer::Rectangle);

        QTest::keyClick(view->viewport(), Qt::Key_V);
        QVERIFY(pointer->isChecked());
        QTest::keyClick(view->viewport(), Qt::Key_R, Qt::ControlModifier);
        QVERIFY(pointer->isChecked());
    }

    void lettersTypedIntoTextDoNotSwitchTools()
    {
        QPixmap shot(400, 300);
        shot.fill(Qt::darkGray);

        ImageEditorWindow editor(shot);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        auto *view = editor.findChild<Editor::DrawingGraphicsView*>();
        auto *layers = editor.findChild<Editor::LayerManager*>();
        auto *toolbar = editor.findChild<QToolBar*>();
        QVERIFY(view && layers && toolbar);
        QAction *pointer = toolAction(toolbar, "pointer");
        QVERIFY(pointer);

        QTest::keyClick(view->viewport(), Qt::Key_T);
        QTest::qWait(50);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(60, 60)));
        auto *text = dynamic_cast<Editor::Tools::TextTool*>(view->scene()->focusItem());
        QVERIFY(text);
        QVERIFY(pointer->isChecked());   // placing text hands the view back to the pointer

        QTest::keyClicks(view->viewport(), "rebar");
        QCOMPARE(text->toPlainText(), QStringLiteral("rebar"));
        QVERIFY(pointer->isChecked());
        QTest::keyClick(view->viewport(), Qt::Key_Escape);
        QTest::qWait(10);
        QCOMPARE(layers->layers().last()->type(), Editor::Layer::Text);
    }

    // Overlay annotations arrive as ordinary layers that start outside the undo history.
    void importedAnnotationsBecomeLayers()
    {
        QPixmap shot(400, 300);
        shot.fill(Qt::darkGray);

        Editor::AnnotationSet set;
        Editor::Tools::RectangleTool rect(QRectF(0, 0, 60, 40));
        set.add("rectangle", rect, QPointF(20, 30));
        Editor::Tools::StepTool step;
        step.setNumber(1);
        set.add("step", step, QPointF(200, 100));
        Editor::Tools::TextTool text(QStringLiteral("note"));
        set.add("text", text, QPointF(50, 200));
        Editor::Tools::BlurTool blur;
        for (int x = 0; x <= 60; x += 10)
            blur.addPoint(QPointF(x, 0));
        blur.finishPath();
        set.add("blur", blur, QPointF(250, 250));

        ImageEditorWindow editor(shot);
        editor.importAnnotations(set);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));

        auto *view = editor.findChild<Editor::DrawingGraphicsView*>();
        auto *layers = editor.findChild<Editor::LayerManager*>();
        auto *stack = editor.findChild<QUndoStack*>();
        auto *toolbar = editor.findChild<QToolBar*>();
        QVERIFY(view && layers && stack && toolbar);

        const QList<Editor::Layer*> all = layers->layers();
        QCOMPARE(all.size(), 5);
        const QList<Editor::Layer::LayerType> types{
            Editor::Layer::Background, Editor::Layer::Rectangle, Editor::Layer::Step,
            Editor::Layer::Text, Editor::Layer::Blur};
        for (int i = 0; i < types.size(); ++i)
            QCOMPARE(all.at(i)->type(), types.at(i));
        QCOMPARE(all.at(1)->item()->pos(), QPointF(20, 30));
        QCOMPARE(all.at(2)->item()->pos(), QPointF(200, 100));
        QCOMPARE(all.at(3)->item()->pos(), QPointF(50, 200));
        QCOMPARE(all.at(4)->item()->pos(), QPointF(250, 250));
        QCOMPARE(all.at(1)->item()->scene(), view->scene());
        QCOMPARE(all.at(3)->name(), QStringLiteral("Text: note"));
        QCOMPARE(stack->count(), 0);

        // The next stamp continues from the imported badge.
        QAction *stepAction = toolAction(toolbar, "step");
        QVERIFY(stepAction);
        stepAction->trigger();
        QTest::qWait(50);   // the tool reveals the side panel, which re-lays out the view
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->mapFromScene(QPointF(300, 60)));
        QCOMPARE(layers->layers().size(), 6);
        auto *stamped = dynamic_cast<Editor::Tools::StepTool*>(layers->layers().last()->item());
        QVERIFY(stamped);
        QCOMPARE(stamped->number(), 2);
        QCOMPARE(stack->count(), 1);

        // Delete removes an imported layer undoably, and Undo brings it back.
        Editor::Layer *imported = all.at(1);
        layers->selectLayer(imported);
        QTest::keyClick(&editor, Qt::Key_Delete);
        QVERIFY(!layers->layers().contains(imported));
        QVERIFY(!imported->item()->scene());
        stack->undo();
        QVERIFY(layers->layers().contains(imported));
        QCOMPARE(imported->item()->scene(), view->scene());
        stack->undo();   // the stamp
        stack->undo();   // no-op: the imports are not on the stack
        QCOMPARE(layers->layers().size(), 5);
    }
};

QTEST_MAIN(tst_ImageEditorSmoke)
#include "tst_imageeditor_smoke.moc"
