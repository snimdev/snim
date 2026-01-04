#ifndef UPLOAD_S3UPLOADER_H
#define UPLOAD_S3UPLOADER_H

#include "upload/Uploader.h"
#include "upload/UploadConfig.h"

#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace Upload {

/**
 * S3-compatible uploader: a single streamed PUT signed with AWS SigV4 (works with AWS,
 * Cloudflare R2, MinIO, Wasabi via the endpoint/region/path-style config). Pure Qt - no
 * platform code - so it compiles everywhere; isConfigured() is false until a bucket +
 * keychain secret exist, so an unconfigured app just gets a disabled Upload action.
 */
class S3Uploader : public Uploader
{
    Q_OBJECT

public:
    // profileId empty = the default profile (resolved at upload time).
    explicit S3Uploader(const QString &profileId = QString(), QObject *parent = nullptr);
    ~S3Uploader() override;

    void upload(const QString &localPath, const QString &keyHint) override;
    void cancel() override;
    [[nodiscard]] bool isConfigured() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("S3"); }

private:
    void onFinished();

    QString m_profileId;              // empty = default profile
    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFile *m_file = nullptr;          // streamed body; parented to the reply
    QUrl m_publicUrl;                 // computed at request time, emitted on success
    bool m_finished = false;          // guards single uploaded()/failed() emission
};

} // namespace Upload

#endif // UPLOAD_S3UPLOADER_H
