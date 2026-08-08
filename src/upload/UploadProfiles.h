#ifndef UPLOAD_UPLOADPROFILES_H
#define UPLOAD_UPLOADPROFILES_H

#include <QString>
#include <QVector>

namespace Upload {

// Which transport a destination speaks. Persisted as a string, and a missing/unknown
// value reads back as S3, so profiles written before this enum existed round-trip intact.
enum class ProviderType { S3, Sftp, Ftp };
enum class SftpAuthMode { Password, PrivateKey };
enum class FtpEncryption { None, Explicit, Implicit };   // plain / AUTH TLS / ftps:// (990)

QString       providerTypeToString(ProviderType t);      // "s3" | "sftp" | "ftp"
ProviderType  providerTypeFromString(const QString &s);  // unknown/missing -> S3
QString       providerDisplayName(ProviderType t);       // "S3" | "SFTP" | "FTP" (UI labels)
QString       sftpAuthModeToString(SftpAuthMode m);      // "password" | "key"
SftpAuthMode  sftpAuthModeFromString(const QString &s);
QString       ftpEncryptionToString(FtpEncryption e);    // "none" | "explicit" | "implicit"
FtpEncryption ftpEncryptionFromString(const QString &s);
// The Core::KeychainStore service id holding this type's secret (account = profile id).
QString       keychainServiceFor(ProviderType t);

/**
 * One saved upload destination. The non-secret config plus a stable `id` (the keychain
 * account for this profile's secret). Mirrors Editor::Image::BackdropPreset. `type`
 * picks which field group applies; the others are simply unused (dedicated fields per
 * type rather than overloaded ones, so nothing is ambiguous at a glance).
 */
struct UploadProfile {
    QString id;              // QUuid (Id128); also the keychain account for the secret
    QString name;            // user-facing label
    ProviderType type = ProviderType::S3;

    // S3
    QString endpoint;        // host only, e.g. "s3.amazonaws.com"
    QString region;          // "us-east-1", "auto" (R2), ...
    QString bucket;
    QString accessKeyId;
    QString keyPrefix;       // e.g. "screenshots/"
    bool forcePathStyle = false;

    // SFTP / FTP
    QString host;
    int port = 0;            // 0 = the protocol default (22 / 21 / 990 for implicit FTPS)
    QString username;        // FTP: empty = anonymous
    QString remoteDir;       // leading "/" = absolute, else relative to the login dir
    SftpAuthMode sftpAuth = SftpAuthMode::Password;
    QString privateKeyPath;  // SFTP key auth; the (optional) passphrase is the secret
    FtpEncryption ftpEncryption = FtpEncryption::Explicit;   // secure default

    // Shared by every type
    QString publicBaseUrl;   // override for the returned link (required for R2)
};

} // namespace Upload

/**
 * Persisted store of upload server profiles + which one is the default, kept as JSON in
 * QSettings (mirrors Editor::Image::BackdropPresets). Secrets are NOT here - each
 * profile's secret lives in the OS keychain under its `id`, in its type's service.
 */
namespace Upload::UploadProfiles {

QVector<UploadProfile> all();
UploadProfile          byId(const QString &id);        // empty id if not found
// Replace the whole list + default in one write (the Settings dialog's commit path).
// Does NOT touch the keychain - the caller reconciles secrets. A defaultId not present
// in the list is cleared (falls back to the first profile, or "" if empty).
void                   setAll(const QVector<UploadProfile> &profiles, const QString &defaultId);

QString       defaultId();
UploadProfile defaultProfile();                        // empty id if no/invalid default

// Make a fresh, unused profile id (QUuid Id128).
QString newId();

} // namespace Upload::UploadProfiles

#endif // UPLOAD_UPLOADPROFILES_H
