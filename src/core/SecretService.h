#ifndef CORE_SECRETSERVICE_H
#define CORE_SECRETSERVICE_H

#include "core/KeychainStore.h"

#include <QByteArray>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QVariant>

#include <optional>

class QDBusArgument;

/**
 * Client for the freedesktop Secret Service API (org.freedesktop.secrets), which GNOME
 * Keyring, KWallet and KeePassXC all serve. Linux backing of Core::KeychainStore
 * (KeychainStore_linux.cpp); Qt DBus only, no libsecret.
 */
namespace Core::SecretService {

// The API's (oayays) Secret struct.
struct Secret {
    QDBusObjectPath session;
    QByteArray parameters;   // empty for the plain algorithm
    QByteArray value;
    QString contentType;
};

using Attributes = QMap<QString, QString>;

inline constexpr int kCallTimeoutMs = 5000;
// The user may be typing a new keyring password, so a prompt gets far longer than a call.
inline constexpr int kPromptTimeoutMs = 120000;

// Registers Secret and Attributes with Qt DBus; safe to call any number of times.
void registerMetaTypes();

// Lookup attributes for one KeychainStore entry; xdg:schema lets libsecret tools name it.
[[nodiscard]] Attributes attributesFor(const QString &service, const QString &account);
// The item label a keyring manager shows.
[[nodiscard]] QString labelFor(const QString &service, const QString &account);
// Collection.CreateItem's a{sv} properties: label and attributes.
[[nodiscard]] QVariantMap itemProperties(const QString &service, const QString &account);
// A UTF-8 text secret for a plain session.
[[nodiscard]] Secret textSecret(const QDBusObjectPath &session, const QString &text);
// "/" means no object: no prompt needed, or no collection under an alias.
[[nodiscard]] bool isNoObject(const QDBusObjectPath &path);
// Sorts a D-Bus error name into the facade's failure kinds.
[[nodiscard]] KeychainStore::Failure failureForError(const QString &errorName);

/**
 * Facade over the service's D-Bus objects (service, collections, items, prompts) for one
 * conversation: every call is bounded by kCallTimeoutMs, prompts wait in a local event
 * loop, and the first failure is kept for failure(). The plain session is opened on
 * first use and closed with the client.
 */
class Client
{
public:
    explicit Client(const QDBusConnection &bus = QDBusConnection::sessionBus(),
                    int promptTimeoutMs = kPromptTimeoutMs);
    ~Client();
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    [[nodiscard]] std::optional<QDBusObjectPath> session();
    // The "default" alias, created with that alias when a fresh system has no collection.
    [[nodiscard]] std::optional<QDBusObjectPath> defaultCollection();
    // Unlocks `objects` (a no-op for an unlocked collection), prompting when asked to.
    bool unlock(const QList<QDBusObjectPath> &objects);
    [[nodiscard]] std::optional<bool> isLocked(const QDBusObjectPath &collection);
    bool search(const Attributes &attributes, QList<QDBusObjectPath> *unlocked,
                QList<QDBusObjectPath> *locked);
    // Stores `secret` as a new item, replacing one with the same attributes.
    bool createItem(const QDBusObjectPath &collection, const QVariantMap &properties,
                    const Secret &secret);
    [[nodiscard]] std::optional<Secret> secretOf(const QDBusObjectPath &item);
    bool deleteItem(const QDBusObjectPath &item);

    [[nodiscard]] KeychainStore::Failure failure() const { return m_failure; }

private:
    // The reply when it carries at least `replyArguments` values; else records why not.
    std::optional<QDBusMessage> call(const QString &path, const QString &interface,
                                     const QString &method, const QVariantList &args,
                                     int replyArguments);
    // Runs a prompt to completion; nullopt when it was dismissed, timed out or failed.
    std::optional<QVariant> prompt(const QDBusObjectPath &path);
    void fail(KeychainStore::Failure failure, const QString &what);

    QDBusConnection m_bus;
    int m_promptTimeoutMs;
    std::optional<QDBusObjectPath> m_session;
    KeychainStore::Failure m_failure = KeychainStore::Failure::None;
};

// Opens and closes a plain session: whether a Secret Service answers, touching no item
// and never prompting. `detail` names the process that owns the service when it can.
[[nodiscard]] KeychainStore::Failure probe(QString *detail,
                                           const QDBusConnection &bus = QDBusConnection::sessionBus());

QDBusArgument &operator<<(QDBusArgument &arg, const Secret &secret);
const QDBusArgument &operator>>(const QDBusArgument &arg, Secret &secret);

} // namespace Core::SecretService

Q_DECLARE_METATYPE(Core::SecretService::Secret)

#endif // CORE_SECRETSERVICE_H
