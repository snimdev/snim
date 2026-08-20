#include <QtTest>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QMutex>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QThread>

#include "core/Portal.h"
#include "hotkeys/backends/PortalHotkeyBackend.h"

using namespace Hotkeys;

namespace {

const QString kShortcuts = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");
const QString kFakeConnection = QStringLiteral("snim-fake-portal");
const QString kSession = QStringLiteral("/org/freedesktop/portal/desktop/session/fake/s1");

} // namespace

// A scripted GlobalShortcuts portal on its own connection and thread, so the client's
// blocking probes are answered while the test thread waits in them.
class FakePortal : public QDBusVirtualObject
{
    Q_OBJECT

public:
    int createAnswer = 0;   // the Response code, or -1 to fail the call itself

    QStringList calls() const { QMutexLocker lock(&m_mutex); return m_calls; }

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        QMutexLocker lock(&m_mutex);
        const QVariantList args = message.arguments();
        const QString member = message.member();
        const QVariantMap options = qdbus_cast<QVariantMap>(args.value(args.size() - 1));
        // The client derived this path itself; a mismatch would leave it waiting forever.
        const QString request = Core::Portal::kPath + QStringLiteral("/request/")
                                + message.service().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'))
                                + QLatin1Char('/') + options.value(QStringLiteral("handle_token")).toString();
        auto respond = [&](uint code, const QVariantMap &results) {
            connection.send(message.createReply({QVariant::fromValue(QDBusObjectPath(request))}));
            QDBusMessage response = QDBusMessage::createSignal(
                request, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"));
            response.setArguments({code, results});
            connection.send(response);
        };

        if (member == QLatin1String("Get") && args.value(0).toString() == kShortcuts) {
            connection.send(message.createReply({QVariant::fromValue(QDBusVariant(1u))}));
        } else if (member == QLatin1String("CreateSession")) {
            m_calls << member;
            if (createAnswer < 0)
                connection.send(message.createErrorReply(QStringLiteral("org.example.Broken"),
                                                         QStringLiteral("portal broke")));
            else
                respond(uint(createAnswer), {{QStringLiteral("session_handle"),
                                              QVariant::fromValue(QDBusObjectPath(kSession))}});
        } else if (member == QLatin1String("BindShortcuts")) {
            const QDBusArgument shortcuts = args.value(1).value<QDBusArgument>();
            shortcuts.beginArray();
            while (!shortcuts.atEnd()) {
                QString id;
                QVariantMap metadata;
                shortcuts.beginStructure();
                shortcuts >> id >> metadata;
                shortcuts.endStructure();
                m_calls << id + QLatin1Char('=') + metadata.value(QStringLiteral("preferred_trigger")).toString();
            }
            shortcuts.endArray();
            respond(0, {});
        } else if (member == QLatin1String("Close")) {
            m_calls << member + QLatin1Char(' ') + message.path();
            connection.send(message.createReply());
        } else {
            connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                                                     member));
        }
        return true;
    }

private:
    mutable QMutex m_mutex;
    QStringList m_calls;
};

// The shared portal plumbing on a private session bus (ctest wraps this in
// dbus-run-session with nothing activatable), driven through the GlobalShortcuts backend.
class tst_Portal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qEnvironmentVariable("SNIM_TEST_PRIVATE_BUS") != QLatin1String("1"))
            QSKIP("needs the private session bus ctest starts (SNIM_TEST_PRIVATE_BUS=1)");
        QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
        if (!bus || bus->isServiceRegistered(Core::Portal::kService))
            QSKIP("no private session bus, or it already has a portal; refusing to touch it");
    }

    void cleanup()
    {
        if (!m_fake)
            return;
        QMetaObject::invokeMethod(m_fake, [] {
            QDBusConnection fake(kFakeConnection);
            fake.unregisterService(Core::Portal::kService);
            fake.unregisterObject(Core::Portal::kPath, QDBusConnection::UnregisterTree);
            QDBusConnection::disconnectFromBus(kFakeConnection);
        }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
        delete m_fake;
        delete m_thread;
        m_fake = nullptr;
        m_thread = nullptr;
        QTRY_VERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(Core::Portal::kService));
    }

    void pathsAndHandles()
    {
        QCOMPARE(Core::Portal::Request::pathFor(QStringLiteral(":1.42"), QStringLiteral("t0k")),
                 QStringLiteral("/org/freedesktop/portal/desktop/request/1_42/t0k"));
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{32}$")).match(Core::Portal::newToken()).hasMatch());
        const QString path = QStringLiteral("/org/freedesktop/portal/desktop/session/1_42/s");
        QCOMPARE(Core::Portal::sessionHandle({{QStringLiteral("session_handle"), path}}), path);
        QCOMPARE(Core::Portal::sessionHandle({{QStringLiteral("session_handle"),
                                               QVariant::fromValue(QDBusObjectPath(path))}}), path);
        QCOMPARE(Core::Portal::sessionHandle({}), QString());
    }

    // A no is asked again (the portal may still be starting); a yes holds for the run.
    void aYesIsKeptForTheRun()
    {
        QVERIFY(!Core::Portal::hasInterface(kShortcuts));
        startFake();
        QVERIFY(Core::Portal::hasInterface(kShortcuts));
        QCOMPARE(Core::Portal::property(kShortcuts, QStringLiteral("version")).toUInt(), 1u);
        cleanup();
        QVERIFY(Core::Portal::hasInterface(kShortcuts));
        QVERIFY(!Core::Portal::property(kShortcuts, QStringLiteral("version")).isValid());
    }

    void bindsThroughTheDesktop_data()
    {
        QTest::addColumn<int>("createAnswer");
        QTest::addColumn<QString>("failure");
        QTest::newRow("bound") << 0 << QString();
        QTest::newRow("refused") << 2 << QStringLiteral("the desktop refused a shortcuts session");
        QTest::newRow("call failed") << -1 << QStringLiteral("portal broke");
    }

    void bindsThroughTheDesktop()
    {
        QFETCH(int, createAnswer);
        QFETCH(QString, failure);
        startFake()->createAnswer = createAnswer;
        const QString id = hotkeyActionId(HotkeyAction::CaptureArea);

        PortalHotkeyBackend backend;
        QSignalSpy activated(&backend, &HotkeyBackend::activated);
        QSignalSpy failed(&backend, &HotkeyBackend::registrationFailed);
        backend.registerAll({{HotkeyAction::CaptureArea, QKeySequence(QStringLiteral("Ctrl+Shift+A"))}});
        if (!failure.isEmpty()) {
            QTRY_COMPARE(failed.count(), 1);
            QCOMPARE(failed.first().at(1).toString(), failure);
            return;
        }
        QTRY_COMPARE(m_fake->calls(), (QStringList{QStringLiteral("CreateSession"), id + "=CTRL+SHIFT+a"}));

        QDBusMessage pressed = QDBusMessage::createSignal(Core::Portal::kPath, kShortcuts,
                                                          QStringLiteral("Activated"));
        pressed.setArguments({QVariant::fromValue(QDBusObjectPath(kSession)), id, qulonglong(0),
                              QVariantMap()});
        QDBusConnection(kFakeConnection).send(pressed);
        QTRY_COMPARE(activated.count(), 1);
        QCOMPARE(activated.first().first().value<HotkeyAction>(), HotkeyAction::CaptureArea);

        backend.unregisterAll();
        QTRY_VERIFY(m_fake->calls().contains(QStringLiteral("Close ") + kSession));
        QCOMPARE(failed.count(), 0);
    }

private:
    FakePortal *startFake()
    {
        m_thread = new QThread;
        m_fake = new FakePortal;
        m_fake->moveToThread(m_thread);
        m_thread->start();
        bool ok = false;
        QMetaObject::invokeMethod(m_fake, [this, &ok] {
            QDBusConnection fake = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                                 kFakeConnection);
            ok = fake.registerVirtualObject(Core::Portal::kPath, m_fake, QDBusConnection::SubPath)
                 && fake.registerService(Core::Portal::kService);
        }, Qt::BlockingQueuedConnection);
        if (!ok)
            qFatal("could not start the fake portal");
        return m_fake;
    }

    QThread *m_thread = nullptr;
    FakePortal *m_fake = nullptr;
};

QTEST_MAIN(tst_Portal)
#include "tst_portal.moc"
