#include "upload/strategies/S3Uploader.h"
#include "upload/SigV4.h"
#include "upload/UploadUtil.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace Upload {

namespace {
// Pin a sensible content type so the public link previews in a browser instead of
// force-downloading (S3 defaults to application/octet-stream). Temp files often lack a
// usable extension, so map by the keyHint's extension; the SAME string is signed + sent.
QString contentTypeFor(const QString &keyHint)
{
    const QString ext = QFileInfo(keyHint).suffix().toLower();
    if (ext == "png")  return QStringLiteral("image/png");
    if (ext == "jpg" || ext == "jpeg") return QStringLiteral("image/jpeg");
    if (ext == "gif")  return QStringLiteral("image/gif");
    if (ext == "mp4" || ext == "mov")  return QStringLiteral("video/mp4");
    return QStringLiteral("application/octet-stream");
}
} // namespace

S3Uploader::S3Uploader(const QString &profileId, QObject *parent)
    : Uploader(parent), m_profileId(profileId), m_nam(new QNetworkAccessManager(this))
{
}

S3Uploader::~S3Uploader() = default;

bool S3Uploader::isConfigured() const
{
    return UploadConfig::forProfile(m_profileId).isComplete();
}

void S3Uploader::upload(const QString &localPath, const QString &keyHint)
{
    m_finished = false;
    const UploadConfig cfg = UploadConfig::forProfile(m_profileId);
    if (!cfg.isComplete()) {
        m_finished = true;
        emit failed(tr("Upload is not configured."));
        return;
    }

    auto *file = new QFile(localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        m_finished = true;
        emit failed(tr("Could not open the file to upload."));
        return;
    }

    // Object key: prefix + a short uuid (avoids collisions / overwrites) + safe name.
    const QString objectKey = cfg.keyPrefix + Util::uniqueRemoteName(keyHint);

    // Host + path differ by URL style; both feed the signer AND the request URL so the
    // wire path matches the signed path exactly.
    QString host, rawPath;
    if (cfg.forcePathStyle) {
        host = cfg.endpoint;
        rawPath = QLatin1Char('/') + cfg.bucket + QLatin1Char('/') + objectKey;
    } else {
        host = cfg.bucket + QLatin1Char('.') + cfg.endpoint;
        rawPath = QLatin1Char('/') + objectKey;
    }
    const QString contentType = contentTypeFor(keyHint);

    SigV4::Request sr;
    sr.method = QStringLiteral("PUT");
    sr.host = host;
    sr.rawPath = rawPath;
    sr.region = cfg.region;
    sr.service = QStringLiteral("s3");
    sr.accessKeyId = cfg.accessKeyId;
    sr.secretKey = cfg.secretKey;       // local-only; never logged or stored on the object
    sr.contentType = contentType;
    sr.payloadHash = SigV4::unsignedPayload();
    sr.now = QDateTime::currentDateTimeUtc();
    const SigV4::Result sig = SigV4::sign(sr);

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setAuthority(host);
    url.setPath(SigV4::awsUriEncode(rawPath, /*encodeSlash=*/false), QUrl::TolerantMode);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    req.setHeader(QNetworkRequest::ContentLengthHeader, file->size());
    req.setRawHeader("Host", host.toUtf8());
    req.setRawHeader("x-amz-date", sig.amzDate.toUtf8());
    req.setRawHeader("x-amz-content-sha256", sig.payloadHash.toUtf8());
    req.setRawHeader("Authorization", sig.authorization.toUtf8());
    req.setTransferTimeout(60'000);   // idle timeout (resets on activity) - survives slow uploads

    // Public URL: explicit base (required for R2's separate public host) else the
    // request URL (works for AWS/MinIO when the object is publicly readable).
    if (!cfg.publicBaseUrl.isEmpty()) {
        m_publicUrl = QUrl(Util::joinPublicUrl(cfg.publicBaseUrl, objectKey));
    } else {
        m_publicUrl = url;
    }

    m_file = file;
    m_reply = m_nam->put(req, file);
    file->setParent(m_reply);   // file outlives the async PUT, dies with the reply

    connect(m_reply, &QNetworkReply::uploadProgress, this, &Uploader::uploadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &S3Uploader::onFinished);
    emit started();
}

void S3Uploader::onFinished()
{
    if (m_finished || !m_reply)
        return;
    m_finished = true;

    const int http = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError err = m_reply->error();
    const QByteArray body = m_reply->readAll();
    m_reply->deleteLater();   // also frees the parented QFile

    if (err == QNetworkReply::NoError && http >= 200 && http < 300) {
        emit uploaded(m_publicUrl);
        return;
    }
    if (err == QNetworkReply::OperationCanceledError) {
        emit failed(tr("Upload cancelled."));
        return;
    }
    // Surface an actionable message; S3 returns an XML <Error><Code> body.
    if (body.contains("RequestTimeTooSkewed")) {
        emit failed(tr("Upload failed: your system clock is too far off. Fix the date/time and retry."));
        return;
    }
    QString code;
    const int s = body.indexOf("<Code>");
    const int e = body.indexOf("</Code>");
    if (s >= 0 && e > s)
        code = QString::fromUtf8(body.mid(s + 6, e - s - 6));
    emit failed(code.isEmpty()
                    ? tr("Upload failed (HTTP %1).").arg(http)
                    : tr("Upload failed: %1 (HTTP %2).").arg(code).arg(http));
}

void S3Uploader::cancel()
{
    if (m_reply)
        m_reply->abort();   // -> onFinished() with OperationCanceledError
}

} // namespace Upload
