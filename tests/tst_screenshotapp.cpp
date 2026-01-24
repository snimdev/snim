#include <QtTest>

#include "core/DesktopIntegration.h"
#include "core/ScreenshotApp.h"

using namespace Core;

// Pure-logic slice of the app shell. The app itself is never constructed here: it
// builds a tray, capture strategy and hotkey backends, none of which belong in a test.
class tst_ScreenshotApp : public QObject
{
    Q_OBJECT

private slots:
    void promptApplicable_neverAfterDismissal()
    {
        // "Never ask again" outranks every other fact, including a broken entry.
        QVERIFY(!ScreenshotApp::kwinPromptApplicable(true, DesktopIntegration::Status::NotInstalled, false));
        QVERIFY(!ScreenshotApp::kwinPromptApplicable(true, DesktopIntegration::Status::ExecMismatch, false));
        QVERIFY(!ScreenshotApp::kwinPromptApplicable(true, DesktopIntegration::Status::Installed, true));
    }

    void promptApplicable_neverWhenTheEntryIsInstalled()
    {
        // The refusal has another cause, so offering to rewrite the entry helps nobody.
        QVERIFY(!ScreenshotApp::kwinPromptApplicable(false, DesktopIntegration::Status::Installed, false));
    }

    void promptApplicable_onlyOncePerRun()
    {
        QVERIFY(!ScreenshotApp::kwinPromptApplicable(false, DesktopIntegration::Status::NotInstalled, true));
        QVERIFY(!ScreenshotApp::kwinPromptApplicable(false, DesktopIntegration::Status::ExecMismatch, true));
    }

    void promptApplicable_whenTheEntryCannotAuthorize()
    {
        QVERIFY(ScreenshotApp::kwinPromptApplicable(false, DesktopIntegration::Status::NotInstalled, false));
        QVERIFY(ScreenshotApp::kwinPromptApplicable(false, DesktopIntegration::Status::ExecMismatch, false));
        QVERIFY(ScreenshotApp::kwinPromptApplicable(false, DesktopIntegration::Status::MissingAuthorizationKey,
                                                    false));
    }
};

QTEST_MAIN(tst_ScreenshotApp)
#include "tst_screenshotapp.moc"
