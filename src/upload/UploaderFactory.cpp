#include "upload/UploaderFactory.h"
#include "upload/ProviderRegistry.h"
#include "upload/UploadConfig.h"
#include "upload/strategies/StubUploader.h"

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
    const ProviderSpec *spec = ProviderRegistry::find(cfg.type);
    if (!spec || !cfg.isComplete())
        return std::make_unique<StubUploader>(QString(), parent);
    if (!spec->available())
        return std::make_unique<StubUploader>(
            StubUploader::tr("%1 support is not included in this build.").arg(spec->displayName),
            parent);
    return spec->create(cfg, parent);
}

bool UploaderFactory::isAvailable(const QString &type)
{
    const ProviderSpec *spec = ProviderRegistry::find(type);
    return spec && spec->available();
}

} // namespace Upload
