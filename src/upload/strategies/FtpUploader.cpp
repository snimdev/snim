#include "upload/strategies/FtpUploader.h"
#include "upload/UploadConfig.h"
#include "upload/UploadUtil.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QtConcurrentRun>

#include <curl/curl.h>

#include <mutex>
#include <utility>

namespace Upload {

namespace {

// curl_global_init() is not thread-safe, so it runs once from the constructor (GUI
// thread) before any worker exists. There is deliberately no curl_global_cleanup():
// curl stays initialized for the process lifetime - tearing it down while another
// upload is in flight would be far worse than leaking a one-time allocation.
std::once_flag g_curlInit;

// Every worker -> GUI hop goes through here. qApp is the context object, so the lambda
// runs on the GUI thread, where the QPointer check is race-free (the uploader is created
// and destroyed there and nowhere else). During shutdown qApp can already be gone - a
// still-running worker then has nobody to talk to, and dropping the post is correct.
template <typename F>
void postToGui(F &&fn)
{
    if (QCoreApplication *app = QCoreApplication::instance())
        QMetaObject::invokeMethod(app, std::forward<F>(fn), Qt::QueuedConnection);
}

void postFailed(const QPointer<FtpUploader> &self, const QString &message)
{
    postToGui([self, message] {
        if (self)
            emit self->failed(message);
    });
}

void postTestResult(const QPointer<FtpUploader> &self, bool ok, const QString &message)
{
    postToGui([self, ok, message] {
        if (self)
            emit self->testFinished(ok, message);
    });
}

struct ReadCtx {
    // A QFile for a real upload, an in-memory QBuffer for the connection-test probe.
    QIODevice *body = nullptr;
    std::shared_ptr<std::atomic_bool> cancel;
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

struct ProgressCtx {
    QPointer<FtpUploader> self;
    std::shared_ptr<std::atomic_bool> cancel;
    QElapsedTimer since;
    curl_off_t lastPosted = -1;
};

int xferInfoCallback(void *userdata, curl_off_t, curl_off_t, curl_off_t ultotal, curl_off_t ulnow)
{
    auto *ctx = static_cast<ProgressCtx *>(userdata);
    if (ctx->cancel->load())
        return 1;   // -> CURLE_ABORTED_BY_CALLBACK
    // curl ticks once per transfer loop; coalesce to ~10 updates/s (plus the final one)
    // so a large upload can't flood the GUI event queue with queued invocations.
    const bool done = ultotal > 0 && ulnow >= ultotal;
    if (ultotal > 0 && ulnow != ctx->lastPosted && (done || ctx->since.elapsed() >= 100)) {
        ctx->lastPosted = ulnow;
        ctx->since.restart();
        postToGui([self = ctx->self, ulnow, ultotal] {
            if (self)
                emit self->uploadProgress(static_cast<qint64>(ulnow), static_cast<qint64>(ultotal));
        });
    }
    return 0;
}

// One store-and-forward transfer, described by value so upload() and testConnection()
// drive the same option block instead of two copies that can drift apart.
struct Transfer {
    QString url;                 // credential-free ftp(s):// URL, path already encoded
    QString username;            // empty = curl's built-in anonymous login
    QString password;
    FtpEncryption encryption = FtpEncryption::Explicit;
    qint64 size = 0;
    QByteArray postQuote;        // raw command sent after the transfer (test: "DELE <path>")
};

// Runs the transfer on the calling (worker) thread. CURLE_FAILED_INIT means curl itself
// could not be set up; *responseCode gets the FTP reply code when the server sent one.
CURLcode performTransfer(const Transfer &t, ReadCtx *readCtx, ProgressCtx *progressCtx,
                         long *responseCode)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return CURLE_FAILED_INIT;

    const QByteArray urlUtf8 = t.url.toUtf8();
    curl_easy_setopt(curl, CURLOPT_URL, urlUtf8.constData());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, &readCallback);
    curl_easy_setopt(curl, CURLOPT_READDATA, readCtx);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(t.size));
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
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, progressCtx);

    // Only when named - an empty username means curl's built-in anonymous login.
    const QByteArray user = t.username.toUtf8();
    const QByteArray pass = t.password.toUtf8();
    if (!t.username.isEmpty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, user.constData());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, pass.constData());
    }

    // Explicit TLS = AUTH TLS negotiated on the plain port; require it for control
    // AND data (never CURLUSESSL_TRY) so a misconfigured server can't silently
    // downgrade the transfer to plaintext. Implicit TLS already rides the ftps://
    // scheme; asking for ALL there is redundant but states the same intent.
    if (t.encryption != FtpEncryption::None)
        curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));

    // POSTQUOTE runs on the same control connection once the data transfer is done, so
    // the connection test writes and removes its probe in a single session. A failing
    // command surfaces as CURLE_QUOTE_ERROR.
    curl_slist *quote = nullptr;
    if (!t.postQuote.isEmpty()) {
        quote = curl_slist_append(nullptr, t.postQuote.constData());
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

FtpUploader::FtpUploader(const QString &profileId, QObject *parent)
    : Uploader(parent), m_profileId(profileId),
      m_cancel(std::make_shared<std::atomic_bool>(false))
{
    std::call_once(g_curlInit, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

FtpUploader::FtpUploader(const UploadConfig &config, QObject *parent)
    : Uploader(parent), m_configOverride(config),
      m_cancel(std::make_shared<std::atomic_bool>(false))
{
    std::call_once(g_curlInit, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

FtpUploader::~FtpUploader()
{
    m_cancel->store(true);   // a worker still running stops at its next callback
}

void FtpUploader::cancel()
{
    m_cancel->store(true);
}

UploadConfig FtpUploader::activeConfig() const
{
    return m_configOverride ? *m_configOverride : UploadConfig::forProfile(m_profileId);
}

bool FtpUploader::isConfigured() const
{
    return activeConfig().isComplete();
}

void FtpUploader::upload(const QString &localPath, const QString &keyHint)
{
    if (m_started)
        return;                 // one uploader, one transfer, one terminal signal
    m_started = true;

    // Snapshot now, on the GUI thread: forProfile() reads the keychain, which must not
    // happen off the main thread, and the worker only ever gets value copies.
    const UploadConfig cfg = activeConfig();
    if (!cfg.isComplete()) {
        // Deferred, like StubUploader - callers connect their handlers after upload().
        QMetaObject::invokeMethod(this, [this] {
            emit failed(tr("Upload is not configured."));
        }, Qt::QueuedConnection);
        return;
    }
    {
        QFile probe(localPath);
        if (!probe.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(this, [this] {
                emit failed(tr("Could not open the file to upload."));
            }, Qt::QueuedConnection);
            return;
        }
    }

    const QString remoteName = Util::uniqueRemoteName(keyHint);
    const QString remotePath = Util::buildRemotePath(cfg.remoteDir, remoteName);
    const QString requestUrl = Util::buildFtpUrl(cfg, remotePath);
    // Public URL: an explicit base maps the whole remote tree, so it joins the full
    // remote path - minus the absolute marker, which would double the separator. Without
    // a base, fall back to the credential-free ftp(s):// URL (mirrors S3Uploader), so
    // isComplete() never has to require publicBaseUrl.
    const QUrl publicUrl =
        cfg.publicBaseUrl.isEmpty()
            ? QUrl(requestUrl)
            : QUrl(Util::joinPublicUrl(cfg.publicBaseUrl,
                                       remotePath.startsWith(QLatin1Char('/'))
                                           ? remotePath.mid(1)
                                           : remotePath));

    emit started();

    // The worker captures value copies only - never `this`. tr() below is the static
    // FtpUploader::tr(), so it needs no capture either. Fire and forget: the worker
    // reports back through the marshaled signals, so nobody holds the QFuture.
    (void) QtConcurrent::run([self = QPointer<FtpUploader>(this), cancel = m_cancel,
                              localPath, publicUrl, url = requestUrl,
                              username = cfg.username, password = cfg.secretKey,
                              encryption = cfg.ftpEncryption] {
        QFile file(localPath);
        if (!file.open(QIODevice::ReadOnly)) {
            postFailed(self, tr("Could not open the file to upload."));
            return;
        }

        ReadCtx readCtx{&file, cancel, false};
        ProgressCtx progressCtx{self, cancel, {}, -1};
        progressCtx.since.start();

        Transfer t;
        t.url = url;
        t.username = username;
        t.password = password;
        t.encryption = encryption;
        t.size = file.size();

        long response = 0;
        const CURLcode rc = performTransfer(t, &readCtx, &progressCtx, &response);

        if (rc == CURLE_OK) {
            postToGui([self, publicUrl] {
                if (self)
                    emit self->uploaded(publicUrl);
            });
            return;
        }
        if (readCtx.readError) {
            postFailed(self, tr("Upload failed: could not read the local file."));
            return;
        }
        if (rc == CURLE_ABORTED_BY_CALLBACK || cancel->load()) {
            postFailed(self, tr("Upload cancelled."));
            return;
        }
        if (rc == CURLE_FAILED_INIT) {
            postFailed(self, tr("Upload failed: could not initialize the FTP transport."));
            return;
        }
        postFailed(self, failureMessage(/*testing=*/false, rc, response, username));
    });
}

// Upload a tiny probe file and have the server delete it again in the same session: the
// only check that proves the login, the remote directory (created on demand, exactly as
// an upload would) and write permission all work.
void FtpUploader::testConnection()
{
    if (m_started)
        return;                 // one uploader, one transfer, one terminal signal
    m_started = true;

    // Snapshot now, on the GUI thread: forProfile() reads the keychain, which must not
    // happen off the main thread, and the worker only ever gets value copies.
    const UploadConfig cfg = activeConfig();
    if (!cfg.isComplete()) {
        // Deferred, like StubUploader - callers connect their handlers after the call.
        QMetaObject::invokeMethod(this, [this] {
            emit testFinished(false, tr("Upload is not configured."));
        }, Qt::QueuedConnection);
        return;
    }

    const QString remoteName = Util::uniqueRemoteName(QStringLiteral("niceshot-connection-test.txt"));
    const QString remotePath = Util::buildRemotePath(cfg.remoteDir, remoteName);
    const QString requestUrl = Util::buildFtpUrl(cfg, remotePath);

    // The worker captures value copies only - never `this`.
    (void) QtConcurrent::run([self = QPointer<FtpUploader>(this), cancel = m_cancel,
                              url = requestUrl, remotePath, remoteName,
                              username = cfg.username, password = cfg.secretKey,
                              encryption = cfg.ftpEncryption] {
        QBuffer body;
        body.setData(QByteArrayLiteral("Niceshot connection test"));
        body.open(QIODevice::ReadOnly);

        ReadCtx readCtx{&body, cancel, false};
        // A null `self` here on purpose: a test posts no upload progress, but the
        // callback still runs so the cancel flag is honored between ticks.
        ProgressCtx progressCtx{QPointer<FtpUploader>(), cancel, {}, -1};
        progressCtx.since.start();

        Transfer t;
        t.url = url;
        t.username = username;
        t.password = password;
        t.encryption = encryption;
        t.size = body.size();
        // DELE is a raw FTP command, so its argument is the DECODED path - never the
        // "%2F"-escaped form the URL needs - resolved against the server's CURRENT
        // directory. curl's default file method is MULTICWD: it CWDs into each directory
        // of the path and STORs the bare name, so by the time POSTQUOTE runs the server
        // already sits in the probe's own directory. The bare name is therefore the right
        // argument for an absolute and a login-relative remoteDir alike (the full
        // relative path would resolve one directory tree too deep).
        t.postQuote = QByteArrayLiteral("DELE ") + remoteName.toUtf8();

        long response = 0;
        const CURLcode rc = performTransfer(t, &readCtx, &progressCtx, &response);

        if (rc == CURLE_OK) {
            postTestResult(self, true, tr("Connected: uploaded and removed a test file."));
            return;
        }
        // The transfer itself - the thing being tested - worked and only the cleanup
        // command failed, so this is still a pass with a stray file to mention.
        if (rc == CURLE_QUOTE_ERROR) {
            postTestResult(self, true,
                           tr("Connected, but the test file %1 could not be removed. "
                              "Delete it manually.").arg(remotePath));
            return;
        }
        if (rc == CURLE_ABORTED_BY_CALLBACK || cancel->load()) {
            postTestResult(self, false, tr("Test cancelled."));
            return;
        }
        if (rc == CURLE_FAILED_INIT) {
            postTestResult(self, false, tr("Test failed: could not initialize the FTP transport."));
            return;
        }
        postTestResult(self, false, failureMessage(/*testing=*/true, rc, response, username));
    });
}

} // namespace Upload
