#ifndef UPLOAD_UPLOADCONFIG_H
#define UPLOAD_UPLOADCONFIG_H

#include "core/KeychainStore.h"
#include "core/Settings.h"
#include "upload/UploadProfiles.h"

#include <QString>

namespace Upload {

/**
 * A snapshot of one upload destination: a profile's non-secret fields plus the secret
 * pulled from the OS keychain (keyed by the profile id, in its type's service). Built
 * just-in-time via forProfile() so the secret is read at sign time and never persisted
 * in QSettings. No streaming operator is defined on purpose - the secret must never end
 * up in a log.
 */
struct UploadConfig {
    bool enabled = false;
    ProviderType type = ProviderType::S3;
    QString secretKey;       // from keychain: S3 secret / SFTP password or key passphrase
                             // / FTP password; empty if absent

    // S3
    QString endpoint;        // host only, e.g. "s3.amazonaws.com"
    QString region;          // "us-east-1", "auto" (R2), ...
    QString bucket;
    QString accessKeyId;
    QString keyPrefix;       // e.g. "screenshots/"
    bool forcePathStyle = false;

    // SFTP / FTP
    QString host;
    int port = 0;            // 0 = the protocol default (22 / 21 / 990)
    QString username;        // FTP: empty = anonymous
    QString remoteDir;
    SftpAuthMode sftpAuth = SftpAuthMode::Password;
    QString privateKeyPath;
    FtpEncryption ftpEncryption = FtpEncryption::Explicit;

    QString publicBaseUrl;   // override for the returned link (required for R2)

    // Pure config check, per type - "is this destination filled in", never "is its
    // backend compiled in" (that is UploaderFactory::isStrategyAvailable). So a
    // configured SFTP profile in a build without libssh2 still passes here and gets a
    // precise "not included in this build" failure instead of a vague "not configured".
    [[nodiscard]] bool isComplete() const {
        if (!enabled)
            return false;
        switch (type) {
        case ProviderType::Sftp:
            // Key auth needs no stored secret (the passphrase is optional), which also
            // keeps it usable on the stub-keychain platforms.
            return !host.isEmpty() && !username.isEmpty()
                   && (sftpAuth == SftpAuthMode::PrivateKey ? !privateKeyPath.isEmpty()
                                                            : !secretKey.isEmpty());
        case ProviderType::Ftp:
            return !host.isEmpty() && (username.isEmpty() || !secretKey.isEmpty());
        case ProviderType::S3:
            break;
        }
        return !endpoint.isEmpty() && !region.isEmpty() && !bucket.isEmpty()
               && !accessKeyId.isEmpty() && !secretKey.isEmpty();
    }

    // Snapshot the given profile (empty id = the default profile). enabled comes from
    // the global toggle; the secret from the keychain under the profile's id.
    [[nodiscard]] static UploadConfig forProfile(const QString &profileId) {
        const UploadProfile p = profileId.isEmpty() ? UploadProfiles::defaultProfile()
                                                    : UploadProfiles::byId(profileId);
        UploadConfig c;
        c.enabled = Core::Settings::uploadEnabled();
        c.type = p.type;
        c.endpoint = p.endpoint;
        c.region = p.region;
        c.bucket = p.bucket;
        c.accessKeyId = p.accessKeyId;
        c.keyPrefix = p.keyPrefix;
        c.forcePathStyle = p.forcePathStyle;
        c.host = p.host;
        c.port = p.port;
        c.username = p.username;
        c.remoteDir = p.remoteDir;
        c.sftpAuth = p.sftpAuth;
        c.privateKeyPath = p.privateKeyPath;
        c.ftpEncryption = p.ftpEncryption;
        c.publicBaseUrl = p.publicBaseUrl;
        if (!p.id.isEmpty()) {
            const auto secret = Core::KeychainStore::retrieve(keychainServiceFor(p.type), p.id);
            if (secret)
                c.secretKey = *secret;
        }
        return c;
    }
};

} // namespace Upload

#endif // UPLOAD_UPLOADCONFIG_H
