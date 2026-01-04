#ifndef UPLOAD_STUBUPLOADER_H
#define UPLOAD_STUBUPLOADER_H

#include "upload/Uploader.h"

namespace Upload {

// Universal fallback: not configured, and any upload attempt fails (deferred, to match
// the real backend's async contract). Selected when no backend is configured.
class StubUploader : public Uploader
{
    Q_OBJECT

public:
    explicit StubUploader(QObject *parent = nullptr) : Uploader(parent) {}

    void upload(const QString &localPath, const QString &keyHint) override;
    [[nodiscard]] bool isConfigured() const override { return false; }
    [[nodiscard]] QString name() const override { return QStringLiteral("None"); }
};

} // namespace Upload

#endif // UPLOAD_STUBUPLOADER_H
