#include <QtTest>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QMutex>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include "core/KeychainStore.h"
#include "core/SecretService.h"

#include <functional>

using namespace Core;
using SecretService::Attributes;
using SecretService::Secret;
using Failure = KeychainStore::Failure;

namespace {

const QString kService = QStringLiteral("org.freedesktop.secrets");
const QString kRoot = QStringLiteral("/org/freedesktop/secrets");
const QString kCollection = QStringLiteral("/org/freedesktop/secrets/collection/fake");

QList<QDBusObjectPath> paths(const QStringList &list)
{
    QList<QDBusObjectPath> out;
    for (const QString &path : list)
        out.append(QDBusObjectPath(path));
    return out;
}

} // namespace

// A scripted org.freedesktop.secrets: its own connection and thread, so the client's
// blocking calls are answered while the test thread waits in them.
class FakeSecretService : public QDBusVirtualObject
{
    Q_OBJECT

public:
    enum class PromptAnswer { Complete, Dismiss, Never };

    struct Item {
        QString label;
        Attributes attributes;
        QByteArray value;
        QString contentType;
    };

    // Knobs, set before the client runs.
    bool hasDefault = true;
    bool locked = false;
    bool unlockNeedsPrompt = true;
    PromptAnswer promptAnswer = PromptAnswer::Complete;

    QStringList calls() const { QMutexLocker lock(&m_mutex); return m_calls; }
    QMap<QString, Item> items() const { QMutexLocker lock(&m_mutex); return m_items; }
    QString alias() const { QMutexLocker lock(&m_mutex); return m_alias; }
    QString collectionLabel() const { QMutexLocker lock(&m_mutex); return m_collectionLabel; }
    void addItem(const QString &value, const Attributes &attributes)
    {
        QMutexLocker lock(&m_mutex);
        m_items.insert(kCollection + QStringLiteral("/%1").arg(++m_next),
                       {QStringLiteral("seeded"), attributes, value.toUtf8(), QStringLiteral("text/plain")});
    }

    QString introspect(const QString &) const override { return {}; }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        QMutexLocker lock(&m_mutex);
        const QString member = message.member();
        const QString path = message.path();
        const QVariantList args = message.arguments();
        m_calls.append(member);
        const auto reply = [&](const QVariantList &out) { connection.send(message.createReply(out)); };

        if (member == QLatin1String("OpenSession")) {
            reply({QVariant::fromValue(QDBusVariant(QString())),
                   QVariant::fromValue(QDBusObjectPath(kRoot + QStringLiteral("/session/%1").arg(++m_next)))});
        } else if (member == QLatin1String("Close")) {
            reply({});
        } else if (member == QLatin1String("ReadAlias")) {
            reply({QVariant::fromValue(QDBusObjectPath(m_alias.isEmpty() && !hasDefault ? QStringLiteral("/")
                                                                                         : kCollection))});
        } else if (member == QLatin1String("CreateCollection")) {
            const QVariantMap properties = qdbus_cast<QVariantMap>(args.value(0));
            const QString alias = args.value(1).toString();
            const QString label = properties.value(QStringLiteral("org.freedesktop.Secret.Collection.Label")).toString();
            reply({QVariant::fromValue(QDBusObjectPath(QStringLiteral("/"))),
                   QVariant::fromValue(newPrompt([this, alias, label] {
                       m_alias = alias;
                       m_collectionLabel = label;
                       hasDefault = true;
                       return QVariant::fromValue(QDBusObjectPath(kCollection));
                   }))});
        } else if (member == QLatin1String("Get")) {
            reply({QVariant::fromValue(QDBusVariant(locked))});
        } else if (member == QLatin1String("Unlock")) {
            const QList<QDBusObjectPath> objects = qdbus_cast<QList<QDBusObjectPath>>(args.value(0));
            if (!locked || !unlockNeedsPrompt) {
                locked = false;
                reply({QVariant::fromValue(objects), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))});
            } else {
                reply({QVariant::fromValue(QList<QDBusObjectPath>()),
                       QVariant::fromValue(newPrompt([this, objects] {
                           locked = false;
                           return QVariant::fromValue(objects);
                       }))});
            }
        } else if (member == QLatin1String("SearchItems")) {
            const Attributes wanted = qdbus_cast<Attributes>(args.value(0));
            QStringList found;
            for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) {
                bool match = true;
                for (auto w = wanted.cbegin(); w != wanted.cend(); ++w)
                    match = match && it->attributes.value(w.key()) == w.value();
                if (match)
                    found.append(it.key());
            }
            reply({QVariant::fromValue(paths(locked ? QStringList() : found)),
                   QVariant::fromValue(paths(locked ? found : QStringList()))});
        } else if (member == QLatin1String("CreateItem")) {
            if (locked) {
                connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.Secret.Error.IsLocked"),
                                                         QStringLiteral("locked")));
                return true;
            }
            const QVariantMap properties = qdbus_cast<QVariantMap>(args.value(0));
            const Secret secret = qdbus_cast<Secret>(args.value(1));
            Item item{properties.value(QStringLiteral("org.freedesktop.Secret.Item.Label")).toString(),
                      qdbus_cast<Attributes>(properties.value(QStringLiteral("org.freedesktop.Secret.Item.Attributes"))),
                      secret.value, secret.contentType};
            m_replaceFlags.append(args.value(2).toBool());
            QString target;
            for (auto it = m_items.cbegin(); it != m_items.cend(); ++it) {
                if (args.value(2).toBool() && it->attributes == item.attributes)
                    target = it.key();
            }
            if (target.isEmpty())
                target = kCollection + QStringLiteral("/%1").arg(++m_next);
            m_items.insert(target, item);
            reply({QVariant::fromValue(QDBusObjectPath(target)),
                   QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))});
        } else if (member == QLatin1String("GetSecret")) {
            const Item item = m_items.value(path);
            reply({QVariant::fromValue(Secret{qdbus_cast<QDBusObjectPath>(args.value(0)), QByteArray(),
                                              item.value, item.contentType})});
        } else if (member == QLatin1String("Delete")) {
            m_items.remove(path);
            reply({QVariant::fromValue(QDBusObjectPath(QStringLiteral("/")))});
        } else if (member == QLatin1String("Prompt")) {
            m_promptWindowIds.append(args.value(0).toString());
            reply({});
            const auto action = m_prompts.take(path);
            if (promptAnswer == PromptAnswer::Never || !action)
                return true;
            const bool dismissed = promptAnswer == PromptAnswer::Dismiss;
            QDBusMessage completed = QDBusMessage::createSignal(path, QStringLiteral("org.freedesktop.Secret.Prompt"),
                                                                QStringLiteral("Completed"));
            completed.setArguments({dismissed, QVariant::fromValue(QDBusVariant(dismissed ? QVariant(QString())
                                                                                         : action()))});
            connection.send(completed);
        } else {
            connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                                                     member));
        }
        return true;
    }

    QList<bool> replaceFlags() const { QMutexLocker lock(&m_mutex); return m_replaceFlags; }
    QStringList promptWindowIds() const { QMutexLocker lock(&m_mutex); return m_promptWindowIds; }

private:
    QDBusObjectPath newPrompt(std::function<QVariant()> action)
    {
        const QString path = kRoot + QStringLiteral("/prompt/p%1").arg(++m_next);
        m_prompts.insert(path, std::move(action));
        return QDBusObjectPath(path);
    }

    mutable QMutex m_mutex;
    QStringList m_calls;
    QMap<QString, Item> m_items;
    QHash<QString, std::function<QVariant()>> m_prompts;
    QList<bool> m_replaceFlags;
    QStringList m_promptWindowIds;
    QString m_alias;
    QString m_collectionLabel;
    int m_next = 0;
};

// Core::KeychainStore's Linux backing on a private session bus (ctest wraps this in
// dbus-run-session with nothing activatable): a scripted fake service drives every
// protocol path, and a real gnome-keyring-daemon the round trip when one is installed.
class tst_KeychainStoreLinux : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void failsFastWithNoService();
    void roundTripsASecret();
    void createsTheDefaultCollectionOnAFreshKeyring();
    void unlocksThroughAPrompt();
    void dismissedPromptMeansLocked();
    void unansweredPromptGivesUp();
    void erasesEveryCopy();
    void probeTouchesNoItem();
    void rejectsEmptyIds();

    void roundTripsThroughGnomeKeyring();
    void lockedGnomeKeyringWithoutPrompterFails();

private:
    FakeSecretService *startFake();
    bool startGnomeKeyring(const QString &daemon);
    QString secretOf(const QString &account, Failure *why = nullptr) const
    {
        return KeychainStore::retrieve(m_service, account, why).value_or(QStringLiteral("<none>"));
    }

    const QString m_service = QStringLiteral("dev.snim.test.keychainstore");
    const QString m_account = QStringLiteral("account-1");
    QThread *m_fakeThread = nullptr;
    FakeSecretService *m_fake = nullptr;
    QProcess *m_daemon = nullptr;
    std::unique_ptr<QTemporaryDir> m_home;
};

void tst_KeychainStoreLinux::initTestCase()
{
    // Never on a desktop's own bus: its keyring must stay untouched.
    if (qEnvironmentVariable("SNIM_TEST_PRIVATE_BUS") != QLatin1String("1"))
        QSKIP("needs the private session bus ctest starts (SNIM_TEST_PRIVATE_BUS=1)");
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus)
        QSKIP("no session bus");
    if (bus->isServiceRegistered(kService)
        || bus->activatableServiceNames().value().contains(kService))
        QSKIP("this bus already has a Secret Service; refusing to touch it");
    SecretService::registerMetaTypes();
}

void tst_KeychainStoreLinux::cleanup()
{
    if (m_fake) {
        QMetaObject::invokeMethod(m_fake, [] {
            QDBusConnection fake(QStringLiteral("snim-fake-secrets"));
            fake.unregisterService(kService);
            fake.unregisterObject(kRoot, QDBusConnection::UnregisterTree);
            QDBusConnection::disconnectFromBus(QStringLiteral("snim-fake-secrets"));
        }, Qt::BlockingQueuedConnection);
        m_fakeThread->quit();
        m_fakeThread->wait();
        delete m_fake;
        delete m_fakeThread;
        m_fake = nullptr;
        m_fakeThread = nullptr;
    }
    if (m_daemon) {
        m_daemon->terminate();
        m_daemon->waitForFinished(5000);
        delete m_daemon;
        m_daemon = nullptr;
        m_home.reset();
    }
    // The name must be gone before the next test's client asks for it.
    QTRY_VERIFY(!QDBusConnection::sessionBus().interface()->isServiceRegistered(kService));
}

FakeSecretService *tst_KeychainStoreLinux::startFake()
{
    m_fakeThread = new QThread;
    m_fake = new FakeSecretService;
    m_fake->moveToThread(m_fakeThread);
    m_fakeThread->start();
    bool ok = false;
    QMetaObject::invokeMethod(m_fake, [this, &ok] {
        QDBusConnection fake = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                             QStringLiteral("snim-fake-secrets"));
        ok = fake.registerVirtualObject(kRoot, m_fake, QDBusConnection::SubPath)
             && fake.registerService(kService);
    }, Qt::BlockingQueuedConnection);
    if (!ok)
        qFatal("could not start the fake Secret Service");
    return m_fake;
}

bool tst_KeychainStoreLinux::startGnomeKeyring(const QString &daemon)
{
    // A throwaway home, so the daemon makes a fresh login keyring there.
    m_home = std::make_unique<QTemporaryDir>();
    if (!m_home->isValid())
        return false;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *var : {"HOME", "XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_RUNTIME_DIR"}) {
        const QString dir = m_home->path() + QLatin1Char('/') + QString::fromLatin1(var);
        if (!QDir().mkpath(dir))
            return false;
        QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        env.insert(QString::fromLatin1(var), dir);
    }
    m_daemon = new QProcess;
    m_daemon->setProcessEnvironment(env);
    m_daemon->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_daemon->start(daemon, {QStringLiteral("--foreground"), QStringLiteral("--unlock"),
                             QStringLiteral("--components=secrets")});
    if (!m_daemon->waitForStarted())
        return false;
    m_daemon->write("test-password");
    m_daemon->closeWriteChannel();
    QDeadlineTimer deadline(10000);
    while (!QDBusConnection::sessionBus().interface()->isServiceRegistered(kService)) {
        if (deadline.hasExpired())
            return false;
        QTest::qWait(50);
    }
    return true;
}

void tst_KeychainStoreLinux::failsFastWithNoService()
{
    QElapsedTimer timer;
    timer.start();
    Failure why = Failure::None;
    QVERIFY(!KeychainStore::store(m_service, m_account, QStringLiteral("x"), &why));
    QCOMPARE(why, Failure::NoService);
    why = Failure::None;
    QVERIFY(!KeychainStore::retrieve(m_service, m_account, &why).has_value());
    QCOMPARE(why, Failure::NoService);
    QVERIFY(!KeychainStore::erase(m_service, m_account));
    QString detail;
    QCOMPARE(SecretService::probe(&detail), Failure::NoService);
    QVERIFY2(timer.elapsed() < 2000, qPrintable(QStringLiteral("took %1 ms").arg(timer.elapsed())));
}

void tst_KeychainStoreLinux::roundTripsASecret()
{
    FakeSecretService *fake = startFake();
    Failure why = Failure::Other;
    QCOMPARE(secretOf(m_account, &why), QStringLiteral("<none>"));
    QCOMPARE(why, Failure::None);

    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("hunter2"), &why));
    QCOMPARE(why, Failure::None);
    QCOMPARE(secretOf(m_account), QStringLiteral("hunter2"));

    const QString replaced = QStringLiteral("päss wörd ✓");
    QVERIFY(KeychainStore::store(m_service, m_account, replaced));
    QCOMPARE(secretOf(m_account), replaced);
    QCOMPARE(fake->items().size(), 1);

    const FakeSecretService::Item item = fake->items().first();
    QCOMPARE(item.attributes, SecretService::attributesFor(m_service, m_account));
    QCOMPARE(item.label, SecretService::labelFor(m_service, m_account));
    QCOMPARE(item.contentType, QStringLiteral("text/plain; charset=utf8"));
    QCOMPARE(item.value, replaced.toUtf8());
    QCOMPARE(fake->replaceFlags(), (QList<bool>{true, true}));

    QVERIFY(KeychainStore::erase(m_service, m_account));
    QCOMPARE(secretOf(m_account, &why), QStringLiteral("<none>"));
    QCOMPARE(why, Failure::None);
    QVERIFY(KeychainStore::erase(m_service, m_account));
    // Every session the client opened was closed again (Close is fire and forget).
    QTRY_COMPARE(fake->calls().count(QStringLiteral("Close")),
                 fake->calls().count(QStringLiteral("OpenSession")));
}

void tst_KeychainStoreLinux::createsTheDefaultCollectionOnAFreshKeyring()
{
    FakeSecretService *fake = startFake();
    fake->hasDefault = false;
    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("hunter2")));
    QCOMPARE(fake->alias(), QStringLiteral("default"));
    QCOMPARE(fake->collectionLabel(), QStringLiteral("Default keyring"));
    QCOMPARE(fake->promptWindowIds(), QStringList{QString()});
    QCOMPARE(secretOf(m_account), QStringLiteral("hunter2"));
}

void tst_KeychainStoreLinux::unlocksThroughAPrompt()
{
    FakeSecretService *fake = startFake();
    fake->locked = true;
    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("hunter2")));
    QVERIFY(fake->calls().contains(QStringLiteral("Prompt")));

    fake->locked = true;
    Failure why = Failure::Other;
    QCOMPARE(secretOf(m_account, &why), QStringLiteral("hunter2"));
    QCOMPARE(why, Failure::None);
    QCOMPARE(fake->calls().count(QStringLiteral("Prompt")), 2);

    // A service that unlocks without asking needs no prompt at all.
    fake->locked = true;
    fake->unlockNeedsPrompt = false;
    QCOMPARE(secretOf(m_account), QStringLiteral("hunter2"));
    QCOMPARE(fake->calls().count(QStringLiteral("Prompt")), 2);
}

void tst_KeychainStoreLinux::dismissedPromptMeansLocked()
{
    FakeSecretService *fake = startFake();
    fake->addItem(QStringLiteral("hunter2"), SecretService::attributesFor(m_service, m_account));
    fake->locked = true;
    fake->promptAnswer = FakeSecretService::PromptAnswer::Dismiss;

    Failure why = Failure::None;
    QVERIFY(!KeychainStore::store(m_service, m_account, QStringLiteral("x"), &why));
    QCOMPARE(why, Failure::Locked);
    QCOMPARE(secretOf(m_account, &why), QStringLiteral("<none>"));
    QCOMPARE(why, Failure::Locked);
    QVERIFY(!KeychainStore::erase(m_service, m_account));
    QCOMPARE(fake->items().size(), 1);

    fake->hasDefault = false;
    fake->locked = false;
    QVERIFY(!KeychainStore::store(m_service, QStringLiteral("other"), QStringLiteral("x"), &why));
    QCOMPARE(why, Failure::Locked);
}

void tst_KeychainStoreLinux::unansweredPromptGivesUp()
{
    FakeSecretService *fake = startFake();
    fake->locked = true;
    fake->promptAnswer = FakeSecretService::PromptAnswer::Never;

    QElapsedTimer timer;
    timer.start();
    SecretService::Client client(QDBusConnection::sessionBus(), 300);
    QVERIFY(!client.unlock(paths({kCollection})));
    QCOMPARE(client.failure(), Failure::Locked);
    QVERIFY2(timer.elapsed() < 3000, qPrintable(QStringLiteral("took %1 ms").arg(timer.elapsed())));
    // Left for the user: gnome-keyring aborts when a pending unlock prompt is dismissed.
    QVERIFY(!fake->calls().contains(QStringLiteral("Dismiss")));
}

void tst_KeychainStoreLinux::erasesEveryCopy()
{
    FakeSecretService *fake = startFake();
    const Attributes attributes = SecretService::attributesFor(m_service, m_account);
    fake->addItem(QStringLiteral("one"), attributes);
    fake->addItem(QStringLiteral("two"), attributes);
    fake->addItem(QStringLiteral("kept"), SecretService::attributesFor(m_service, QStringLiteral("other")));
    QVERIFY(KeychainStore::erase(m_service, m_account));
    QCOMPARE(fake->items().size(), 1);
    QCOMPARE(fake->items().first().value, QByteArray("kept"));
}

void tst_KeychainStoreLinux::probeTouchesNoItem()
{
    FakeSecretService *fake = startFake();
    QString detail;
    QCOMPARE(SecretService::probe(&detail), Failure::None);
    QVERIFY(detail.endsWith(QLatin1String(" answers")));
    QTRY_COMPARE(fake->calls(), (QStringList{QStringLiteral("OpenSession"), QStringLiteral("Close")}));
}

void tst_KeychainStoreLinux::rejectsEmptyIds()
{
    FakeSecretService *fake = startFake();
    QVERIFY(!KeychainStore::store(QString(), m_account, QStringLiteral("x")));
    QVERIFY(!KeychainStore::store(m_service, QString(), QStringLiteral("x")));
    QVERIFY(!KeychainStore::retrieve(QString(), m_account).has_value());
    QVERIFY(!KeychainStore::erase(m_service, QString()));
    QVERIFY(fake->calls().isEmpty());
}

void tst_KeychainStoreLinux::roundTripsThroughGnomeKeyring()
{
    const QString daemon = QStandardPaths::findExecutable(QStringLiteral("gnome-keyring-daemon"));
    if (daemon.isEmpty())
        QSKIP("gnome-keyring-daemon is not installed");
    QVERIFY(startGnomeKeyring(daemon));
    QString detail;
    QCOMPARE(SecretService::probe(&detail), Failure::None);
    qInfo() << "probe:" << detail;

    Failure why = Failure::Other;
    QCOMPARE(secretOf(m_account, &why), QStringLiteral("<none>"));
    QCOMPARE(why, Failure::None);
    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("hunter2"), &why));
    QCOMPARE(why, Failure::None);
    QCOMPARE(secretOf(m_account), QStringLiteral("hunter2"));

    const QString replaced = QStringLiteral("päss wörd ✓");
    QVERIFY(KeychainStore::store(m_service, m_account, replaced));
    QCOMPARE(secretOf(m_account), replaced);
    QVERIFY(KeychainStore::store(m_service, QStringLiteral("account-2"), QStringLiteral("second")));
    QCOMPARE(secretOf(QStringLiteral("account-2")), QStringLiteral("second"));

    // Overwriting replaced the item instead of adding one.
    SecretService::Client client;
    QList<QDBusObjectPath> unlocked;
    QList<QDBusObjectPath> locked;
    QVERIFY(client.search(SecretService::attributesFor(m_service, m_account), &unlocked, &locked));
    QCOMPARE(unlocked.size() + locked.size(), 1);

    QVERIFY(KeychainStore::erase(m_service, m_account));
    QCOMPARE(secretOf(m_account, &why), QStringLiteral("<none>"));
    QCOMPARE(why, Failure::None);
    QVERIFY(KeychainStore::erase(m_service, m_account));
    QCOMPARE(secretOf(QStringLiteral("account-2")), QStringLiteral("second"));
}

void tst_KeychainStoreLinux::lockedGnomeKeyringWithoutPrompterFails()
{
    const QString daemon = QStandardPaths::findExecutable(QStringLiteral("gnome-keyring-daemon"));
    if (daemon.isEmpty())
        QSKIP("gnome-keyring-daemon is not installed");
    QVERIFY(startGnomeKeyring(daemon));
    QVERIFY(KeychainStore::store(m_service, m_account, QStringLiteral("hunter2")));

    SecretService::Client client;
    const auto collection = client.defaultCollection();
    QVERIFY(collection.has_value());
    QDBusMessage lock = QDBusMessage::createMethodCall(kService, kRoot,
                                                       QStringLiteral("org.freedesktop.Secret.Service"),
                                                       QStringLiteral("Lock"));
    lock.setArguments({QVariant::fromValue(QList<QDBusObjectPath>{*collection})});
    QCOMPARE(QDBusConnection::sessionBus().call(lock).type(), QDBusMessage::ReplyMessage);
    QVERIFY(client.isLocked(*collection).value_or(false));

    // Nothing on this bus can show gnome-keyring's prompt, so it fails at once.
    QElapsedTimer timer;
    timer.start();
    Failure why = Failure::None;
    QCOMPARE(secretOf(m_account, &why), QStringLiteral("<none>"));
    QCOMPARE(why, Failure::Locked);
    QVERIFY(!KeychainStore::store(m_service, m_account, QStringLiteral("x"), &why));
    QCOMPARE(why, Failure::Locked);
    QVERIFY2(timer.elapsed() < 10000, qPrintable(QStringLiteral("took %1 ms").arg(timer.elapsed())));
}

QTEST_GUILESS_MAIN(tst_KeychainStoreLinux)
#include "tst_keychainstore_linux.moc"
