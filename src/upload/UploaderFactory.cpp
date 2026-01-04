#include "upload/UploaderFactory.h"
#include "upload/UploadConfig.h"
#include "upload/strategies/S3Uploader.h"
#include "upload/strategies/StubUploader.h"

namespace Upload {

std::unique_ptr<Uploader> UploaderFactory::create(StrategyType type, QObject *parent,
                                                  const QString &profileId)
{
    if (type == StrategyType::Auto)
        type = getDefaultStrategyType();

    switch (type) {
    case StrategyType::S3: {
        auto s3 = std::make_unique<S3Uploader>(profileId, parent);
        if (s3->isConfigured())
            return s3;
        [[fallthrough]];   // bucket/secret missing -> inert stub
    }
    case StrategyType::Stub:
    default:
        return std::make_unique<StubUploader>(parent);
    }
}

UploaderFactory::StrategyType UploaderFactory::getDefaultStrategyType()
{
    return UploadConfig::forProfile(QString()).isComplete() ? StrategyType::S3 : StrategyType::Stub;
}

bool UploaderFactory::isStrategyAvailable(StrategyType type)
{
    switch (type) {
    case StrategyType::S3:   return UploadConfig::forProfile(QString()).isComplete();
    case StrategyType::Stub: return true;
    case StrategyType::Auto: return true;
    default:                 return false;
    }
}

} // namespace Upload
