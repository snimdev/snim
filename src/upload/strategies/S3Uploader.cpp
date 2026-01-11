#include "upload/strategies/S3Uploader.h"
#include "upload/SigV4.h"
#include "upload/Util.h"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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

// One mapping of a failed S3 response to an actionable message, worded for the phase it
// happened in, so an upload and a connection test explain the same causes the same way.
// S3 answers errors with an XML <Error><Code> body.
QString failureMessage(bool testing, int http, const QByteArray &body)
{
    if (body.contains("RequestTimeTooSkewed")) {
        return testing
            ? S3Uploader::tr("Test failed: your system clock is too far off. Fix the date/time and retry.")
            : S3Uploader::tr("Upload failed: your system clock is too far off. Fix the date/time and retry.");
    }
    QString code;
    const int s = body.indexOf("<Code>");
    const int e = body.indexOf("</Code>");
    if (s >= 0 && e > s)
        code = QString::fromUtf8(body.mid(s + 6, e - s - 6));
    if (code.isEmpty()) {
        return testing ? S3Uploader::tr("Test failed (HTTP %1).").arg(http)
                       : S3Uploader::tr("Upload failed (HTTP %1).").arg(http);
    }
    return testing ? S3Uploader::tr("Test failed: %1 (HTTP %2).").arg(code).arg(http)
                   : S3Uploader::tr("Upload failed: %1 (HTTP %2).").arg(code).arg(http);
}
} // namespace

S3Uploader::S3Uploader(const QString &profileId, QObject *parent)
    : Uploader(parent), m_profileId(profileId), m_nam(new QNetworkAccessManager(this))
{
}

S3Uploader::S3Uploader(const UploadConfig &config, QObject *parent)
    : Uploader(parent), m_configOverride(config), m_nam(new QNetworkAccessManager(this))
{
}

S3Uploader::~S3Uploader() = default;

UploadConfig S3Uploader::activeConfig() const
{
    return m_configOverride ? *m_configOverride : UploadConfig::forProfile(m_profileId);
}

bool S3Uploader::isConfigured() const
{
    return activeConfig().isComplete();
}

QNetworkRequest S3Uploader::signedRequest(const UploadConfig &cfg, const QString &method,
                                          const QString &objectKey, const QString &contentType,
                                          const QString &payloadHash, QUrl *urlOut) const
{
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

    SigV4::Request sr;
    sr.method = method;
    sr.host = host;
    sr.rawPath = rawPath;
    sr.region = cfg.region;
    sr.service = QStringLiteral("s3");
    sr.accessKeyId = cfg.accessKeyId;
    sr.secretKey = cfg.secretKey;       // local-only; never logged or stored on the object
    sr.contentType = contentType;
    sr.payloadHash = payloadHash;
    sr.now = QDateTime::currentDateTimeUtc();
    const SigV4::Result sig = SigV4::sign(sr);

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setAuthority(host);
    url.setPath(SigV4::awsUriEncode(rawPath, /*encodeSlash=*/false), QUrl::TolerantMode);
    if (urlOut)
        *urlOut = url;

    QNetworkRequest req(url);
    // content-type is part of the signed header set, so it is always sent - even on a
    // DELETE, where it is meaningless but must still match what was signed.
    req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    req.setRawHeader("Host", host.toUtf8());
    req.setRawHeader("x-amz-date", sig.amzDate.toUtf8());
    req.setRawHeader("x-amz-content-sha256", sig.payloadHash.toUtf8());
    req.setRawHeader("Authorization", sig.authorization.toUtf8());
    req.setTransferTimeout(60'000);   // idle timeout (resets on activity) - survives slow uploads
    return req;
}

void S3Uploader::upload(const QString &localPath, const QString &keyHint)
{
    m_finished = false;
    const UploadConfig cfg = activeConfig();
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

    QUrl url;
    QNetworkRequest req = signedRequest(cfg, QStringLiteral("PUT"), objectKey,
                                        contentTypeFor(keyHint), SigV4::unsignedPayload(), &url);
    req.setHeader(QNetworkRequest::ContentLengthHeader, file->size());

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
    emit failed(failureMessage(/*testing=*/false, http, body));
}

// Write a small object with the real signing path, then delete it again: that is the only
// check that proves the credentials, the bucket, the prefix and the write permission all
// work. Single-shot, and never alongside an upload on the same object.
void S3Uploader::testConnection()
{
    if (m_tested || m_reply)
        return;
    m_tested = true;

    const UploadConfig cfg = activeConfig();
    if (!cfg.isComplete()) {
        // Deferred, like StubUploader - callers connect their handlers after the call.
        QMetaObject::invokeMethod(this, [this] {
            emit testFinished(false, tr("Upload is not configured."));
        }, Qt::QueuedConnection);
        return;
    }

    // Same prefix + unique-name rules as a real upload, so this also proves the key
    // prefix is writable rather than just the bucket root.
    m_probeKey = cfg.keyPrefix
                 + Util::uniqueRemoteName(QStringLiteral("niceshot-connection-test.txt"));
    const QByteArray body = QByteArrayLiteral("Niceshot connection test");

    QNetworkRequest req = signedRequest(cfg, QStringLiteral("PUT"), m_probeKey,
                                        QStringLiteral("text/plain"), SigV4::unsignedPayload());
    req.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

    // The body lives in memory (no temp file to clean up); it is parented to the reply,
    // exactly like the streamed QFile of an upload, so it dies with the request.
    auto *buffer = new QBuffer();
    buffer->setData(body);
    buffer->open(QIODevice::ReadOnly);
    m_reply = m_nam->put(req, buffer);
    buffer->setParent(m_reply);

    connect(m_reply, &QNetworkReply::finished, this, &S3Uploader::onTestPutFinished);
}

void S3Uploader::onTestPutFinished()
{
    if (!m_reply)
        return;
    const int http = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError err = m_reply->error();
    const QByteArray body = m_reply->readAll();
    const QString transportError = m_reply->errorString();
    m_reply->deleteLater();   // also frees the parented QBuffer

    if (err == QNetworkReply::OperationCanceledError) {
        emit testFinished(false, tr("Test cancelled."));
        return;
    }
    if (err != QNetworkReply::NoError && http == 0) {
        // Never reached an HTTP status: a wrong endpoint, DNS or TLS. "(HTTP 0)" would
        // say nothing, and Qt's own wording is the actionable part here. It carries the
        // URL but no credential - SigV4 travels in headers.
        emit testFinished(false, tr("Test failed: %1").arg(transportError));
        return;
    }
    if (err != QNetworkReply::NoError || http < 200 || http >= 300) {
        emit testFinished(false, failureMessage(/*testing=*/true, http, body));
        return;
    }

    // The write worked; clean up after ourselves with a signed DELETE (empty payload).
    const UploadConfig cfg = activeConfig();
    const QNetworkRequest req = signedRequest(cfg, QStringLiteral("DELETE"), m_probeKey,
                                              QStringLiteral("text/plain"),
                                              SigV4::sha256Hex(QByteArray()));
    m_reply = m_nam->deleteResource(req);
    connect(m_reply, &QNetworkReply::finished, this, &S3Uploader::onTestDeleteFinished);
}

void S3Uploader::onTestDeleteFinished()
{
    if (!m_reply)
        return;
    const int http = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError err = m_reply->error();
    m_reply->deleteLater();

    if (err == QNetworkReply::NoError && http >= 200 && http < 300) {
        emit testFinished(true, tr("Connected: uploaded and removed a test object."));
        return;
    }
    // The upload itself - the thing being tested - worked, so this is still a pass; the
    // user just has one stray object to know about.
    emit testFinished(true, tr("Connected, but the test object %1 could not be removed. "
                               "Delete it manually.").arg(m_probeKey));
}

void S3Uploader::cancel()
{
    if (m_reply)
        m_reply->abort();   // -> onFinished() with OperationCanceledError
}

} // namespace Upload
