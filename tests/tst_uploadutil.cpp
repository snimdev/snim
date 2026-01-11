#include <QtTest>
#include <QRegularExpression>
#include <QStandardPaths>

#include "upload/Util.h"
#include "upload/UploadConfig.h"

using namespace Upload;

// The shared remote-name / path / URL helpers. Pure string work - no network, no
// keychain, no optional dependency - so every assertion here holds in every build
// (with or without HAVE_LIBCURL / HAVE_LIBSSH2). The UploadConfig instances are
// hand-built; forProfile() (and therefore the keychain) is never called.
class tst_UploadUtil : public QObject
{
    Q_OBJECT

    // A credential-bearing FTP config, so the URL builders can be checked to leak nothing.
    static UploadConfig ftpConfig(const QString &host, int port, FtpEncryption enc)
    {
        UploadConfig c;
        c.enabled = true;
        c.type = ProviderType::Ftp;
        c.host = host;
        c.port = port;
        c.ftpEncryption = enc;
        c.username = "bob";
        c.secretKey = "hunter2";
        return c;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("NiceshotTest");
        QCoreApplication::setApplicationName("tst_uploadutil");
        QStandardPaths::setTestModeEnabled(true);
    }

    // --- sanitizeHint -------------------------------------------------------

    void sanitizeHintKeepsOnlyTheFileName()
    {
        QCOMPARE(Util::sanitizeHint("shot.png"), QStringLiteral("shot.png"));
        QCOMPARE(Util::sanitizeHint("a/b/c.png"), QStringLiteral("c.png"));
        QCOMPARE(Util::sanitizeHint("/abs/path/My File.png"), QStringLiteral("My File.png"));
        // A traversal hint can never escape the remote prefix: only the leaf survives.
        QCOMPARE(Util::sanitizeHint("../../etc/passwd"), QStringLiteral("passwd"));
    }

    void sanitizeHintFallsBackForNamelessHints()
    {
        QCOMPARE(Util::sanitizeHint(QString()), QStringLiteral("upload.bin"));
        QCOMPARE(Util::sanitizeHint(""), QStringLiteral("upload.bin"));
        QCOMPARE(Util::sanitizeHint("/"), QStringLiteral("upload.bin"));
        QCOMPARE(Util::sanitizeHint("dir/"), QStringLiteral("upload.bin"));
    }

    // --- uniqueRemoteName ---------------------------------------------------

    void uniqueRemoteNameHasUuidPrefixAndSafeHint()
    {
        const QString n = Util::uniqueRemoteName("shot.png");
        // 8 lowercase hex chars (QUuid Id128 prefix) + '-' + the sanitized hint.
        QVERIFY2(QRegularExpression(QStringLiteral("^[0-9a-f]{8}-shot\\.png$"))
                     .match(n).hasMatch(),
                 qPrintable(n));
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{8}-passwd$"))
                    .match(Util::uniqueRemoteName("../../etc/passwd")).hasMatch());
        QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{8}-upload\\.bin$"))
                    .match(Util::uniqueRemoteName(QString())).hasMatch());
    }

    void uniqueRemoteNameDoesNotRepeat()
    {
        // The whole point of the prefix: two uploads of the same file never collide.
        QVERIFY(Util::uniqueRemoteName("shot.png") != Util::uniqueRemoteName("shot.png"));
    }

    // --- buildRemotePath ----------------------------------------------------

    void buildRemotePathSlashMatrix()
    {
        // Empty dir -> the bare name (relative to the login directory).
        QCOMPARE(Util::buildRemotePath("", "f.png"), QStringLiteral("f.png"));
        QCOMPARE(Util::buildRemotePath(QString(), "f.png"), QStringLiteral("f.png"));
        // Trailing slash collapses instead of doubling.
        QCOMPARE(Util::buildRemotePath("uploads/", "f.png"), QStringLiteral("uploads/f.png"));
        QCOMPARE(Util::buildRemotePath("uploads", "f.png"), QStringLiteral("uploads/f.png"));
        QCOMPARE(Util::buildRemotePath("a/b", "f.png"), QStringLiteral("a/b/f.png"));
        // A leading '/' means absolute and survives exactly once; inner doubles collapse.
        QCOMPARE(Util::buildRemotePath("/var//www/", "f.png"), QStringLiteral("/var/www/f.png"));
        QCOMPARE(Util::buildRemotePath("/", "f.png"), QStringLiteral("/f.png"));
        QCOMPARE(Util::buildRemotePath("//var///www//", "f.png"), QStringLiteral("/var/www/f.png"));
    }

    // --- joinPublicUrl ------------------------------------------------------

    void joinPublicUrlChopsTrailingSlashesAndEncodes()
    {
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com/", "a b.png"),
                 QStringLiteral("https://cdn.example.com/a%20b.png"));
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com", "a b.png"),
                 QStringLiteral("https://cdn.example.com/a%20b.png"));
        // Every trailing slash goes, not just one.
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com///", "x.png"),
                 QStringLiteral("https://cdn.example.com/x.png"));
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com/pub/", "x.png"),
                 QStringLiteral("https://cdn.example.com/pub/x.png"));
    }

    void joinPublicUrlIsByteCompatibleWithTheS3ObjectKey()
    {
        // The same AWS encoder the signed request URL uses: '/' stays literal so a
        // prefixed key keeps its shape, '~' is unreserved, non-ASCII is UTF-8 %XX
        // (uppercase hex). Changing any of this would break existing S3 public links.
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com", "screenshots/a b.png"),
                 QStringLiteral("https://cdn.example.com/screenshots/a%20b.png"));
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com", "a~b.png"),
                 QStringLiteral("https://cdn.example.com/a~b.png"));
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com", QString::fromUtf8("ä.png")),
                 QStringLiteral("https://cdn.example.com/%C3%A4.png"));
        QCOMPARE(Util::joinPublicUrl("https://cdn.example.com", "a+b.png"),
                 QStringLiteral("https://cdn.example.com/a%2Bb.png"));
    }

    // --- buildFtpUrl --------------------------------------------------------

    void buildFtpUrlSchemePerEncryption()
    {
        // Explicit TLS is negotiated in-band (AUTH TLS), so the scheme stays ftp://;
        // only implicit TLS gets its own scheme.
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 0, FtpEncryption::None), "f.png"),
                 QStringLiteral("ftp://h/f.png"));
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 0, FtpEncryption::Explicit), "f.png"),
                 QStringLiteral("ftp://h/f.png"));
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 0, FtpEncryption::Implicit), "f.png"),
                 QStringLiteral("ftps://h/f.png"));
    }

    void buildFtpUrlPortOnlyWhenSet()
    {
        // port 0 = "protocol default" and must not appear in the URL at all.
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 0, FtpEncryption::Explicit), "f.png"),
                 QStringLiteral("ftp://h/f.png"));
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 2121, FtpEncryption::Explicit), "f.png"),
                 QStringLiteral("ftp://h:2121/f.png"));
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 990, FtpEncryption::Implicit), "f.png"),
                 QStringLiteral("ftps://h:990/f.png"));
    }

    void buildFtpUrlEncodesSegmentsAndMarksAbsolutePaths()
    {
        // Each segment is percent-encoded (curl's parser rejects raw spaces) while the
        // separators stay literal; a leading '/' becomes "%2F" because curl reads an FTP
        // path as login-dir-relative.
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 2121, FtpEncryption::Explicit),
                                   "/var/www files/x y.png"),
                 QStringLiteral("ftp://h:2121/%2Fvar/www%20files/x%20y.png"));
        // Relative paths keep no marker.
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 0, FtpEncryption::Explicit),
                                   "up loads/x y.png"),
                 QStringLiteral("ftp://h/up%20loads/x%20y.png"));
        // Doubled separators do not survive into the URL.
        QCOMPARE(Util::buildFtpUrl(ftpConfig("h", 0, FtpEncryption::Explicit), "//a//b.png"),
                 QStringLiteral("ftp://h/%2Fa/b.png"));
    }

    void buildFtpUrlNeverCarriesCredentials()
    {
        // Credentials go to CURLOPT_USERNAME/PASSWORD; a URL may be logged or shown to
        // the user as the fallback link, so it must stay clean.
        const UploadConfig cfg = ftpConfig("h", 21, FtpEncryption::Explicit);
        const QString url = Util::buildFtpUrl(cfg, "/pub/x.png");
        QVERIFY(!url.contains("bob"));
        QVERIFY(!url.contains("hunter2"));
        QVERIFY(!url.contains(QLatin1Char('@')));
        QCOMPARE(url, QStringLiteral("ftp://h:21/%2Fpub/x.png"));
    }
};

QTEST_MAIN(tst_UploadUtil)
#include "tst_uploadutil.moc"
