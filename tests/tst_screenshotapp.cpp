#include <QtTest>

#include "core/ScreenshotApp.h"

using namespace Core;

// Pure-logic slice of the app shell. The app itself is never constructed here: it
// builds a tray, capture strategy and hotkey backends, none of which belong in a test.
class tst_ScreenshotApp : public QObject
{
    Q_OBJECT

private slots:
    void deferPrompt_whileTheSelectionOverlayIsUp()
    {
        // The overlay is a layer surface with exclusive keyboard focus, so a modal
        // box opened behind it cannot be reached.
        QVERIFY(ScreenshotApp::shouldDeferPrompt(true, false));
    }

    void deferPrompt_whileRecording()
    {
        QVERIFY(ScreenshotApp::shouldDeferPrompt(false, true));
        QVERIFY(ScreenshotApp::shouldDeferPrompt(true, true));
    }

    void deferPrompt_notWhenTheCaptureFlowIsOver()
    {
        // An open editor is ordinary stacking, not a trap, so nothing defers here.
        QVERIFY(!ScreenshotApp::shouldDeferPrompt(false, false));
    }
};

QTEST_MAIN(tst_ScreenshotApp)
#include "tst_screenshotapp.moc"
