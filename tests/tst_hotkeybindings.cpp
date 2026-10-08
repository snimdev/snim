#include <QtTest>
#include <QSettings>
#include <QStandardPaths>

#include "hotkeys/HotkeyAction.h"
#include "hotkeys/HotkeyBindings.h"

using namespace Hotkeys;

// The persisted layer: defaults for keys nobody has written, portable-text round
// trips, and the absent-vs-explicitly-empty distinction that lets a user unbind an
// action without it coming back on the next read.
class tst_HotkeyBindings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_hotkeybindings");
        QStandardPaths::setTestModeEnabled(true);   // throwaway store, never the real prefs
    }

    void init()
    {
        QSettings().clear();   // each test starts with every hotkey key absent
    }

    void absentKeysResolveToDefaults()
    {
        for (const HotkeyAction a : allHotkeyActions())
            QCOMPARE(HotkeyBindings::sequence(a), hotkeyActionDefault(a));

        QVERIFY(!HotkeyBindings::sequence(HotkeyAction::CaptureArea).isEmpty());
        QVERIFY(HotkeyBindings::sequence(HotkeyAction::RecordWindow).isEmpty());
    }

    void setThenReloadRoundTripsThroughPortableText()
    {
        HotkeyBindings::setSequence(HotkeyAction::CaptureFullScreen,
                                    QKeySequence("Ctrl+Alt+Shift+F9"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen),
                 QKeySequence("Ctrl+Alt+Shift+F9"));
        // What lands in QSettings is the portable spelling, not the native one.
        QCOMPARE(QSettings().value("Hotkeys/CaptureFullScreen").toString(),
                 QKeySequence("Ctrl+Alt+Shift+F9").toString(QKeySequence::PortableText));

        // Overwriting replaces, and the other actions are untouched.
        HotkeyBindings::setSequence(HotkeyAction::CaptureFullScreen, QKeySequence("Meta+F1"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen), QKeySequence("Meta+F1"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea),
                 hotkeyActionDefault(HotkeyAction::CaptureArea));
    }

    void keyNamesAreFrozen()
    {
        // Renaming a key would drop every user's bindings for that action.
        const QStringList keys = {"Hotkeys/CaptureArea", "Hotkeys/CaptureWindow",
                                  "Hotkeys/CaptureFullScreen", "Hotkeys/OcrTextSnip",
                                  "Hotkeys/RecordArea", "Hotkeys/RecordWindow"};
        for (int i = 0; i < keys.size(); ++i) {
            HotkeyBindings::setSequence(allHotkeyActions().at(i), QKeySequence("Ctrl+Alt+F1"));
            QCOMPARE(QSettings().value(keys.at(i)).toString(), QStringLiteral("Ctrl+Alt+F1"));
        }
    }

    void explicitClearStaysUnbound()
    {
        // The whole point of storing "" rather than removing the key: an action the
        // user unbound must NOT snap back to its default on the next read.
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, QKeySequence());
        QVERIFY(HotkeyBindings::sequence(HotkeyAction::CaptureArea).isEmpty());
        QCOMPARE(QSettings().value("Hotkeys/CaptureArea"), QVariant(QString()));
        QVERIFY(HotkeyBindings::isCustomized(HotkeyAction::CaptureArea));
        QVERIFY(!hotkeyActionDefault(HotkeyAction::CaptureArea).isEmpty());  // default intact

        for (const HotkeyBinding &b : HotkeyBindings::activeBindings())
            QVERIFY(b.action != HotkeyAction::CaptureArea);
    }

    void settingTheDefaultStoresNothing()
    {
        // Only overrides are stored, so a later change of default reaches every user
        // who never picked their own key.
        for (const HotkeyAction a : allHotkeyActions()) {
            HotkeyBindings::setSequence(a, hotkeyActionDefault(a));
            QVERIFY(!HotkeyBindings::isCustomized(a));
            QCOMPARE(HotkeyBindings::sequence(a), hotkeyActionDefault(a));
        }
        QVERIFY(QSettings().allKeys().isEmpty());
    }

    void overrideThenDefaultRemovesTheKey()
    {
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, QKeySequence("Ctrl+Alt+F7"));
        QVERIFY(HotkeyBindings::isCustomized(HotkeyAction::CaptureArea));
        QVERIFY(QSettings().contains("Hotkeys/CaptureArea"));

        HotkeyBindings::setSequence(HotkeyAction::CaptureArea,
                                    hotkeyActionDefault(HotkeyAction::CaptureArea));
        QVERIFY(!HotkeyBindings::isCustomized(HotkeyAction::CaptureArea));
        QVERIFY(!QSettings().contains("Hotkeys/CaptureArea"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea),
                 hotkeyActionDefault(HotkeyAction::CaptureArea));
    }

    void activeBindingsSkipsUnboundAndNormalizes()
    {
        // Out of the box: the three bound defaults, in enum order.
        QList<HotkeyAction> bound;
        for (const HotkeyAction a : allHotkeyActions()) {
            if (!hotkeyActionDefault(a).isEmpty())
                bound.append(a);
        }
        QCOMPARE(bound.size(), 3);
        QList<HotkeyBinding> active = HotkeyBindings::activeBindings();
        QCOMPARE(active.size(), bound.size());
        for (qsizetype i = 0; i < active.size(); ++i) {
            QCOMPARE(active.at(i).action, bound.at(i));
            QCOMPARE(active.at(i).sequence, hotkeyActionDefault(bound.at(i)));
        }

        // A multi-chord sequence is trimmed to its first chord on the way out, since
        // no backend can register the rest.
        HotkeyBindings::setSequence(HotkeyAction::RecordWindow, QKeySequence("Ctrl+K, Ctrl+J"));
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, QKeySequence());   // unbound
        active = HotkeyBindings::activeBindings();
        QCOMPARE(active.size(), bound.size());
        QVERIFY(active.first().action != HotkeyAction::CaptureArea);
        QCOMPARE(active.last().action, HotkeyAction::RecordWindow);
        QCOMPARE(active.last().sequence, QKeySequence("Ctrl+K"));
        for (const HotkeyBinding &b : active)
            QVERIFY(!b.sequence.isEmpty());
    }

    void normalizedKeepsOnlyTheFirstChord()
    {
        QCOMPARE(HotkeyBindings::normalized(QKeySequence("Ctrl+A, Ctrl+B")), QKeySequence("Ctrl+A"));
        QCOMPARE(HotkeyBindings::normalized(QKeySequence("Ctrl+Shift+A")), QKeySequence("Ctrl+Shift+A"));
        QVERIFY(HotkeyBindings::normalized(QKeySequence()).isEmpty());
    }
};

QTEST_MAIN(tst_HotkeyBindings)
#include "tst_hotkeybindings.moc"
