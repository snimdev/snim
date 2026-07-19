#include "core/SecretService.h"
#include "core/Sandbox.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QScopeGuard>
#include <QTimer>

#include <utility>

namespace Core::SecretService {

namespace {

const QString kService = QStringLiteral("org.freedesktop.secrets");
const QString kServicePath = QStringLiteral("/org/freedesktop/secrets");
const QString kServiceInterface = QStringLiteral("org.freedesktop.Secret.Service");
const QString kCollectionInterface = QStringLiteral("org.freedesktop.Secret.Collection");
const QString kItemInterface = QStringLiteral("org.freedesktop.Secret.Item");
const QString kSessionInterface = QStringLiteral("org.freedesktop.Secret.Session");
const QString kPromptInterface = QStringLiteral("org.freedesktop.Secret.Prompt");
const QString kPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

// Collects one prompt's Completed(dismissed, result) and ends the wait.
class PromptWaiter : public QObject
{
    Q_OBJECT

public:
    QEventLoop loop;
    bool completed = false;
    bool dismissed = true;
    QVariant result;

public slots:
    void onCompleted(bool wasDismissed, const QDBusVariant &value)
    {
        completed = true;
        dismissed = wasDismissed;
        result = value.variant();
        loop.quit();
    }
};

QDBusObjectPath pathArgument(const QDBusMessage &reply, int index)
{
    return qdbus_cast<QDBusObjectPath>(reply.arguments().value(index));
}

QList<QDBusObjectPath> pathListArgument(const QDBusMessage &reply, int index)
{
    return qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().value(index));
}

// Which process serves the name, for the self-test. Empty in Flatpak, whose /proc holds
// none of the host's processes.
QString ownerProcessName(const QDBusConnection &bus)
{
    if (Sandbox::isFlatpak())
        return {};
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetConnectionUnixProcessID"));
    message.setArguments({kService});
    const QDBusMessage reply = bus.call(message, QDBus::Block, kCallTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    QFile comm(QStringLiteral("/proc/%1/comm").arg(reply.arguments().constFirst().toUInt()));
    if (!comm.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(comm.readAll()).trimmed();
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &arg, const Secret &secret)
{
    arg.beginStructure();
    arg << secret.session << secret.parameters << secret.value << secret.contentType;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Secret &secret)
{
    arg.beginStructure();
    arg >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    arg.endStructure();
    return arg;
}

void registerMetaTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<Secret>();
        qDBusRegisterMetaType<Attributes>();
        return true;
    }();
    Q_UNUSED(registered);
}

Attributes attributesFor(const QString &service, const QString &account)
{
    return {
        {QStringLiteral("service"), service},
        {QStringLiteral("account"), account},
        {QStringLiteral("xdg:schema"), QStringLiteral("dev.snim.Snim.Secret")},
    };
}

QString labelFor(const QString &service, const QString &account)
{
    return QStringLiteral("Snim upload secret (%1, %2)").arg(service, account);
}

QVariantMap itemProperties(const QString &service, const QString &account)
{
    return {
        {QStringLiteral("org.freedesktop.Secret.Item.Label"), labelFor(service, account)},
        {QStringLiteral("org.freedesktop.Secret.Item.Attributes"),
         QVariant::fromValue(attributesFor(service, account))},
    };
}

Secret textSecret(const QDBusObjectPath &session, const QString &text)
{
    return {session, QByteArray(), text.toUtf8(), QStringLiteral("text/plain; charset=utf8")};
}

bool isNoObject(const QDBusObjectPath &path)
{
    return path.path().isEmpty() || path.path() == QLatin1String("/");
}

KeychainStore::Failure failureForError(const QString &errorName)
{
    using KeychainStore::Failure;
    // A service that is absent, failed to start or never answers looks the same to the user.
    if (errorName == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.Disconnected")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.NoReply")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.Timeout")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.TimedOut")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.NoServer")
        || errorName.startsWith(QLatin1String("org.freedesktop.DBus.Error.Spawn.")))
        return Failure::NoService;
    if (errorName == QLatin1String("org.freedesktop.Secret.Error.IsLocked"))
        return Failure::Locked;
    return Failure::Other;
}

Client::Client(const QDBusConnection &bus, int promptTimeoutMs)
    : m_bus(bus)
    , m_promptTimeoutMs(promptTimeoutMs)
{
    registerMetaTypes();
}

Client::~Client()
{
    // Fire and forget: the service also drops the session when this connection goes.
    if (m_session)
        m_bus.send(QDBusMessage::createMethodCall(kService, m_session->path(), kSessionInterface,
                                                  QStringLiteral("Close")));
}

void Client::fail(KeychainStore::Failure failure, const QString &what)
{
    if (m_failure == KeychainStore::Failure::None)
        m_failure = failure;
    if (failure == KeychainStore::Failure::NoService) {
        static bool told = false;
        if (!std::exchange(told, true))
            qInfo().noquote() << QStringLiteral("Keychain: no Secret Service on the session bus "
                                                "(%1), upload secrets cannot be kept").arg(what);
        return;
    }
    qWarning().noquote() << "Keychain:" << what;
}

std::optional<QDBusMessage> Client::call(const QString &path, const QString &interface,
                                         const QString &method, const QVariantList &args,
                                         int replyArguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(kService, path, interface, method);
    message.setArguments(args);
    const QDBusMessage reply = m_bus.call(message, QDBus::Block, kCallTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        fail(failureForError(reply.errorName()),
             QStringLiteral("%1 failed: %2 %3").arg(method, reply.errorName(), reply.errorMessage()));
        return std::nullopt;
    }
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < replyArguments) {
        fail(KeychainStore::Failure::Other, QStringLiteral("%1 got a malformed reply").arg(method));
        return std::nullopt;
    }
    return reply;
}

std::optional<QDBusObjectPath> Client::session()
{
    if (m_session)
        return m_session;
    // The session bus is private to this user, so the plain algorithm skips the DH exchange.
    const auto reply = call(kServicePath, kServiceInterface, QStringLiteral("OpenSession"),
                            {QStringLiteral("plain"), QVariant::fromValue(QDBusVariant(QString()))}, 2);
    if (!reply)
        return std::nullopt;
    const QDBusObjectPath path = pathArgument(*reply, 1);
    if (isNoObject(path)) {
        fail(KeychainStore::Failure::Other, QStringLiteral("OpenSession returned no session"));
        return std::nullopt;
    }
    m_session = path;
    return m_session;
}

std::optional<QDBusObjectPath> Client::defaultCollection()
{
    const auto alias = call(kServicePath, kServiceInterface, QStringLiteral("ReadAlias"),
                            {QStringLiteral("default")}, 1);
    if (!alias)
        return std::nullopt;
    const QDBusObjectPath existing = pathArgument(*alias, 0);
    if (!isNoObject(existing))
        return existing;

    // A fresh keyring has no default collection yet; the service may prompt for its password.
    const QVariantMap properties{{QStringLiteral("org.freedesktop.Secret.Collection.Label"),
                                  QStringLiteral("Default keyring")}};
    const auto created = call(kServicePath, kServiceInterface, QStringLiteral("CreateCollection"),
                              {properties, QStringLiteral("default")}, 2);
    if (!created)
        return std::nullopt;
    QDBusObjectPath collection = pathArgument(*created, 0);
    const QDBusObjectPath promptPath = pathArgument(*created, 1);
    if (isNoObject(collection) && !isNoObject(promptPath)) {
        const auto result = prompt(promptPath);
        if (!result)
            return std::nullopt;
        collection = qdbus_cast<QDBusObjectPath>(*result);
    }
    if (isNoObject(collection)) {
        fail(KeychainStore::Failure::Other, QStringLiteral("CreateCollection made no collection"));
        return std::nullopt;
    }
    return collection;
}

bool Client::unlock(const QList<QDBusObjectPath> &objects)
{
    const auto reply = call(kServicePath, kServiceInterface, QStringLiteral("Unlock"),
                            {QVariant::fromValue(objects)}, 2);
    if (!reply)
        return false;
    const QDBusObjectPath promptPath = pathArgument(*reply, 1);
    return isNoObject(promptPath) || prompt(promptPath).has_value();
}

std::optional<bool> Client::isLocked(const QDBusObjectPath &collection)
{
    const auto reply = call(collection.path(), kPropertiesInterface, QStringLiteral("Get"),
                            {kCollectionInterface, QStringLiteral("Locked")}, 1);
    if (!reply)
        return std::nullopt;
    return reply->arguments().constFirst().value<QDBusVariant>().variant().toBool();
}

bool Client::search(const Attributes &attributes, QList<QDBusObjectPath> *unlocked,
                    QList<QDBusObjectPath> *locked)
{
    const auto reply = call(kServicePath, kServiceInterface, QStringLiteral("SearchItems"),
                            {QVariant::fromValue(attributes)}, 2);
    if (!reply)
        return false;
    *unlocked = pathListArgument(*reply, 0);
    *locked = pathListArgument(*reply, 1);
    return true;
}

bool Client::createItem(const QDBusObjectPath &collection, const QVariantMap &properties,
                        const Secret &secret)
{
    const auto reply = call(collection.path(), kCollectionInterface, QStringLiteral("CreateItem"),
                            {properties, QVariant::fromValue(secret), true}, 2);
    if (!reply)
        return false;
    const QDBusObjectPath item = pathArgument(*reply, 0);
    const QDBusObjectPath promptPath = pathArgument(*reply, 1);
    if (!isNoObject(item))
        return true;
    if (isNoObject(promptPath)) {
        fail(KeychainStore::Failure::Other, QStringLiteral("CreateItem made no item"));
        return false;
    }
    return prompt(promptPath).has_value();
}

std::optional<Secret> Client::secretOf(const QDBusObjectPath &item)
{
    const auto sessionPath = session();
    if (!sessionPath)
        return std::nullopt;
    const auto reply = call(item.path(), kItemInterface, QStringLiteral("GetSecret"),
                            {QVariant::fromValue(*sessionPath)}, 1);
    if (!reply)
        return std::nullopt;
    return qdbus_cast<Secret>(reply->arguments().constFirst());
}

bool Client::deleteItem(const QDBusObjectPath &item)
{
    const auto reply = call(item.path(), kItemInterface, QStringLiteral("Delete"), {}, 1);
    if (!reply)
        return false;
    const QDBusObjectPath promptPath = pathArgument(*reply, 0);
    return isNoObject(promptPath) || prompt(promptPath).has_value();
}

std::optional<QVariant> Client::prompt(const QDBusObjectPath &path)
{
    PromptWaiter waiter;
    // Subscribe before Prompt(): Completed can follow its reply at once.
    if (!m_bus.connect(kService, path.path(), kPromptInterface, QStringLiteral("Completed"),
                       &waiter, SLOT(onCompleted(bool,QDBusVariant)))) {
        fail(KeychainStore::Failure::Other, QStringLiteral("cannot watch prompt %1").arg(path.path()));
        return std::nullopt;
    }
    const auto unsubscribe = qScopeGuard([&] {
        m_bus.disconnect(kService, path.path(), kPromptInterface, QStringLiteral("Completed"),
                         &waiter, SLOT(onCompleted(bool,QDBusVariant)));
    });

    if (!call(path.path(), kPromptInterface, QStringLiteral("Prompt"), {QString()}, 0))
        return std::nullopt;
    if (!waiter.completed) {
        QTimer::singleShot(m_promptTimeoutMs, &waiter.loop, &QEventLoop::quit);
        waiter.loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
    if (!waiter.completed) {
        // Left up, not dismissed: gnome-keyring 50 aborts on Dismiss mid-unlock.
        fail(KeychainStore::Failure::Locked, QStringLiteral("prompt still unanswered, gave up"));
        return std::nullopt;
    }
    if (waiter.dismissed) {
        fail(KeychainStore::Failure::Locked, QStringLiteral("prompt dismissed"));
        return std::nullopt;
    }
    return waiter.result;
}

KeychainStore::Failure probe(QString *detail, const QDBusConnection &bus)
{
    KeychainStore::Failure failure = KeychainStore::Failure::None;
    {
        Client client(bus);
        if (!client.session())
            failure = client.failure();
    }
    if (detail) {
        if (failure == KeychainStore::Failure::None) {
            const QString owner = ownerProcessName(bus);
            *detail = owner.isEmpty() ? QStringLiteral("Secret Service answers")
                                      : QStringLiteral("%1 answers").arg(owner);
        } else {
            *detail = failure == KeychainStore::Failure::NoService
                ? QStringLiteral("no Secret Service on the session bus")
                : QStringLiteral("the Secret Service refused a session");
        }
    }
    return failure;
}

} // namespace Core::SecretService

#include "SecretService.moc"
