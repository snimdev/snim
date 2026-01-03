#include <QtTest>
#include <QStandardPaths>
#include <QPixmap>

#include "editor/ImageEditor.h"
#include "editor/DrawingGraphicsView.h"
#include "editor/LayerManager.h"

// The editor class is named ImageEditor inside namespace ImageEditor, so a
// `using namespace` makes the bare name ambiguous; alias the class instead.
using Editor = ImageEditor::ImageEditor;

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

        Editor editor(shot);   // not shown; just constructed
        QVERIFY(editor.findChild<ImageEditor::DrawingGraphicsView*>() != nullptr);
        QVERIFY(editor.findChild<ImageEditor::LayerManager*>() != nullptr);
    }
};

QTEST_MAIN(tst_ImageEditorSmoke)
#include "tst_imageeditor_smoke.moc"
