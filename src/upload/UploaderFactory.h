#ifndef UPLOAD_UPLOADERFACTORY_H
#define UPLOAD_UPLOADERFACTORY_H

#include "upload/Uploader.h"
#include "upload/UploadProfiles.h"

#include <memory>

namespace Upload {

struct UploadConfig;

/**
 * Builds the Uploader for one destination. A backend keeps the config snapshot it was
 * built with, secret included, so it is created right before it is used. A destination
 * that cannot upload (incomplete, or a transport this build lacks) gets a Stub whose
 * failure says why, so the app always has a valid (inert) uploader.
 */
class UploaderFactory
{
public:
    // A saved destination (empty id = the default profile), read from the profile store
    // and the keychain now.
    static std::unique_ptr<Uploader> create(const QString &profileId = QString(),
                                            QObject *parent = nullptr);
    // An explicit config, so the Settings dialog can test values that are typed but not
    // saved (and not in the keychain) yet.
    static std::unique_ptr<Uploader> createForConfig(const UploadConfig &cfg,
                                                     QObject *parent = nullptr);
    // Compile-time availability ("was this backend built in"), NOT configuredness - the
    // settings UI uses it to grey out provider types it could never run.
    static bool isAvailable(ProviderType type);
};

} // namespace Upload

#endif // UPLOAD_UPLOADERFACTORY_H
