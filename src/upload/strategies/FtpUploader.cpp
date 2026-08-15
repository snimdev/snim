#include "upload/strategies/FtpUploader.h"
#include "upload/Util.h"

#include <QIODevice>

#include <curl/curl.h>

#include <mutex>

namespace Upload {

namespace {

// curl_global_init() is not thread-safe, so it runs once from the constructor (GUI
// thread) before any worker exists. There is deliberately no curl_global_cleanup():
// curl stays initialized for the process lifetime - tearing it down while another
// upload is in flight would be far worse than leaking a one-time allocation.
std::once_flag g_curlInit;

struct ReadCtx {
    // A QFile for a real upload, an in-memory QBuffer for the connection-test probe.
    QIODevice *body = nullptr;
    const std::atomic_bool *cancel = nullptr;
    bool readError = false;   // a local read failure, told apart from a cancel abort
};

size_t readCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    auto *ctx = static_cast<ReadCtx *>(userdata);
    if (ctx->cancel->load())
        return CURL_READFUNC_ABORT;
    const qint64 n = ctx->body->read(buffer, static_cast<qint64>(size * nitems));
    if (n < 0) {
        // Returning 0 would mean EOF and silently publish a truncated file.
        ctx->readError = true;
        return CURL_READFUNC_ABORT;
    }
    return static_cast<size_t>(n);
}

// Polled every transfer loop, so a cancel lands between reads too (1 = abort).
int xferInfoCallback(void *userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<const std::atomic_bool *>(userdata)->load() ? 1 : 0;
}

// One store of readCtx's body to url, run on the calling (worker) thread; postQuote is a
// raw command sent after it (test: "DELE <name>"). CURLE_FAILED_INIT means curl itself
// could not be set up; *responseCode gets the FTP reply code when the server sent one.
CURLcode performTransfer(const UploadConfig &cfg, const QString &url, ReadCtx *readCtx,
                         const QByteArray &postQuote, long *responseCode)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return CURLE_FAILED_INIT;

    const QByteArray urlUtf8 = url.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, urlUtf8.constData());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, &readCallback);
    curl_easy_setopt(curl, CURLOPT_READDATA, readCtx);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     static_cast<curl_off_t>(readCtx->body->size()));
    // Remote mkdir -p, but only after a CWD actually fails.
    curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS,
                     static_cast<long>(CURLFTP_CREATE_DIR_RETRY));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);   // mandatory off the main thread
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    // No total timeout (uploads can be big) - give up only on a dead transfer:
    // under 1 byte/s for 60 s, mirroring S3Uploader's idle timeout.
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &xferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, readCtx->cancel);

    // Only when named - an empty username means curl's built-in anonymous login.
    const QByteArray user = cfg.username.toUtf8();
    const QByteArray pass = cfg.secretKey.toUtf8();
    if (!cfg.username.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, user.constData());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, pass.constData());
    }

    // Explicit TLS = AUTH TLS negotiated on the plain port; require it for control
    // AND data (never CURLUSESSL_TRY) so a misconfigured server can't silently
    // downgrade the transfer to plaintext. Implicit TLS already rides the ftps://
    // scheme; asking for ALL there is redundant but states the same intent.
    if (cfg.ftpEncryption != FtpEncryption::None)
        curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));

    // POSTQUOTE runs on the same control connection once the data transfer is done, so
    // the connection test writes and removes its probe in a single session. A failing
    // command surfaces as CURLE_QUOTE_ERROR.
    curl_slist *quote = nullptr;
    if (!postQuote.isEmpty()) {
        quote = curl_slist_append(nullptr, postQuote.constData());
        curl_easy_setopt(curl, CURLOPT_POSTQUOTE, quote);
    }

    const CURLcode rc = curl_easy_perform(curl);
    if (responseCode)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, responseCode);
    curl_easy_cleanup(curl);
    if (quote)
        curl_slist_free_all(quote);
    return rc;
}

// One mapping of a failed transfer to an actionable message, worded for the phase it
// happened in, so an upload and a connection test explain the same causes the same way.
// curl_easy_strerror is generic; the FTP reply code (when the server got far enough to
// send one) is what usually pins down the cause. Never the password.
QString failureMessage(bool testing, CURLcode rc, long response, const QString &username)
{
    if (rc == CURLE_LOGIN_DENIED) {
        if (username.isEmpty()) {
            return testing ? FtpUploader::tr("Test failed: the server refused an anonymous login.")
                           : FtpUploader::tr("Upload failed: the server refused an anonymous login.");
        }
        return testing
            ? FtpUploader::tr("Test failed: the server rejected the login for %1.").arg(username)
            : FtpUploader::tr("Upload failed: the server rejected the login for %1.").arg(username);
    }
    const QString detail = QString::fromUtf8(curl_easy_strerror(rc));
    if (response > 0) {
        return testing ? FtpUploader::tr("Test failed: %1 (FTP %2).").arg(detail).arg(response)
                       : FtpUploader::tr("Upload failed: %1 (FTP %2).").arg(detail).arg(response);
    }
    return testing ? FtpUploader::tr("Test failed: %1.").arg(detail)
                   : FtpUploader::tr("Upload failed: %1.").arg(detail);
}

} // namespace

FtpUploader::FtpUploader(const UploadConfig &config, QObject *parent)
    : BlockingUploader(config, parent)
{
    std::call_once(g_curlInit, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

QUrl FtpUploader::fallbackUrl(const QString &remotePath) const
{
    return QUrl(Util::buildFtpUrl(m_config, remotePath));
}

BlockingUploader::Transfer FtpUploader::transfer() const
{
    // tr() below is the static FtpUploader::tr(), so the worker captures values only.
    return [cfg = m_config](QIODevice &body, const Job &job, const std::atomic_bool &cancel) {
        ReadCtx readCtx{&body, &cancel, false};
        // DELE is a raw FTP command, so its argument is the DECODED path - never the
        // "%2F"-escaped form the URL needs - resolved against the server's CURRENT
        // directory. curl's default file method is MULTICWD: it CWDs into each directory
        // of the path and STORs the bare name, so by the time POSTQUOTE runs the server
        // already sits in the probe's own directory. The bare name is therefore the right
        // argument for an absolute and a login-relative remoteDir alike (the full
        // relative path would resolve one directory tree too deep).
        const QByteArray postQuote =
            job.testing ? QByteArrayLiteral("DELE ") + job.remoteName.toUtf8() : QByteArray();
        long response = 0;
        const CURLcode rc = performTransfer(cfg, Util::buildFtpUrl(cfg, job.remotePath),
                                            &readCtx, postQuote, &response);

        Outcome out;
        if (rc == CURLE_OK || rc == CURLE_QUOTE_ERROR) {
            // A quote error is the probe's cleanup alone: the transfer itself worked.
            out.status = Outcome::Status::Done;
            out.probeLeft = rc == CURLE_QUOTE_ERROR;
        } else if (readCtx.readError) {
            out.error = tr("Upload failed: could not read the local file.");
        } else if (rc == CURLE_ABORTED_BY_CALLBACK || cancel.load()) {
            out.status = Outcome::Status::Cancelled;
        } else if (rc == CURLE_FAILED_INIT) {
            out.error = job.testing ? tr("Test failed: could not initialize the FTP transport.")
                                    : tr("Upload failed: could not initialize the FTP transport.");
        } else {
            out.error = failureMessage(job.testing, rc, response, cfg.username);
        }
        return out;
    };
}

} // namespace Upload
