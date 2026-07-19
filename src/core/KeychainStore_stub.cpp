#include "core/KeychainStore.h"

// Fallback keychain backing (not macOS, Windows or Linux, which has the Secret Service in
// KeychainStore_linux.cpp): a no-op so the app builds and links everywhere. With no secret
// store, an uploader that needs a password reports itself not configured.
namespace Core::KeychainStore {

bool store(const QString &, const QString &, const QString &, Failure *why)
{
    if (why)
        *why = Failure::Other;
    return false;
}

std::optional<QString> retrieve(const QString &, const QString &, Failure *why)
{
    if (why)
        *why = Failure::Other;
    return std::nullopt;
}

bool erase(const QString &, const QString &) { return false; }

} // namespace Core::KeychainStore
