#include <QtTest>

#include "core/AppScope.h"

using namespace Core;

// The cgroup line parser behind the portal app-id fix. Pure string work, so it runs
// everywhere: inAppScope() reads /proc and adoptAppScope() talks to systemd, and neither
// belongs in a test run (CI has no session bus, and moving the runner's process is rude).
class tst_AppScope : public QObject
{
    Q_OBJECT

private slots:
    void acceptsAppScopedUnits();
    void rejectsUnitsWithoutAnAppId();
    void handlesCgroupV1Lines();
};

void tst_AppScope::acceptsAppScopedUnits()
{
    QVERIFY(AppScope::unitLineIndicatesAppScope(
        u"0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-dev.snim.Snim-1234.scope"));
    // A launcher-started app lands in a .service unit instead, and still has an app id.
    QVERIFY(AppScope::unitLineIndicatesAppScope(
        u"0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-org.kde.dolphin@12ab.service"));
}

void tst_AppScope::rejectsUnitsWithoutAnAppId()
{
    // Terminal children: app-shaped ancestry, but the leaf names no application.
    QVERIFY(!AppScope::unitLineIndicatesAppScope(
        u"0::/user.slice/user-1000.slice/user@1000.service/app.slice/vte-spawn-abc.scope"));
    QVERIFY(!AppScope::unitLineIndicatesAppScope(
        u"0::/user.slice/user-1000.slice/user@1000.service/app.slice/app-vte-spawn-abc.scope"));
    QVERIFY(!AppScope::unitLineIndicatesAppScope(
        u"0::/user.slice/user-1000.slice/session-2.scope"));
    QVERIFY(!AppScope::unitLineIndicatesAppScope(
        u"0::/user.slice/user-1000.slice/user@1000.service"));
    QVERIFY(!AppScope::unitLineIndicatesAppScope(u""));
}

void tst_AppScope::handlesCgroupV1Lines()
{
    // v1 lines carry the hierarchy id and controllers first; the unit is still the leaf.
    QVERIFY(AppScope::unitLineIndicatesAppScope(
        u"7:memory:/user.slice/user-1000.slice/user@1000.service/app.slice/app-x-1.scope"));
}

QTEST_MAIN(tst_AppScope)
#include "tst_appscope.moc"
