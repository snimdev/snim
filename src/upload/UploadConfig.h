#ifndef UPLOAD_UPLOADCONFIG_H
#define UPLOAD_UPLOADCONFIG_H

#include "core/KeychainStore.h"
#include "core/Settings.h"
#include "upload/ProviderRegistry.h"
#include "upload/UploadProfiles.h"

#include <QString>

namespace Upload {

/**
 * A snapshot of one upload destination: a profile's non-secret fields plus the secret
 * pulled from the OS keychain (keyed by the profile id, in its type's service). Built
 * just-in-time via forProfile() so the secret is read right before use and never
 * persisted in QSettings. No streaming operator is defined on purpose - the secret must never end
 * up in a log.
 */
struct UploadConfig : UploadProfile {
    bool enabled = false;
    QString secretKey;       // from keychain: S3 secret / SFTP password or key passphrase
                             // / FTP password; empty if absent

    // Pure config check, per provider (ProviderSpec::isComplete) - "is this destination
    // filled in", never "is its backend compiled in" (that is UploaderFactory::isAvailable). So a
    // configured SFTP profile in a build without libssh2 still passes here and gets a
    // precise "not included in this build" failure instead of a vague "not configured".
    [[nodiscard]] bool isComplete() const {
        const ProviderSpec *spec = ProviderRegistry::find(type);
        return enabled && spec && spec->isComplete(*this);
    }

    // Snapshot the given profile (empty id = the default profile). enabled comes from
    // the global toggle; the secret from the keychain under the profile's id.
    [[nodiscard]] static UploadConfig forProfile(const QString &profileId) {
        UploadConfig c{profileId.isEmpty() ? UploadProfiles::defaultProfile()
                                           : UploadProfiles::byId(profileId)};
        c.enabled = Core::Settings::uploadEnabled();
        if (!c.id.isEmpty()) {
            const auto secret = Core::KeychainStore::retrieve(keychainServiceFor(c.type), c.id);
            if (secret)
                c.secretKey = *secret;
        }
        return c;
    }
};

} // namespace Upload

#endif // UPLOAD_UPLOADCONFIG_H
