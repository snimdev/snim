#include <QtTest>

#include "upload/SigV4.h"

using namespace Upload;

// AWS Signature V4 for S3. The expected Authorization strings below were computed by an
// INDEPENDENT Python implementation of the AWS algorithm (see plan), so a match proves
// the C++ signer agrees on the spec - across virtual-hosted (AWS), path-style (MinIO),
// and region="auto" (R2). Fixed AWS example credentials + a fixed UTC date make it
// deterministic; no network, no clock dependence.
class tst_SigV4 : public QObject
{
    Q_OBJECT

    static SigV4::Request baseReq()
    {
        SigV4::Request r;
        r.method = "PUT";
        r.region = "us-east-1";
        r.service = "s3";
        r.accessKeyId = "AKIAIOSFODNN7EXAMPLE";
        r.secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
        r.contentType = "image/png";
        r.payloadHash = SigV4::unsignedPayload();
        r.now = QDateTime(QDate(2013, 5, 24), QTime(0, 0, 0), QTimeZone::UTC);
        return r;
    }

private slots:
    void dateIsUtcUppercaseT()
    {
        SigV4::Request r = baseReq();
        r.host = "examplebucket.s3.amazonaws.com";
        r.rawPath = "/test.png";
        // Even if `now` is given in a non-UTC zone, the signed date must be the UTC instant.
        r.now = QDateTime(QDate(2013, 5, 24), QTime(2, 0, 0), QTimeZone(7200)); // +02:00 -> 00:00Z
        const SigV4::Result res = SigV4::sign(r);
        QCOMPARE(res.amzDate, QStringLiteral("20130524T000000Z"));   // uppercase T, Z, UTC
        QCOMPARE(res.payloadHash, QStringLiteral("UNSIGNED-PAYLOAD"));
    }

    void virtualHostedAws()
    {
        SigV4::Request r = baseReq();
        r.host = "examplebucket.s3.amazonaws.com";
        r.rawPath = "/test.png";
        QCOMPARE(SigV4::sign(r).authorization, QStringLiteral(
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request, "
            "SignedHeaders=content-type;host;x-amz-content-sha256;x-amz-date, "
            "Signature=1a28d698f8ea4b98332ee5a05a3e0a2e71853e76fdbd69323e49ece725dc059f"));
    }

    void pathStyleMinio()
    {
        SigV4::Request r = baseReq();
        r.host = "localhost:9000";
        r.rawPath = "/mybucket/test.png";
        QCOMPARE(SigV4::sign(r).authorization, QStringLiteral(
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request, "
            "SignedHeaders=content-type;host;x-amz-content-sha256;x-amz-date, "
            "Signature=0a4cf704eb2d6b3d774005b82fa0f32d12bbcb6fae45b3c21b8907344199c9e2"));
    }

    void pathStyleR2RegionAuto()
    {
        SigV4::Request r = baseReq();
        r.region = "auto";
        r.host = "acct.r2.cloudflarestorage.com";
        r.rawPath = "/mybucket/test.png";
        QCOMPARE(SigV4::sign(r).authorization, QStringLiteral(
            "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/auto/s3/aws4_request, "
            "SignedHeaders=content-type;host;x-amz-content-sha256;x-amz-date, "
            "Signature=7b0e356bd6f9a719363563dfc9c94d7629f7be738deb68a9b4be4a4b383e5fe5"));
    }

    void uriEncodingMatchesAwsRules()
    {
        // Unreserved set incl. '~' stays literal; space -> %20; '/' preserved unless asked.
        QCOMPARE(SigV4::awsUriEncode("a~b.c-d_e", false), QStringLiteral("a~b.c-d_e"));
        QCOMPARE(SigV4::awsUriEncode("my file.png", false), QStringLiteral("my%20file.png"));
        QCOMPARE(SigV4::awsUriEncode("/a/b c/d", false), QStringLiteral("/a/b%20c/d"));
        QCOMPARE(SigV4::awsUriEncode("/a/b", true), QStringLiteral("%2Fa%2Fb"));
        QCOMPARE(SigV4::awsUriEncode("a+b=c:d@e", false), QStringLiteral("a%2Bb%3Dc%3Ad%40e"));
        // UTF-8 multibyte percent-encodes each byte (é = C3 A9).
        QCOMPARE(SigV4::awsUriEncode(QString::fromUtf8("é"), false), QStringLiteral("%C3%A9"));
    }
};

QTEST_MAIN(tst_SigV4)
#include "tst_sigv4.moc"
