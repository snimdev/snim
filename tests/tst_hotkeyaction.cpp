#include <QtTest>
#include <QSet>

#include "hotkeys/HotkeyAction.h"

using namespace Hotkeys;

// The hotkey action vocabulary: the persisted ids, the labels, and the factory
// defaults. Pure lookups, no settings involved.
class tst_HotkeyAction : public QObject
{
    Q_OBJECT

private slots:
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

    void defaultsPerPlatform_data()
    {
        QTest::addColumn<HotkeyPlatform>("platform");
        QTest::addColumn<HotkeyAction>("action");
        QTest::addColumn<QKeySequence>("expected");

        // Empty is deliberately unbound, not an oversight: every plausible free chord
        // left for those actions is already taken by something common.
        const QList<QPair<HotkeyPlatform, const char *>> platforms = {
            {HotkeyPlatform::Windows, "windows"},
            {HotkeyPlatform::Mac, "mac"},
            {HotkeyPlatform::Linux, "linux"}};
        for (const auto &[platform, name] : platforms) {
            const auto row = [&](HotkeyAction a, const char *keys) {
                QTest::addRow("%s/%s", name, qPrintable(hotkeyActionId(a)))
                    << platform << a << QKeySequence(QString::fromLatin1(keys));
            };
            row(HotkeyAction::CaptureArea, "Ctrl+Shift+A");
            row(HotkeyAction::CaptureWindow, "Ctrl+Shift+W");
            row(HotkeyAction::CaptureFullScreen, "");
            row(HotkeyAction::OcrTextSnip, "Ctrl+Shift+T");
            row(HotkeyAction::RecordArea, "Ctrl+Shift+R");
            row(HotkeyAction::RecordWindow, "");
        }
    }

    void defaultsPerPlatform()
    {
        QFETCH(HotkeyPlatform, platform);
        QFETCH(HotkeyAction, action);
        QFETCH(QKeySequence, expected);
        QCOMPARE(hotkeyActionDefault(action, platform), expected);
    }

    void hostOverloadUsesTheHostPlatform()
    {
#if defined(Q_OS_MACOS)
        static_assert(hostHotkeyPlatform() == HotkeyPlatform::Mac);
#elif defined(Q_OS_WIN)
        static_assert(hostHotkeyPlatform() == HotkeyPlatform::Windows);
#else
        static_assert(hostHotkeyPlatform() == HotkeyPlatform::Linux);
#endif
        for (const HotkeyAction a : allHotkeyActions())
            QCOMPARE(hotkeyActionDefault(a), hotkeyActionDefault(a, hostHotkeyPlatform()));
    }
};

QTEST_MAIN(tst_HotkeyAction)
#include "tst_hotkeyaction.moc"
