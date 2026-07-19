#ifndef CORE_KEYCHAINSTORE_H
#define CORE_KEYCHAINSTORE_H

#include <QString>
#include <optional>

/**
 * Tiny secrets vault: stores a secret string under (service, account) in the OS
 * keychain. macOS uses Security.framework (KeychainStore_mac.mm), Windows the Credential
 * Manager (KeychainStore_win.cpp); other platforms get a no-op stub
 * (KeychainStore_stub.cpp) so the app still builds and links - with the stub, store()
 * returns false and retrieve() returns nullopt, which makes the S3 uploader report
 * itself not-configured rather than crash.
 *
 * Only upload-destination secrets go here (the S3 secret access key, an SFTP/FTP
 * password or key passphrase); all non-secret config lives in Core::Settings.
 */
namespace Core::KeychainStore {

// One service id per upload provider type; account = the profile id. (The type -> service
// mapping is Upload::keychainServiceFor - Core must not know the Upload enums.) The ids
// changed in the Snim rebrand: secrets stored under the old com.darkog.niceshot.* services
// are not migrated and must be re-entered.
inline QString s3Service() { return QStringLiteral("dev.snim.s3"); }
inline QString sftpService() { return QStringLiteral("dev.snim.sftp"); }
inline QString ftpService() { return QStringLiteral("dev.snim.ftp"); }

// Why a call failed, so the UI can say what to do. NoService (no keyring daemon answers)
// and Locked (an unlock prompt was dismissed) come from backings that can tell them apart.
enum class Failure { None, NoService, Locked, Other };

bool store(const QString &service, const QString &account, const QString &secret,
           Failure *why = nullptr);
// nullopt with *why == Failure::None means nothing is stored under the pair.
[[nodiscard]] std::optional<QString> retrieve(const QString &service, const QString &account,
                                              Failure *why = nullptr);
bool erase(const QString &service, const QString &account);

} // namespace Core::KeychainStore

#endif // CORE_KEYCHAINSTORE_H
