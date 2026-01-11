#ifndef CORE_KEYCHAINSTORE_H
#define CORE_KEYCHAINSTORE_H

#include <QString>
#include <optional>

/**
 * Tiny secrets vault: stores a secret string under (service, account) in the OS
 * keychain. macOS uses Security.framework (KeychainStore_mac.mm); other platforms get
 * a no-op stub (KeychainStore_stub.cpp) so the app still builds and links - with the
 * stub, store() returns false and retrieve() returns nullopt, which makes the S3
 * uploader report itself not-configured rather than crash.
 *
 * Only upload-destination secrets go here (the S3 secret access key, an SFTP/FTP
 * password or key passphrase); all non-secret config lives in Core::Settings.
 */
namespace Core::KeychainStore {

// One service id per upload provider type; account = the profile id. (The type -> service
// mapping is Upload::keychainServiceFor - Core must not know the Upload enums.) The S3 id
// is unchanged from before profiles existed, so existing keychain items keep working; its
// account was the access key id until the one-time profile migration re-keyed it.
inline QString s3Service() { return QStringLiteral("com.darkog.niceshot.s3"); }
inline QString sftpService() { return QStringLiteral("com.darkog.niceshot.sftp"); }
inline QString ftpService() { return QStringLiteral("com.darkog.niceshot.ftp"); }

bool store(const QString &service, const QString &account, const QString &secret);
[[nodiscard]] std::optional<QString> retrieve(const QString &service, const QString &account);
bool erase(const QString &service, const QString &account);

} // namespace Core::KeychainStore

#endif // CORE_KEYCHAINSTORE_H
