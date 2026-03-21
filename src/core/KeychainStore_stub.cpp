#include "core/KeychainStore.h"

// Fallback keychain backing (neither macOS nor Windows): a no-op so the app builds and
// links everywhere. With no secret store, the S3 uploader's isConfigured() returns false
// and the Upload action stays disabled until a platform-native store is added (libsecret
// would slot in here behind the same Core::KeychainStore facade).
namespace Core::KeychainStore {

bool store(const QString &, const QString &, const QString &) { return false; }

std::optional<QString> retrieve(const QString &, const QString &) { return std::nullopt; }

bool erase(const QString &, const QString &) { return false; }

} // namespace Core::KeychainStore
