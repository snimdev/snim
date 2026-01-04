#ifndef UPLOAD_UPLOADERFACTORY_H
#define UPLOAD_UPLOADERFACTORY_H

#include "upload/Uploader.h"

#include <memory>

namespace Upload {

/**
 * Picks the active Uploader from settings. Mirrors RecordingFactory: an enum with the
 * backends, an Auto that resolves to the configured one (S3 today; Ftp/Sftp/Http are
 * reserved slots), and a Stub fallback so the app always has a valid (inert) uploader.
 */
class UploaderFactory
{
public:
    enum class StrategyType { Auto, S3, Stub /*, Ftp, Sftp, Http (reserved) */ };

    static std::unique_ptr<Uploader> create(StrategyType type = StrategyType::Auto,
                                            QObject *parent = nullptr);
    static StrategyType getDefaultStrategyType();
    static bool isStrategyAvailable(StrategyType type);
};

} // namespace Upload

#endif // UPLOAD_UPLOADERFACTORY_H
