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

// The editor window is Editor::Image::ImageEditor; alias it for brevity (a plain
// `using namespace` would clash with the Editor namespace).
using ImageEditorWindow = Editor::Image::ImageEditor;

// Smoke test: the full editor window constructs offscreen (loads its qss + themed
// SVG icons from the qrc that's linked into this test target) without crashing.
class tst_ImageEditorSmoke : public QObject
{
    Q_OBJECT

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

        const Editor::ToolSpec *spec = Editor::ToolRegistry::find("rectangle");
        QVERIFY(spec);
        QAction *rectAction = nullptr;
        for (QAction *a : toolbar->actions())
            if (a->toolTip() == spec->tooltip)
                rectAction = a;
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
};

QTEST_MAIN(tst_ImageEditorSmoke)
#include "tst_imageeditor_smoke.moc"
