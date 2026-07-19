#include "core/SelfTest.h"
#include "core/BundledPaths.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QMediaFormat>
#include <QSslSocket>
#include <QStyle>
#include <QStyleFactory>

#ifdef Q_OS_LINUX
#include "core/SecretService.h"
#endif
#ifdef HAVE_TESSERACT
#include <tesseract/baseapi.h>
#endif
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif
#ifdef HAVE_LIBSSH2
#include <libssh2.h>
#endif

namespace Core::SelfTest {

namespace {

class Report
{
public:
    explicit Report(std::ostream &out) : m_out(out) {}

    void check(const QString &name, bool ok, const QString &detail)
    {
        line(ok ? "ok  " : "FAIL", name, detail);
        m_failed = m_failed || !ok;
    }
    void skip(const QString &name, const QString &why) { line("skip", name, why); }
    [[nodiscard]] bool failed() const { return m_failed; }

private:
    void line(const char *status, const QString &name, const QString &detail)
    {
        m_out << status << "  " << name.toStdString();
        if (!detail.isEmpty())
            m_out << ": " << detail.toStdString();
        m_out << '\n';
    }

    std::ostream &m_out;
    bool m_failed = false;
};

// Where OCRService looks for bundled language packs; empty when there is no such place.
QString bundledTessdataDir()
{
#ifdef Q_OS_MACOS
    return QDir::cleanPath(QCoreApplication::applicationDirPath()
                           + QStringLiteral("/../Resources/tessdata"));
#else
    return BundledPaths::forThisPlatform(QCoreApplication::applicationDirPath()).tessdataDir;
#endif
}

} // namespace

int run(std::ostream &out, const QList<Check> &extra)
{
    Report report(out);

    const QString platform = QGuiApplication::platformName();
#ifdef Q_OS_WIN
    const bool platformOk = platform == QLatin1String("windows")
                            || qEnvironmentVariableIsSet("QT_QPA_PLATFORM");
#else
    const bool platformOk = !platform.isEmpty();
#endif
    report.check(QStringLiteral("platform"), platformOk, platform);
#ifdef Q_OS_WIN
    report.check(QStringLiteral("style"),
                 QStyleFactory::keys().contains(QLatin1String("windowsvista"), Qt::CaseInsensitive),
                 QApplication::style()->name());
#endif

    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const char *format : { "png", "jpeg", "gif", "webp", "svg", "ico" })
        report.check(QStringLiteral("imageformat %1").arg(QLatin1String(format)),
                     formats.contains(format), QString());

    const QStringList tlsBackends = QSslSocket::availableBackends();
#ifdef Q_OS_WIN
    const bool tlsOk = tlsBackends.contains(QLatin1String("schannel")) && QSslSocket::supportsSsl();
#else
    const bool tlsOk = QSslSocket::supportsSsl();
#endif
    report.check(QStringLiteral("tls"), tlsOk, tlsBackends.join(QLatin1String(", ")));

    const auto decodable = QMediaFormat().supportedFileFormats(QMediaFormat::Decode);
    report.check(QStringLiteral("multimedia"), !decodable.isEmpty(),
                 QStringLiteral("%1 decodable formats").arg(decodable.size()));

    const QString tessdata = bundledTessdataDir();
    const bool tessdataBundled = !tessdata.isEmpty() && QDir(tessdata).exists();
    if (tessdataBundled)
        report.check(QStringLiteral("tessdata"),
                     QFile::exists(tessdata + QStringLiteral("/eng.traineddata")),
                     QDir::toNativeSeparators(tessdata));
    else
        report.skip(QStringLiteral("tessdata"), QStringLiteral("not bundled"));

#ifdef HAVE_TESSERACT
    const QString tesseractVersion = QStringLiteral("tesseract %1")
                                         .arg(QLatin1String(tesseract::TessBaseAPI::Version()));
    if (tessdataBundled || qEnvironmentVariableIsSet("TESSDATA_PREFIX")) {
        const QByteArray path = tessdataBundled ? QFile::encodeName(tessdata) : QByteArray();
        tesseract::TessBaseAPI api;
        const bool ok = api.Init(path.isEmpty() ? nullptr : path.constData(), "eng") == 0;
        api.End();
        report.check(QStringLiteral("ocr"), ok, tesseractVersion);
    } else {
        report.skip(QStringLiteral("ocr"), tesseractVersion + QStringLiteral(", no tessdata to load"));
    }
#else
    report.skip(QStringLiteral("ocr"), QStringLiteral("not compiled in"));
#endif

#ifdef HAVE_LIBCURL
    {
        const bool init = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
        bool ftp = false;
        for (const char *const *p = info->protocols; p && *p; ++p)
            ftp = ftp || qstrcmp(*p, "ftp") == 0;
        const QString tls = info->ssl_version ? QLatin1String(info->ssl_version)
                                              : QStringLiteral("no TLS");
        report.check(QStringLiteral("ftp"), init && ftp && info->ssl_version,
                     QStringLiteral("libcurl %1, %2").arg(QLatin1String(info->version), tls));
        if (init)
            curl_global_cleanup();
    }
#else
    report.skip(QStringLiteral("ftp"), QStringLiteral("not compiled in"));
#endif

#ifdef HAVE_LIBSSH2
    {
        const bool init = libssh2_init(0) == 0;
        LIBSSH2_SESSION *session = init ? libssh2_session_init() : nullptr;
        report.check(QStringLiteral("sftp"), session != nullptr,
                     QStringLiteral("libssh2 %1").arg(QLatin1String(libssh2_version(0))));
        if (session)
            libssh2_session_free(session);
        if (init)
            libssh2_exit();
    }
#else
    report.skip(QStringLiteral("sftp"), QStringLiteral("not compiled in"));
#endif

#ifdef Q_OS_LINUX
    {
        // A session only: no item is read or written and nothing prompts. CI has no keyring.
        QString detail;
        const KeychainStore::Failure keychain = SecretService::probe(&detail);
        if (keychain == KeychainStore::Failure::NoService)
            report.skip(QStringLiteral("keychain"), detail);
        else
            report.check(QStringLiteral("keychain"), keychain == KeychainStore::Failure::None, detail);
    }
#endif

    for (const Check &check : extra) {
        QString detail;
        const bool ok = check.run(&detail);
        report.check(check.name, ok, detail);
    }

    out << (report.failed() ? "self-test: FAILED" : "self-test: passed") << std::endl;
    return report.failed() ? 1 : 0;
}

} // namespace Core::SelfTest
