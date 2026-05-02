#include <QtTest>

#include "core/DesktopIntegration.h"
#include "app/CaptureWorkflow.h"

using namespace Core;
using App::CaptureWorkflow;

// Pure-logic slice of the capture flow. The workflow itself is never constructed here:
// it builds a capture strategy and a tray action, neither of which this test needs.
class tst_CaptureWorkflow : public QObject
{
    Q_OBJECT

private slots:
    void promptApplicable_neverAfterDismissal()
    {
        // "Never ask again" outranks every other fact, including a broken entry.
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(true, DesktopIntegration::Status::NotInstalled,
                                                       false, false));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(true, DesktopIntegration::Status::ExecMismatch,
                                                       false, false));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(true, DesktopIntegration::Status::Installed,
                                                       true, false));
    }

    void promptApplicable_neverWhenTheEntryIsInstalled()
    {
        // The refusal has another cause, so offering to rewrite the entry helps nobody.
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::Installed,
                                                       false, false));
    }

    void promptApplicable_onlyOncePerRun()
    {
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::NotInstalled,
                                                       true, false));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::ExecMismatch,
                                                       true, false));
    }

    void promptApplicable_neverUnderAnAppImage()
    {
        // KWin resolves the caller's /proc/PID/exe, which an AppImage remounts somewhere
        // new every launch, so no desktop entry can ever match it.
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::NotInstalled,
                                                       false, true));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::ExecMismatch,
                                                       false, true));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(
            false, DesktopIntegration::Status::MissingAuthorizationKey, false, true));
    }

    void promptApplicable_whenTheEntryCannotAuthorize()
    {
        QVERIFY(CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::NotInstalled,
                                                      false, false));
        QVERIFY(CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::ExecMismatch,
                                                      false, false));
        QVERIFY(CaptureWorkflow::kwinPromptApplicable(
            false, DesktopIntegration::Status::MissingAuthorizationKey, false, false));
    }
};

QTEST_MAIN(tst_CaptureWorkflow)
#include "tst_screenshotapp.moc"
