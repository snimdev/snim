#include <QtTest>
#include <QSet>

#include "hotkeys/HotkeyAction.h"
#include "hotkeys/MacKeyMapping.h"
#include "hotkeys/PortalKeyMapping.h"
#include "hotkeys/WinKeyMapping.h"

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

        // Empty is deliberately unbound, not an oversight: the free chords left for
        // those actions collide with common app shortcuts.
        const auto row = [](HotkeyPlatform p, const char *name, HotkeyAction a, const char *keys) {
            QTest::addRow("%s/%s", name, qPrintable(hotkeyActionId(a)))
                << p << a << QKeySequence(QString::fromLatin1(keys));
        };
        // Windows and Linux: Print chords, since the OS keeps bare Print for itself.
        for (const auto &[p, name] : {std::pair{HotkeyPlatform::Windows, "windows"},
                                      std::pair{HotkeyPlatform::Linux, "linux"}}) {
            row(p, name, HotkeyAction::CaptureArea, "Ctrl+Print");
            row(p, name, HotkeyAction::CaptureWindow, "Ctrl+Alt+Print");
            row(p, name, HotkeyAction::CaptureFullScreen, "");
            row(p, name, HotkeyAction::OcrTextSnip, "");
            row(p, name, HotkeyAction::RecordArea, "Ctrl+Shift+Print");
            row(p, name, HotkeyAction::RecordWindow, "");
        }
        // macOS: Qt's Ctrl is Command, so Option is added to the system's 3/4/5 keys.
        const HotkeyPlatform mac = HotkeyPlatform::Mac;
        row(mac, "mac", HotkeyAction::CaptureArea, "Ctrl+Alt+Shift+4");
        row(mac, "mac", HotkeyAction::CaptureWindow, "");
        row(mac, "mac", HotkeyAction::CaptureFullScreen, "Ctrl+Alt+Shift+3");
        row(mac, "mac", HotkeyAction::OcrTextSnip, "");
        row(mac, "mac", HotkeyAction::RecordArea, "Ctrl+Alt+Shift+5");
        row(mac, "mac", HotkeyAction::RecordWindow, "");
    }

    void defaultsPerPlatform()
    {
        QFETCH(HotkeyPlatform, platform);
        QFETCH(HotkeyAction, action);
        QFETCH(QKeySequence, expected);
        QCOMPARE(hotkeyActionDefault(action, platform), expected);
    }

    void everyDefaultMapsOnItsPlatform()
    {
        // A default its own backend cannot register is a hotkey that silently never fires.
        for (const HotkeyAction a : allHotkeyActions()) {
            const QKeySequence winSeq = hotkeyActionDefault(a, HotkeyPlatform::Windows);
            const QKeySequence macSeq = hotkeyActionDefault(a, HotkeyPlatform::Mac);
            const QKeySequence linuxSeq = hotkeyActionDefault(a, HotkeyPlatform::Linux);
            if (!winSeq.isEmpty())
                QVERIFY2(toWinHotkey(winSeq).has_value(), qPrintable(winSeq.toString()));
            if (!macSeq.isEmpty())
                QVERIFY2(toCarbonHotkey(macSeq).has_value(), qPrintable(macSeq.toString()));
            if (!linuxSeq.isEmpty())
                QVERIFY2(!toPortalTrigger(linuxSeq).isEmpty(), qPrintable(linuxSeq.toString()));
        }
    }

    void noDuplicateDefaultsWithinAPlatform()
    {
        for (const HotkeyPlatform p :
             {HotkeyPlatform::Windows, HotkeyPlatform::Mac, HotkeyPlatform::Linux}) {
            QSet<QString> seen;
            for (const HotkeyAction a : allHotkeyActions()) {
                const QKeySequence seq = hotkeyActionDefault(a, p);
                if (seq.isEmpty())
                    continue;
                const QString text = seq.toString(QKeySequence::PortableText);
                QVERIFY2(!seen.contains(text), qPrintable(text));
                seen.insert(text);
            }
        }
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
