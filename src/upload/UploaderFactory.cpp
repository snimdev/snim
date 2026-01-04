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

namespace {

// Which backend one profile asks for (empty id = the default profile). An incomplete
// destination resolves to Stub, so Auto never hands back a backend that can't run.
UploaderFactory::StrategyType resolveForProfile(const QString &profileId)
{
    const UploadConfig cfg = UploadConfig::forProfile(profileId);
    if (!cfg.isComplete())
        return UploaderFactory::StrategyType::Stub;
    switch (cfg.type) {
    case ProviderType::Sftp: return UploaderFactory::StrategyType::Sftp;
    case ProviderType::Ftp:  return UploaderFactory::StrategyType::Ftp;
    case ProviderType::S3:   break;
    }
    return UploaderFactory::StrategyType::S3;
}

} // namespace

std::unique_ptr<Uploader> UploaderFactory::create(StrategyType type, QObject *parent,
                                                  const QString &profileId)
{
    // Resolve from the REQUESTED profile, not the default one - the ▾ menu uploads to a
    // non-default destination all the time.
    if (type == StrategyType::Auto)
        type = resolveForProfile(profileId);

    switch (type) {
    case StrategyType::S3: {
        auto s3 = std::make_unique<S3Uploader>(profileId, parent);
        if (s3->isConfigured())
            return s3;
        break;   // bucket/secret missing -> inert stub
    }
    case StrategyType::Sftp:
#ifdef HAVE_LIBSSH2
    {
        auto sftp = std::make_unique<SftpUploader>(profileId, parent);
        if (sftp->isConfigured())
            return sftp;
        break;   // host/credentials missing -> inert stub
    }
#else
        return std::make_unique<StubUploader>(
            StubUploader::tr("SFTP support is not included in this build."), parent);
#endif
    case StrategyType::Ftp:
#ifdef HAVE_LIBCURL
    {
        auto ftp = std::make_unique<FtpUploader>(profileId, parent);
        if (ftp->isConfigured())
            return ftp;
        break;   // host/password missing -> inert stub
    }
#else
        return std::make_unique<StubUploader>(
            StubUploader::tr("FTP support is not included in this build."), parent);
#endif
    case StrategyType::Stub:
    case StrategyType::Auto:
        break;
    }
    return std::make_unique<StubUploader>(QString(), parent);
}

UploaderFactory::StrategyType UploaderFactory::getDefaultStrategyType()
{
    return resolveForProfile(QString());
}

bool UploaderFactory::isStrategyAvailable(StrategyType type)
{
    switch (type) {
#ifndef HAVE_LIBSSH2
    case StrategyType::Sftp: return false;
#endif
#ifndef HAVE_LIBCURL
    case StrategyType::Ftp:  return false;
#endif
    default: return true;   // S3/Stub/Auto are pure Qt, plus whatever dep compiled in
    }
}

} // namespace Upload
