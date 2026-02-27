#include <QtTest>
#include <QLabel>
#include <QStandardPaths>
#include <QVideoWidget>

#include "editor/video/TrimTimeline.h"
#include "editor/video/VideoEditor.h"

using namespace Editor::Video;

// Construction smoke test on the preview-error path: the editor is pointed at a
// nonexistent file (offscreen CI has no decodable media anyway) and must still
// come up with its chrome intact and Save usable - saving is a plain file move
// that never decodes. The widget is deleted directly (not close()d), so the
// interactive "Discard this recording?" prompt never runs headless.
class tst_VideoEditorSmoke : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_videoeditor_smoke");
        QStandardPaths::setTestModeEnabled(true);
    }

    void constructsWithChromeOnErrorPath()
    {
        const QString missing = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                + "/Snim_recording_does-not-exist.mp4";
        auto *editor = new VideoEditor(missing);

        QVERIFY(editor->findChild<TrimTimeline *>() != nullptr);
        QVERIFY(editor->findChild<QVideoWidget *>() != nullptr);
        QVERIFY(!editor->findChildren<QLabel *>().isEmpty());

        // Save / Copy / Discard must exist and stay enabled even while the player
        // is failing to load the missing media. The icon actions are tooltip-only
        // (matching the image editor's toolbar idiom), so probe tooltips; Discard is
        // the one text button.
        const auto actions = editor->findChildren<QAction *>();
        bool sawSave = false, sawCopy = false, sawDiscard = false;
        for (const QAction *a : actions) {
            if (a->toolTip().startsWith("Save As")) { sawSave = true; QVERIFY(a->isEnabled()); }
            if (a->toolTip().contains("copy the file")) { sawCopy = true; QVERIFY(a->isEnabled()); }
            if (a->text() == "Discard") { sawDiscard = true; QVERIFY(a->isEnabled()); }
        }
        QVERIFY(sawSave);
        QVERIFY(sawCopy);
        QVERIFY(sawDiscard);

        // GIF encoding is macOS-only; elsewhere the action must not invite a sure failure.
        bool sawGif = false;
        for (const QAction *a : actions) {
            if (!a->toolTip().contains("GIF"))
                continue;
            sawGif = true;
#ifndef Q_OS_MACOS
            QVERIFY(!a->isEnabled());
#endif
        }
        QVERIFY(sawGif);

        // Let queued player error signals deliver; the editor must absorb them.
        QTest::qWait(50);

        delete editor;   // direct delete: no closeEvent, no modal discard prompt
    }
};

QTEST_MAIN(tst_VideoEditorSmoke)
#include "tst_videoeditor_smoke.moc"
