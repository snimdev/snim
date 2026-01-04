#include "upload/SigV4.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

#include <cstring>

namespace Upload::SigV4 {

QString awsUriEncode(const QString &value, bool encodeSlash)
{
    static const char *kUnreserved =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    const QByteArray utf8 = value.toUtf8();
    QString out;
    out.reserve(utf8.size() * 3);
    for (const char ch : utf8) {
        if (ch == '/' && !encodeSlash) {
            out += QLatin1Char('/');
        } else if (ch != '\0' && std::strchr(kUnreserved, ch) != nullptr) {
            out += QLatin1Char(ch);
        } else {
            out += QString::asprintf("%%%02X", static_cast<unsigned char>(ch));
        }
    }
    return out;
}

QByteArray hmacSha256(const QByteArray &key, const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

Result sign(const Request &req)
{
    const QDateTime utc = req.now.toUTC();
    const QString amzDate   = utc.toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
    const QString dateStamp = utc.toString(QStringLiteral("yyyyMMdd"));
    const QString payloadHash = req.payloadHash.isEmpty() ? unsignedPayload() : req.payloadHash;

    // Canonical request. Signed headers are a fixed, sorted, lowercased set.
    const QString canonicalUri = awsUriEncode(req.rawPath, /*encodeSlash=*/false);
    const QString signedHeaders = QStringLiteral("content-type;host;x-amz-content-sha256;x-amz-date");
    const QString canonicalHeaders =
        QStringLiteral("content-type:") + req.contentType + QLatin1Char('\n') +
        QStringLiteral("host:") + req.host + QLatin1Char('\n') +
        QStringLiteral("x-amz-content-sha256:") + payloadHash + QLatin1Char('\n') +
        QStringLiteral("x-amz-date:") + amzDate + QLatin1Char('\n');
    const QString canonicalRequest =
        req.method + QLatin1Char('\n') +
        canonicalUri + QLatin1Char('\n') +
        QString() /* canonical query string: none */ + QLatin1Char('\n') +
        canonicalHeaders + QLatin1Char('\n') +
        signedHeaders + QLatin1Char('\n') +
        payloadHash;

    // String to sign.
    const QString scope = dateStamp + QLatin1Char('/') + req.region + QLatin1Char('/') +
                          req.service + QStringLiteral("/aws4_request");
    const QString stringToSign =
        QStringLiteral("AWS4-HMAC-SHA256\n") + amzDate + QLatin1Char('\n') + scope + QLatin1Char('\n') +
        sha256Hex(canonicalRequest.toUtf8());

    // Derive the signing key: HMAC chain seeded with "AWS4" + secret.
    const QByteArray kDate    = hmacSha256(("AWS4" + req.secretKey).toUtf8(), dateStamp.toUtf8());
    const QByteArray kRegion  = hmacSha256(kDate, req.region.toUtf8());
    const QByteArray kService = hmacSha256(kRegion, req.service.toUtf8());
    const QByteArray kSigning = hmacSha256(kService, QByteArrayLiteral("aws4_request"));
    const QString signature = QString::fromLatin1(hmacSha256(kSigning, stringToSign.toUtf8()).toHex());

    Result r;
    r.amzDate = amzDate;
    r.payloadHash = payloadHash;
    r.authorization =
        QStringLiteral("AWS4-HMAC-SHA256 Credential=") + req.accessKeyId + QLatin1Char('/') + scope +
        QStringLiteral(", SignedHeaders=") + signedHeaders +
        QStringLiteral(", Signature=") + signature;
    return r;
}

} // namespace Upload::SigV4
