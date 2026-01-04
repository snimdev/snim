#ifndef UPLOAD_STUBUPLOADER_H
#define UPLOAD_STUBUPLOADER_H

#include "upload/Uploader.h"

#include <utility>

namespace Upload {

// Universal fallback: not configured, and any upload attempt fails (deferred, to match
// the real backend's async contract). Selected when no backend is configured.
class StubUploader : public Uploader
{
    Q_OBJECT

public:
    // An empty message keeps the generic "not configured" failure; the factory passes a
    // specific one for a backend whose optional dependency wasn't compiled in.
    explicit StubUploader(QString message = {}, QObject *parent = nullptr)
        : Uploader(parent), m_message(std::move(message)) {}

    void upload(const QString &localPath, const QString &keyHint) override;
    void testConnection() override;
    [[nodiscard]] bool isConfigured() const override { return false; }
    [[nodiscard]] QString name() const override { return QStringLiteral("None"); }

private:
    QString m_message;
};

} // namespace Upload

#endif // UPLOAD_STUBUPLOADER_H
