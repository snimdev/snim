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
                                                       false));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(true, DesktopIntegration::Status::ExecMismatch,
                                                       false));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(true, DesktopIntegration::Status::Installed,
                                                       true));
    }

    void promptApplicable_neverWhenTheEntryIsInstalled()
    {
        // The refusal has another cause, so offering to rewrite the entry helps nobody.
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::Installed,
                                                       false));
    }

    void promptApplicable_neverWhenNotApplicable()
    {
        // Off Linux or inside Flatpak there is no entry for Snim to write.
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::NotApplicable,
                                                       false));
    }

    void promptApplicable_neverForAStaleUserEntry()
    {
        // Removing it is offered at startup; writing over it would only add another copy.
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::StaleUserEntry,
                                                       false));
    }

    void promptApplicable_onlyOncePerRun()
    {
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::NotInstalled,
                                                       true));
        QVERIFY(!CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::ExecMismatch,
                                                       true));
    }

    void promptApplicable_whenTheEntryCannotAuthorize()
    {
        QVERIFY(CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::NotInstalled,
                                                      false));
        QVERIFY(CaptureWorkflow::kwinPromptApplicable(false, DesktopIntegration::Status::ExecMismatch,
                                                      false));
        QVERIFY(CaptureWorkflow::kwinPromptApplicable(
            false, DesktopIntegration::Status::MissingAuthorizationKey, false));
    }
};

QTEST_MAIN(tst_CaptureWorkflow)
#include "tst_screenshotapp.moc"
