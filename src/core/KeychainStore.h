#ifndef CORE_KEYCHAINSTORE_H
#define CORE_KEYCHAINSTORE_H

#include <QString>
#include <optional>

namespace Core {

/**
 * Tiny secrets vault: stores a secret string under (service, account) in the OS
 * keychain. macOS uses Security.framework (KeychainStore_mac.mm); other platforms get
 * a no-op stub (KeychainStore_stub.cpp) so the app still builds and links — with the
 * stub, store() returns false and retrieve() returns nullopt, which makes the S3
 * uploader report itself not-configured rather than crash.
 *
 * Only the S3 secret access key goes here; all non-secret config lives in Core::Settings.
 */
namespace KeychainStore {

// Niceshot's keychain service id for S3 credentials; account = the access key id.
inline QString s3Service() { return QStringLiteral("com.darkog.niceshot.s3"); }

bool store(const QString &service, const QString &account, const QString &secret);
[[nodiscard]] std::optional<QString> retrieve(const QString &service, const QString &account);
bool erase(const QString &service, const QString &account);

} // namespace KeychainStore

} // namespace Core

#endif // CORE_KEYCHAINSTORE_H
