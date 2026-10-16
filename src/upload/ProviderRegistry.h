#ifndef UPLOAD_PROVIDERREGISTRY_H
#define UPLOAD_PROVIDERREGISTRY_H

#include "upload/Uploader.h"

#include <QCoreApplication>
#include <QList>
#include <QString>
#include <functional>
#include <memory>

namespace Upload {

struct UploadConfig;

/**
 * Declarative description of one upload provider: its persisted id and UI name, the
 * keychain service holding its secrets, when a destination is filled in and how its
 * uploader is built. Adding a provider is one ProviderSpec entry in ProviderRegistry.cpp
 * (plus its strategy's sources). Plain data, so it stays below the UI layer.
 */
struct ProviderSpec {
    QString id;                  // persisted as the profile type: "s3", "sftp", "ftp"
    QString displayName;         // "S3", "SFTP", "FTP"
    // Core::KeychainStore service for this provider's secrets (account = the profile id).
    // Frozen: existing keychain items were written under it.
    QString keychainService;
    QString missingDependency;   // why the backend is not in this build; empty = it is

    // Pure config, "is this destination filled in", never "is its backend built in".
    std::function<bool(const UploadConfig &)> isComplete;
    // A complete config, secret included; null when the backend is not in this build.
    std::function<std::unique_ptr<Uploader>(const UploadConfig &, QObject *)> create;

    [[nodiscard]] bool available() const { return missingDependency.isEmpty(); }
};

// The built-in providers, in Add-menu order. One explicit list rather than
// self-registration: snim_lib is static, so the linker would drop unreferenced entries.
class ProviderRegistry
{
    Q_DECLARE_TR_FUNCTIONS(Upload::ProviderRegistry)

public:
    static const QList<ProviderSpec> &providers();
    static const ProviderSpec *find(const QString &id);   // null when no provider uses id
};

} // namespace Upload

#endif // UPLOAD_PROVIDERREGISTRY_H
