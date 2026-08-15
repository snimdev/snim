#include "upload/UploaderFactory.h"
#include "upload/UploadConfig.h"
#include "upload/strategies/S3Uploader.h"
#include "upload/strategies/StubUploader.h"
#ifdef HAVE_LIBCURL
#include "upload/strategies/FtpUploader.h"
#endif
#ifdef HAVE_LIBSSH2
#include "upload/strategies/SftpUploader.h"
#endif

namespace Upload {

std::unique_ptr<Uploader> UploaderFactory::create(const QString &profileId, QObject *parent)
{
    // The REQUESTED profile, not the default one - the ▾ menu uploads to a non-default
    // destination all the time.
    return createForConfig(UploadConfig::forProfile(profileId), parent);
}

std::unique_ptr<Uploader> UploaderFactory::createForConfig(const UploadConfig &cfg,
                                                           QObject *parent)
{
    if (!cfg.isComplete())
        return std::make_unique<StubUploader>(QString(), parent);
    switch (cfg.type) {
    case ProviderType::Sftp:
#ifdef HAVE_LIBSSH2
        return std::make_unique<SftpUploader>(cfg, parent);
#else
        return std::make_unique<StubUploader>(
            StubUploader::tr("SFTP support is not included in this build."), parent);
#endif
    case ProviderType::Ftp:
#ifdef HAVE_LIBCURL
        return std::make_unique<FtpUploader>(cfg, parent);
#else
        return std::make_unique<StubUploader>(
            StubUploader::tr("FTP support is not included in this build."), parent);
#endif
    case ProviderType::S3:
        break;
    }
    return std::make_unique<S3Uploader>(cfg, parent);
}

bool UploaderFactory::isAvailable(ProviderType type)
{
    switch (type) {
#ifndef HAVE_LIBSSH2
    case ProviderType::Sftp: return false;
#endif
#ifndef HAVE_LIBCURL
    case ProviderType::Ftp:  return false;
#endif
    default: return true;   // S3 is pure Qt, plus whatever transport compiled in
    }
}

} // namespace Upload
