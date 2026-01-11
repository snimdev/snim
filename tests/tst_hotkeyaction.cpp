#include <QtTest>
#include <QSet>
#include <QStandardPaths>

#include "hotkeys/HotkeyAction.h"

using namespace Hotkeys;

// The hotkey action vocabulary: the persisted ids, the labels, and the factory
// defaults. Pure lookups, no settings involved.
class tst_HotkeyAction : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_hotkeyaction");
        QStandardPaths::setTestModeEnabled(true);
    }

    void allActionsIsTheWholeEnumInOrder()
    {
        const QList<HotkeyAction> all = allHotkeyActions();
        QCOMPARE(all.size(), kHotkeyActionCount);
        QCOMPARE(all.at(0), HotkeyAction::CaptureArea);
        QCOMPARE(all.at(1), HotkeyAction::CaptureWindow);
        QCOMPARE(all.at(2), HotkeyAction::CaptureFullScreen);
        QCOMPARE(all.at(3), HotkeyAction::OcrTextSnip);
        QCOMPARE(all.at(4), HotkeyAction::RecordArea);
        QCOMPARE(all.at(5), HotkeyAction::RecordWindow);
    }

    void idsAreFrozenAndRoundTrip()
    {
        // Frozen for compatibility: these are persisted, and on Linux the desktop
        // stores the user's binding under them.
        QCOMPARE(hotkeyActionId(HotkeyAction::CaptureArea), QStringLiteral("captureArea"));
        QCOMPARE(hotkeyActionId(HotkeyAction::CaptureWindow), QStringLiteral("captureWindow"));
        QCOMPARE(hotkeyActionId(HotkeyAction::CaptureFullScreen), QStringLiteral("captureFullScreen"));
        QCOMPARE(hotkeyActionId(HotkeyAction::OcrTextSnip), QStringLiteral("ocrTextSnip"));
        QCOMPARE(hotkeyActionId(HotkeyAction::RecordArea), QStringLiteral("recordArea"));
        QCOMPARE(hotkeyActionId(HotkeyAction::RecordWindow), QStringLiteral("recordWindow"));

        QSet<QString> seen;
        for (const HotkeyAction a : allHotkeyActions()) {
            const QString id = hotkeyActionId(a);
            QVERIFY(!id.isEmpty());
            QVERIFY(!seen.contains(id));
            seen.insert(id);
            const auto back = hotkeyActionFromId(id);
            QVERIFY(back.has_value());
            QCOMPARE(*back, a);
        }
    }

    void unknownIdIsNullopt()
    {
        QVERIFY(!hotkeyActionFromId(QString()).has_value());
        QVERIFY(!hotkeyActionFromId("").has_value());
        QVERIFY(!hotkeyActionFromId("nonsense").has_value());
        QVERIFY(!hotkeyActionFromId("CaptureArea").has_value());   // ids are camelCase
    }

    void descriptionsAreUniqueAndPresent()
    {
        QCOMPARE(hotkeyActionDescription(HotkeyAction::CaptureArea), QStringLiteral("Capture Area"));
        QCOMPARE(hotkeyActionDescription(HotkeyAction::OcrTextSnip), QStringLiteral("Extract Text (OCR)"));

        QSet<QString> seen;
        for (const HotkeyAction a : allHotkeyActions()) {
            const QString d = hotkeyActionDescription(a);
            QVERIFY(!d.isEmpty());
            QVERIFY(!seen.contains(d));   // they label rows in the settings list
            seen.insert(d);
        }
    }

    void defaultsMatchTheTrayShortcuts()
    {
        QCOMPARE(hotkeyActionDefault(HotkeyAction::CaptureArea), QKeySequence("Ctrl+Shift+A"));
        QCOMPARE(hotkeyActionDefault(HotkeyAction::CaptureWindow), QKeySequence("Ctrl+Shift+W"));
        QCOMPARE(hotkeyActionDefault(HotkeyAction::OcrTextSnip), QKeySequence("Ctrl+Shift+T"));
        QCOMPARE(hotkeyActionDefault(HotkeyAction::RecordArea), QKeySequence("Ctrl+Shift+R"));
        // Deliberately unbound, not an oversight: on macOS Qt maps Ctrl to Command and
        // every plausible Cmd+Shift+<letter> left for these two is already taken.
        QVERIFY(hotkeyActionDefault(HotkeyAction::CaptureFullScreen).isEmpty());
        QVERIFY(hotkeyActionDefault(HotkeyAction::RecordWindow).isEmpty());
    }
};

QTEST_MAIN(tst_HotkeyAction)
#include "tst_hotkeyaction.moc"
