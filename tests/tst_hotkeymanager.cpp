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
    }
    void unregisterAll() override { ++unregisterCount; }

    void simulateActivation(HotkeyAction action) { emit activated(action); }

    QList<HotkeyBinding> lastRegistered;
    int registerCount = 0;
    int unregisterCount = 0;
    bool failNext = false;
    bool failEvery = false;
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
        HotkeyBindings::setSequence(HotkeyAction::CaptureArea, QKeySequence("Ctrl+Alt+1"));
        HotkeyBindings::setSequence(HotkeyAction::CaptureWindow, QKeySequence());   // unbound
        HotkeyBindings::setSequence(HotkeyAction::OcrTextSnip, QKeySequence());
        HotkeyBindings::setSequence(HotkeyAction::RecordArea, QKeySequence());

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
        QCOMPARE(f->lastRegistered.size(), 5);           // 4 defaults + the new one
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
};

QTEST_MAIN(tst_HotkeyManager)
#include "tst_hotkeymanager.moc"
