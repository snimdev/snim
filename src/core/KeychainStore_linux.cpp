#include "core/KeychainStore.h"
#include "core/SecretService.h"

// Linux backing for Core::KeychainStore: one Secret Service item per (service, account) in
// the default collection, found again by its attributes in whichever collection holds it.
namespace Core::KeychainStore {

namespace {

void report(Failure *why, Failure failure)
{
    if (why)
        *why = failure;
}

} // namespace

bool store(const QString &service, const QString &account, const QString &secret, Failure *why)
{
    report(why, Failure::Other);
    if (service.isEmpty() || account.isEmpty())
        return false;
    SecretService::Client client;
    const auto session = client.session();
    const auto collection = session ? client.defaultCollection() : std::nullopt;
    const auto locked = collection ? client.isLocked(*collection) : std::nullopt;
    const bool ok = locked && (!*locked || client.unlock({*collection}))
                    && client.createItem(*collection, SecretService::itemProperties(service, account),
                                         SecretService::textSecret(*session, secret));
    report(why, ok ? Failure::None : client.failure());
    return ok;
}

std::optional<QString> retrieve(const QString &service, const QString &account, Failure *why)
{
    report(why, Failure::Other);
    if (service.isEmpty() || account.isEmpty())
        return std::nullopt;
    SecretService::Client client;
    const SecretService::Attributes attributes = SecretService::attributesFor(service, account);
    QList<QDBusObjectPath> unlocked;
    QList<QDBusObjectPath> locked;
    bool found = client.search(attributes, &unlocked, &locked);
    // Only a locked copy: unlock it, then look again for what that opened.
    if (found && unlocked.isEmpty() && !locked.isEmpty())
        found = client.unlock(locked) && client.search(attributes, &unlocked, &locked);
    if (!found) {
        report(why, client.failure());
        return std::nullopt;
    }
    if (unlocked.isEmpty()) {
        report(why, locked.isEmpty() ? Failure::None : Failure::Locked);
        return std::nullopt;
    }
    const auto value = client.secretOf(unlocked.constFirst());
    if (!value) {
        report(why, client.failure());
        return std::nullopt;
    }
    report(why, Failure::None);
    return QString::fromUtf8(value->value);
}

bool erase(const QString &service, const QString &account)
{
    if (service.isEmpty() || account.isEmpty())
        return false;
    SecretService::Client client;
    QList<QDBusObjectPath> unlocked;
    QList<QDBusObjectPath> locked;
    if (!client.search(SecretService::attributesFor(service, account), &unlocked, &locked))
        return false;
    if (!locked.isEmpty() && !client.unlock(locked))
        return false;
    for (const QDBusObjectPath &item : unlocked + locked) {
        if (!client.deleteItem(item))
            return false;
    }
    return true;
}

} // namespace Core::KeychainStore
