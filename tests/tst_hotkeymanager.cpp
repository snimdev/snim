#include <QtTest>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <memory>

#include "hotkeys/GlobalHotkeyManager.h"
#include "hotkeys/HotkeyAction.h"
#include "hotkeys/HotkeyBackend.h"
#include "hotkeys/HotkeyBindings.h"

using namespace Hotkeys;

// A fake backend, so the manager is testable headlessly. Nothing here touches a real
// platform backend or registers a real OS hotkey - CI runs on a live macOS host, where
// a stray RegisterEventHotKey would fight the developer's own shortcuts.
class FakeHotkeyBackend : public HotkeyBackend
{
    Q_OBJECT

public:
    void registerAll(const QList<HotkeyBinding> &bindings) override
    {
        lastRegistered = bindings;
        ++registerCount;
        if (failEvery) {
            for (const HotkeyBinding &binding : bindings)
                emit registrationFailed(binding.action, QStringLiteral("no app id"));
            return;
        }
        if (failNext && !bindings.isEmpty()) {
            failNext = false;
            emit registrationFailed(bindings.first().action, QStringLiteral("already in use"));
        }
        for (const HotkeyBinding &binding : bindings) {
            if (failWith.contains(binding.action))
                emit registrationFailed(binding.action, failWith.value(binding.action));
        }
    }
    void unregisterAll() override { ++unregisterCount; }

    void simulateActivation(HotkeyAction action) { emit activated(action); }
    // The portal answers after registerAll returns.
    void simulateLateFailure(HotkeyAction action, const QString &reason)
    {
        emit registrationFailed(action, reason);
    }

    QList<HotkeyBinding> lastRegistered;
    int registerCount = 0;
    int unregisterCount = 0;
    bool failNext = false;
    bool failEvery = false;
    QMap<HotkeyAction, QString> failWith;
};

class tst_HotkeyManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_hotkeymanager");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QSettings().clear();   // every test decides its own bindings
    }

    void applyBindingsPassesTheResolvedActiveSet()
    {
        for (const HotkeyAction a : allHotkeyActions())
            HotkeyBindings::setSequence(a, QKeySequence());   // unbound
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, QKeySequence("Ctrl+Alt+1"));

        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        GlobalHotkeyManager mgr(std::move(fake));

        mgr.applyBindings();
        QCOMPARE(f->registerCount, 1);
        QCOMPARE(f->lastRegistered.size(), 1);   // the one bound action, defaults included
        QCOMPARE(f->lastRegistered.first().action, HotkeyAction::CaptureArea);
        QCOMPARE(f->lastRegistered.first().sequence, QKeySequence("Ctrl+Alt+1"));
    }

    void reapplyReplacesTheActiveSet()
    {
        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        GlobalHotkeyManager mgr(std::move(fake));

        mgr.applyBindings();
        QCOMPARE(f->unregisterCount, 1);
        QCOMPARE(f->registerCount, 1);

        HotkeyBindings::setSequence(HotkeyAction::RecordWindow, QKeySequence("Ctrl+Alt+9"));
        mgr.applyBindings();
        // The old set is dropped before the new one goes in, so nothing survives twice.
        QCOMPARE(f->unregisterCount, 2);
        QCOMPARE(f->registerCount, 2);
        qsizetype defaults = 0;
        for (const HotkeyAction a : allHotkeyActions())
            defaults += hotkeyActionDefault(a).isEmpty() ? 0 : 1;
        QCOMPARE(f->lastRegistered.size(), defaults + 1);   // the defaults + the new one
        QCOMPARE(f->lastRegistered.last().action, HotkeyAction::RecordWindow);
    }

    void activationRelaysToActionTriggered()
    {
        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        GlobalHotkeyManager mgr(std::move(fake));

        QSignalSpy spy(&mgr, &GlobalHotkeyManager::actionTriggered);
        f->simulateActivation(HotkeyAction::OcrTextSnip);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<HotkeyAction>(), HotkeyAction::OcrTextSnip);
    }

    void registrationFailureIsComposedForTheUser()
    {
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, QKeySequence("Ctrl+Shift+A"));

        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        f->failNext = true;
        GlobalHotkeyManager mgr(std::move(fake));

        QSignalSpy spy(&mgr, &GlobalHotkeyManager::registrationFailed);
        mgr.applyBindings();
        // Reported one turn later: a pass is judged as a whole before anything is shown.
        QTRY_COMPARE(spy.count(), 1);

        const QString msg = spy.at(0).at(0).toString();
        QVERIFY(msg.contains(hotkeyActionDescription(HotkeyAction::CaptureArea)));
        QVERIFY(msg.contains(QStringLiteral("already in use")));
        // Native spelling: the user is being told which keys failed, and on macOS that
        // is the Command symbol, not "Ctrl".
        QVERIFY(msg.contains(QKeySequence("Ctrl+Shift+A").toString(QKeySequence::NativeText)));
    }

    void aWhollyFailedPassIsOneConsolidatedMessage()
    {
        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        f->failEvery = true;
        GlobalHotkeyManager mgr(std::move(fake));

        QSignalSpy spy(&mgr, &GlobalHotkeyManager::registrationFailed);
        mgr.applyBindings();
        QVERIFY(f->lastRegistered.size() > 1);   // several bindings, one message

        QTRY_COMPARE(spy.count(), 1);
        const QString msg = spy.at(0).at(0).toString();
        QVERIFY(msg.contains(QStringLiteral("Global hotkeys are unavailable")));
        QVERIFY(msg.contains(QStringLiteral("application menu")));
        // The per-binding wording is gone: it said nothing the user could act on.
        QVERIFY(!msg.contains(QStringLiteral("no app id")));
    }

    void failureReasonIsKeptPerAction()
    {
        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        f->failWith.insert(HotkeyAction::CaptureArea, QStringLiteral("taken by Spectacle"));
        GlobalHotkeyManager mgr(std::move(fake));

        QSignalSpy spy(&mgr, &GlobalHotkeyManager::failuresChanged);
        mgr.applyBindings();
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(mgr.failureReason(HotkeyAction::CaptureArea), QStringLiteral("taken by Spectacle"));
        for (const HotkeyAction a : allHotkeyActions()) {
            if (a != HotkeyAction::CaptureArea)
                QVERIFY(mgr.failureReason(a).isEmpty());   // registered or unbound
        }
        QVERIFY(!mgr.allFailed());

        // A failure the portal reports later is kept and announced again.
        f->simulateLateFailure(HotkeyAction::RecordArea, QStringLiteral("no portal spelling"));
        QTRY_COMPARE(spy.count(), 2);
        QCOMPARE(mgr.failureReason(HotkeyAction::RecordArea), QStringLiteral("no portal spelling"));
    }

    void reapplyClearsTheFailures()
    {
        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        f->failWith.insert(HotkeyAction::CaptureArea, QStringLiteral("already in use"));
        GlobalHotkeyManager mgr(std::move(fake));

        QSignalSpy spy(&mgr, &GlobalHotkeyManager::failuresChanged);
        mgr.applyBindings();
        QTRY_COMPARE(spy.count(), 1);
        QVERIFY(!mgr.failureReason(HotkeyAction::CaptureArea).isEmpty());

        f->failWith.clear();
        mgr.applyBindings();
        QVERIFY(mgr.failureReason(HotkeyAction::CaptureArea).isEmpty());
        // A clean pass is announced too, so a stale notice can be taken down.
        QTRY_COMPARE(spy.count(), 2);
    }

    void oneFailuresChangedPerPass()
    {
        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        f->failEvery = true;
        GlobalHotkeyManager mgr(std::move(fake));

        QSignalSpy spy(&mgr, &GlobalHotkeyManager::failuresChanged);
        mgr.applyBindings();
        QVERIFY(f->lastRegistered.size() > 1);   // several failures, one notification
        QTRY_COMPARE(spy.count(), 1);
        QTest::qWait(20);
        QCOMPARE(spy.count(), 1);
    }

    void allFailedOnlyWhenNothingRegistered()
    {
        auto fake = std::make_unique<FakeHotkeyBackend>();
        FakeHotkeyBackend *f = fake.get();
        f->failEvery = true;
        GlobalHotkeyManager mgr(std::move(fake));

        QSignalSpy spy(&mgr, &GlobalHotkeyManager::failuresChanged);
        mgr.applyBindings();
        QTRY_COMPARE(spy.count(), 1);
        QVERIFY(mgr.allFailed());
        for (const HotkeyBinding &b : f->lastRegistered)
            QCOMPARE(mgr.failureReason(b.action), QStringLiteral("no app id"));

        f->failEvery = false;
        f->failNext = true;   // one of several fails
        mgr.applyBindings();
        QTRY_COMPARE(spy.count(), 2);
        QVERIFY(!mgr.allFailed());

        // Nothing bound means nothing failed.
        for (const HotkeyAction a : allHotkeyActions())
            HotkeyBindings::setSequence(a, QKeySequence());
        mgr.applyBindings();
        QTRY_COMPARE(spy.count(), 3);
        QVERIFY(!mgr.allFailed());
    }
};

QTEST_MAIN(tst_HotkeyManager)
#include "tst_hotkeymanager.moc"
