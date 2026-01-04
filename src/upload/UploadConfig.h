#ifndef UPLOAD_UPLOADCONFIG_H
#define UPLOAD_UPLOADCONFIG_H

#include "core/KeychainStore.h"
#include "core/Settings.h"
#include "upload/UploadProfiles.h"

#include <QString>

namespace Upload {

/**
 * A snapshot of one upload destination: a profile's non-secret fields plus the secret
 * access key pulled from the OS keychain (keyed by the profile id). Built just-in-time
 * via forProfile() so the secret is read at sign time and never persisted in QSettings.
 * No streaming operator is defined on purpose — the secret must never end up in a log.
 */
struct UploadConfig {
    bool enabled = false;
    QString endpoint;        // host only, e.g. "s3.amazonaws.com"
    QString region;          // "us-east-1", "auto" (R2), ...
    QString bucket;
    QString accessKeyId;
    QString secretKey;       // from keychain; empty if absent
    QString keyPrefix;       // e.g. "screenshots/"
    QString publicBaseUrl;   // override for the returned link (required for R2)
    bool forcePathStyle = false;

    [[nodiscard]] bool isComplete() const {
        return enabled && !endpoint.isEmpty() && !region.isEmpty() && !bucket.isEmpty()
               && !accessKeyId.isEmpty() && !secretKey.isEmpty();
    }

    // Snapshot the given profile (empty id = the default profile). enabled comes from
    // the global toggle; the secret from the keychain under the profile's id.
    [[nodiscard]] static UploadConfig forProfile(const QString &profileId) {
        const UploadProfile p = profileId.isEmpty() ? UploadProfiles::defaultProfile()
                                                    : UploadProfiles::byId(profileId);
        UploadConfig c;
        c.enabled = Core::Settings::uploadEnabled();
        c.endpoint = p.endpoint;
        c.region = p.region;
        c.bucket = p.bucket;
        c.accessKeyId = p.accessKeyId;
        c.keyPrefix = p.keyPrefix;
        c.publicBaseUrl = p.publicBaseUrl;
        c.forcePathStyle = p.forcePathStyle;
        if (!p.id.isEmpty()) {
            const auto secret = Core::KeychainStore::retrieve(
                Core::KeychainStore::s3Service(), p.id);
            if (secret)
                c.secretKey = *secret;
        }
        return c;
    }
};

} // namespace Upload

#endif // UPLOAD_UPLOADCONFIG_H
