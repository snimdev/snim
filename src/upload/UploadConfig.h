#ifndef UPLOAD_UPLOADCONFIG_H
#define UPLOAD_UPLOADCONFIG_H

#include "core/KeychainStore.h"
#include "core/Settings.h"

#include <QString>

namespace Upload {

/**
 * A snapshot of the upload destination: non-secret fields from Core::Settings plus the
 * secret access key pulled from the OS keychain. Built once per upload via fromSettings()
 * so the secret is read just-in-time and never persisted in QSettings. No streaming
 * operator is defined on purpose — the secret must never end up in a log.
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

    [[nodiscard]] static UploadConfig fromSettings() {
        UploadConfig c;
        c.enabled = Core::Settings::uploadEnabled();
        c.endpoint = Core::Settings::uploadEndpoint();
        c.region = Core::Settings::uploadRegion();
        c.bucket = Core::Settings::uploadBucket();
        c.accessKeyId = Core::Settings::uploadAccessKeyId();
        c.keyPrefix = Core::Settings::uploadKeyPrefix();
        c.publicBaseUrl = Core::Settings::uploadPublicBaseUrl();
        c.forcePathStyle = Core::Settings::uploadForcePathStyle();
        if (!c.accessKeyId.isEmpty()) {
            const auto secret = Core::KeychainStore::retrieve(
                Core::KeychainStore::s3Service(), c.accessKeyId);
            if (secret)
                c.secretKey = *secret;
        }
        return c;
    }
};

} // namespace Upload

#endif // UPLOAD_UPLOADCONFIG_H
