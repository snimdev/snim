#include "upload/ProviderRegistry.h"
#include "upload/UploadConfig.h"
#include "upload/strategies/S3Uploader.h"
#ifdef HAVE_LIBCURL
#include "upload/strategies/FtpUploader.h"
#endif
#ifdef HAVE_LIBSSH2
#include "upload/strategies/SftpUploader.h"
#endif

namespace Upload {

const QList<ProviderSpec> &ProviderRegistry::providers()
{
    // The keychain ids changed in the Snim rebrand: secrets stored under the old
    // com.darkog.niceshot.* services are not migrated and must be re-entered.
    static const QList<ProviderSpec> specs = {
        {
            .id = QStringLiteral("s3"),
            .displayName = QStringLiteral("S3"),
            .keychainService = QStringLiteral("dev.snim.s3"),
            .isComplete = [](const UploadConfig &c) {
                return !c.endpoint.isEmpty() && !c.region.isEmpty() && !c.bucket.isEmpty()
                       && !c.accessKeyId.isEmpty() && !c.secretKey.isEmpty();
            },
            .create = [](const UploadConfig &c, QObject *parent) -> std::unique_ptr<Uploader> {
                return std::make_unique<S3Uploader>(c, parent);
            },
        },
        {
            .id = QStringLiteral("sftp"),
            .displayName = QStringLiteral("SFTP"),
            .keychainService = QStringLiteral("dev.snim.sftp"),
#ifndef HAVE_LIBSSH2
            .missingDependency = tr("Requires libssh2, which was not included in this build."),
#endif
            .isComplete = [](const UploadConfig &c) {
                // Key auth needs no stored secret (the passphrase is optional), which also
                // keeps it usable on the stub-keychain platforms.
                return !c.host.isEmpty() && !c.username.isEmpty()
                       && (c.sftpAuth == SftpAuthMode::PrivateKey ? !c.privateKeyPath.isEmpty()
                                                                  : !c.secretKey.isEmpty());
            },
#ifdef HAVE_LIBSSH2
            .create = [](const UploadConfig &c, QObject *parent) -> std::unique_ptr<Uploader> {
                return std::make_unique<SftpUploader>(c, parent);
            },
#endif
        },
        {
            .id = QStringLiteral("ftp"),
            .displayName = QStringLiteral("FTP"),
            .keychainService = QStringLiteral("dev.snim.ftp"),
#ifndef HAVE_LIBCURL
            .missingDependency = tr("Requires libcurl, which was not included in this build."),
#endif
            .isComplete = [](const UploadConfig &c) {
                return !c.host.isEmpty() && (c.username.isEmpty() || !c.secretKey.isEmpty());
            },
#ifdef HAVE_LIBCURL
            .create = [](const UploadConfig &c, QObject *parent) -> std::unique_ptr<Uploader> {
                return std::make_unique<FtpUploader>(c, parent);
            },
#endif
        },
    };
    return specs;
}

const ProviderSpec *ProviderRegistry::find(const QString &id)
{
    for (const ProviderSpec &spec : providers())
        if (spec.id == id)
            return &spec;
    return nullptr;
}

} // namespace Upload
