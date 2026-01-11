#include <QtTest>
#include <QStandardPaths>
#include <QPixmap>
#include <QStatusBar>

#include "editor/image/ImageEditor.h"
#include "editor/DrawingGraphicsView.h"
#include "editor/LayerManager.h"

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
        QCoreApplication::setOrganizationName("NiceshotTest");
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
};

QTEST_MAIN(tst_ImageEditorSmoke)
#include "tst_imageeditor_smoke.moc"
