#ifndef UPLOAD_S3UPLOADER_H
#define UPLOAD_S3UPLOADER_H

#include "upload/Uploader.h"
#include "upload/UploadConfig.h"

#include <QNetworkRequest>
#include <QPointer>

#include <optional>

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
    // Explicit config, used by UploaderFactory::createForConfig so the Settings dialog can
    // test values the user has typed but not saved yet: every lookup then reads this
    // snapshot instead of the profile store + keychain.
    explicit S3Uploader(const UploadConfig &config, QObject *parent = nullptr);
    ~S3Uploader() override;

    void upload(const QString &localPath, const QString &keyHint) override;
    void testConnection() override;
    void cancel() override;
    [[nodiscard]] bool isConfigured() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("S3"); }

private:
    void onFinished();
    void onTestPutFinished();
    void onTestDeleteFinished();
    // The config this uploader works against: the explicit snapshot when it was
    // constructed with one, else the profile resolved fresh (secret included).
    [[nodiscard]] UploadConfig activeConfig() const;
    // One SigV4-signed request for one object key. Host + path follow the URL style and
    // feed the signer AND the URL, so the wire path always matches the signed path.
    [[nodiscard]] QNetworkRequest signedRequest(const UploadConfig &cfg, const QString &method,
                                                const QString &objectKey,
                                                const QString &contentType,
                                                const QString &payloadHash,
                                                QUrl *urlOut = nullptr) const;

    QString m_profileId;              // empty = default profile
    std::optional<UploadConfig> m_configOverride;   // set = ignore m_profileId entirely
    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFile *m_file = nullptr;          // streamed body; parented to the reply
    QUrl m_publicUrl;                 // computed at request time, emitted on success
    bool m_finished = false;          // guards single uploaded()/failed() emission
    QString m_probeKey;               // connection-test object, deleted once written
    bool m_tested = false;            // guards single-shot testConnection()
};

} // namespace Upload

#endif // UPLOAD_S3UPLOADER_H
