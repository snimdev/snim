#include <QtTest>
#include <QLabel>
#include <QStandardPaths>
#include <QMenu>
#include <QToolButton>
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
                                + "/snimcapture-does-not-exist.mp4";
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

        // GIF and WebP share one export path, so neither is gated by platform.
        QAction *gif = nullptr;
        QAction *webp = nullptr;
        for (QAction *a : actions) {
            if (a->toolTip().contains("animated GIF"))
                gif = a;
            if (a->toolTip().contains("animated WebP"))
                webp = a;
        }
        QVERIFY(gif != nullptr);
        QVERIFY(webp != nullptr);

        // Let queued player error signals deliver; the editor must absorb them.
        QTest::qWait(50);

        // Once the preview has failed there are no frames to grab, so both animations go
        // off together. A backend that never reports the failure leaves both on.
        const bool previewFailed = QTest::qWaitFor([editor] {
            for (const QLabel *label : editor->findChildren<QLabel *>())
                if (label->text().startsWith("Preview unavailable"))
                    return true;
            return false;
        }, 2000);
        if (previewFailed)
            QVERIFY(!gif->isEnabled());
        QCOMPARE(gif->isEnabled(), webp->isEnabled());

        // Nothing decodes from a missing file, so there is no frame to grab.
        QAction *frame = nullptr;
        for (QAction *a : editor->findChildren<QAction *>())
            if (a->toolTip().contains("current frame"))
                frame = a;
        QVERIFY(frame != nullptr);
        QVERIFY(!frame->isEnabled());

        // GIF and WebP are split buttons: click exports, the menu copies, uploads, or tunes.
        int animationButtons = 0;
        for (const QToolButton *b : editor->findChildren<QToolButton *>()) {
            const QString tip = b->defaultAction() ? b->defaultAction()->toolTip() : QString();
            if (!tip.contains("GIF") && !tip.contains("WebP"))
                continue;
            ++animationButtons;
            QCOMPARE(b->popupMode(), QToolButton::MenuButtonPopup);
            QVERIFY(b->menu() != nullptr);
            QStringList entries;
            for (const QAction *a : b->menu()->actions())
                entries << a->text();
            QVERIFY(entries.contains("Copy"));
            QVERIFY(entries.contains("Upload"));
            QVERIFY(entries.contains(QStringLiteral("Export options…")));
        }
        QCOMPARE(animationButtons, 2);

        delete editor;   // direct delete: no closeEvent, no modal discard prompt
    }
};

QTEST_MAIN(tst_VideoEditorSmoke)
#include "tst_videoeditor_smoke.moc"
