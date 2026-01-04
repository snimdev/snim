#ifndef UPLOAD_SIGV4_H
#define UPLOAD_SIGV4_H

#include <QByteArray>
#include <QDateTime>
#include <QString>

// AWS Signature Version 4 for S3 (and S3-compatible: Cloudflare R2, MinIO, Wasabi).
// Pure functions - no QtNetwork, no I/O, no keychain - so the signing is unit-tested
// against fixed vectors. The caller injects the timestamp and sets the returned
// headers on its QNetworkRequest.
namespace Upload::SigV4 {

// Percent-encode per AWS canonical rules: unreserved set is A-Za-z0-9-_.~ (note `~`
// stays literal, unlike Qt's encoders); everything else -> %XX uppercase hex. With
// encodeSlash=false, '/' is preserved (used for the object-key path). The request URL
// MUST be built with the SAME encoding so the wire path matches the signed path.
[[nodiscard]] QString awsUriEncode(const QString &value, bool encodeSlash);

[[nodiscard]] QByteArray hmacSha256(const QByteArray &key, const QByteArray &data);
[[nodiscard]] QString sha256Hex(const QByteArray &data);

// The literal payload-hash for a streamed body (HTTPS protects the bytes; avoids a
// full pre-hash read pass). Pass this as `payloadHash` for a streamed PUT.
inline QString unsignedPayload() { return QStringLiteral("UNSIGNED-PAYLOAD"); }

struct Request {
    QString method;        // "PUT"
    QString host;          // signed Host header, e.g. "bucket.s3.amazonaws.com" or "endpoint"
    QString rawPath;       // unencoded, leading '/', e.g. "/prefix/My File.png" or "/bucket/key"
    QString region;        // "us-east-1", "auto" (R2), ...
    QString service;       // "s3"
    QString accessKeyId;
    QString secretKey;     // used only here, never stored/logged
    QString contentType;   // exact value also set as the Content-Type header
    QString payloadHash;   // unsignedPayload() or a real hex SHA256
    QDateTime now;         // converted to UTC internally
};

struct Result {
    QString authorization;   // -> "Authorization" header
    QString amzDate;         // -> "x-amz-date" header (yyyyMMdd'T'HHmmss'Z', UTC)
    QString payloadHash;     // -> "x-amz-content-sha256" header
};

[[nodiscard]] Result sign(const Request &req);

} // namespace Upload::SigV4

#endif // UPLOAD_SIGV4_H
