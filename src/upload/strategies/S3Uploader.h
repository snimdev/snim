#ifndef UPLOAD_S3UPLOADER_H
#define UPLOAD_S3UPLOADER_H

#include "upload/Uploader.h"
#include "upload/UploadConfig.h"

#include <QNetworkRequest>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;

namespace Upload {

/**
 * S3-compatible uploader: a single streamed PUT signed with AWS SigV4 (works with AWS,
 * Cloudflare R2, MinIO, Wasabi via the endpoint/region/path-style config). Pure Qt - no
 * platform code - so it compiles everywhere.
 */
class S3Uploader : public Uploader
{
    Q_OBJECT

public:
    // A complete config, secret included (see UploaderFactory).
    explicit S3Uploader(const UploadConfig &config, QObject *parent = nullptr);
    ~S3Uploader() override;

    void upload(const QString &localPath, const QString &keyHint) override;
    void testConnection() override;

private:
    void onFinished();
    void onTestPutFinished();
    void onTestDeleteFinished();
    // One SigV4-signed request for one object key. Host + path follow the URL style and
    // feed the signer AND the URL, so the wire path always matches the signed path.
    [[nodiscard]] QNetworkRequest signedRequest(const QString &method, const QString &objectKey,
                                                const QString &contentType,
                                                const QString &payloadHash,
                                                QUrl *urlOut = nullptr) const;

    const UploadConfig m_config;
    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    QUrl m_publicUrl;                 // computed at request time, emitted on success
    bool m_finished = false;          // guards single uploaded()/failed() emission
    QString m_probeKey;               // connection-test object, deleted once written
    bool m_tested = false;            // guards single-shot testConnection()
};

} // namespace Upload

#endif // UPLOAD_S3UPLOADER_H
