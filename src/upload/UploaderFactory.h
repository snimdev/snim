#ifndef UPLOAD_UPLOADERFACTORY_H
#define UPLOAD_UPLOADERFACTORY_H

#include "upload/Uploader.h"

#include <memory>

namespace Upload {

struct UploadConfig;

/**
 * Picks the active Uploader from settings. Mirrors RecordingFactory: an enum with the
 * backends, an Auto that resolves to the requested profile's type, and a Stub fallback
 * so the app always has a valid (inert) uploader. The enum is never persisted, so it is
 * safe to extend; the Sftp/Ftp members exist in every build even when their optional
 * dependency (libssh2 / libcurl) isn't there - those builds just resolve to a Stub that
 * says so.
 */
class UploaderFactory
{
public:
    enum class StrategyType { Auto, S3, Sftp, Ftp, Stub };

    // profileId selects a destination (empty = the default profile).
    static std::unique_ptr<Uploader> create(StrategyType type = StrategyType::Auto,
                                            QObject *parent = nullptr,
                                            const QString &profileId = QString());
    // Build the backend for an explicit config instead of a stored profile, so the
    // Settings dialog can test values that are typed but not saved (and not in the
    // keychain) yet. Resolution is by cfg.type + compile-time availability only: a type
    // this build lacks yields the Stub that says so, while an incomplete config still
    // yields the real backend, whose testConnection() reports precisely what is missing.
    static std::unique_ptr<Uploader> createForConfig(const UploadConfig &cfg,
                                                     QObject *parent = nullptr);
    static StrategyType getDefaultStrategyType();
    // Compile-time availability ("was this backend built in"), NOT configuredness - the
    // settings UI uses it to grey out provider types it could never run.
    static bool isStrategyAvailable(StrategyType type);
};

} // namespace Upload

#endif // UPLOAD_UPLOADERFACTORY_H
