#include <QtTest>
#include <QSettings>
#include <QStandardPaths>

#include "core/Settings.h"
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
        QCoreApplication::setOrganizationName("NiceshotTest");
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
            QCOMPARE(HotkeyBindings::sequence(a), HotkeyBindings::defaultSequence(a));

        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea),
                 QKeySequence("Ctrl+Shift+A"));
        QVERIFY(HotkeyBindings::sequence(HotkeyAction::RecordWindow).isEmpty());
        QCOMPARE(HotkeyBindings::defaultSequence(HotkeyAction::OcrTextSnip),
                 hotkeyActionDefault(HotkeyAction::OcrTextSnip));
    }

    void setThenReloadRoundTripsThroughPortableText()
    {
        HotkeyBindings::setSequence(HotkeyAction::CaptureFullScreen,
                                    QKeySequence("Ctrl+Alt+Shift+F9"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen),
                 QKeySequence("Ctrl+Alt+Shift+F9"));
        // What lands in QSettings is the portable spelling, not the native one.
        QCOMPARE(Core::Settings::hotkeyCaptureFullScreen(),
                 QKeySequence("Ctrl+Alt+Shift+F9").toString(QKeySequence::PortableText));

        // Overwriting replaces, and the other actions are untouched.
        HotkeyBindings::setSequence(HotkeyAction::CaptureFullScreen, QKeySequence("Meta+F1"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureFullScreen), QKeySequence("Meta+F1"));
        QCOMPARE(HotkeyBindings::sequence(HotkeyAction::CaptureArea), QKeySequence("Ctrl+Shift+A"));
    }

    void explicitClearStaysUnbound()
    {
        // The whole point of storing "" rather than removing the key: an action the
        // user unbound must NOT snap back to its default on the next read.
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, QKeySequence());
        QVERIFY(HotkeyBindings::sequence(HotkeyAction::CaptureArea).isEmpty());
        QVERIFY(Core::Settings::hotkeyCaptureArea().isEmpty());
        QVERIFY(!HotkeyBindings::defaultSequence(HotkeyAction::CaptureArea).isEmpty());  // default intact

        for (const HotkeyBinding &b : HotkeyBindings::activeBindings())
            QVERIFY(b.action != HotkeyAction::CaptureArea);
    }

    void activeBindingsSkipsUnboundAndNormalizes()
    {
        // Out of the box: the four bound defaults, in enum order.
        QList<HotkeyBinding> active = HotkeyBindings::activeBindings();
        QCOMPARE(active.size(), 4);
        QCOMPARE(active.at(0).action, HotkeyAction::CaptureArea);
        QCOMPARE(active.at(1).action, HotkeyAction::CaptureWindow);
        QCOMPARE(active.at(2).action, HotkeyAction::OcrTextSnip);
        QCOMPARE(active.at(3).action, HotkeyAction::RecordArea);
        QCOMPARE(active.at(0).sequence, QKeySequence("Ctrl+Shift+A"));

        // A multi-chord sequence is trimmed to its first chord on the way out, since
        // no backend can register the rest.
        HotkeyBindings::setSequence(HotkeyAction::RecordWindow, QKeySequence("Ctrl+K, Ctrl+J"));
        HotkeyBindings::setSequence(HotkeyAction::CaptureWindow, QKeySequence());   // unbound
        active = HotkeyBindings::activeBindings();
        QCOMPARE(active.size(), 4);
        QCOMPARE(active.at(3).action, HotkeyAction::RecordWindow);
        QCOMPARE(active.at(3).sequence, QKeySequence("Ctrl+K"));
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
