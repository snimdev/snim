#include "upload/strategies/FtpUploader.h"
#include "upload/UploadConfig.h"
#include "upload/UploadUtil.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
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

struct ReadCtx {
    QFile *file = nullptr;
    std::shared_ptr<std::atomic_bool> cancel;
    bool readError = false;   // a local read failure, told apart from a cancel abort
};

size_t readCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    auto *ctx = static_cast<ReadCtx *>(userdata);
    if (ctx->cancel->load())
        return CURL_READFUNC_ABORT;
    const qint64 n = ctx->file->read(buffer, static_cast<qint64>(size * nitems));
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

} // namespace

FtpUploader::FtpUploader(const QString &profileId, QObject *parent)
    : Uploader(parent), m_profileId(profileId),
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

bool FtpUploader::isConfigured() const
{
    return UploadConfig::forProfile(m_profileId).isComplete();
}

void FtpUploader::upload(const QString &localPath, const QString &keyHint)
{
    if (m_started)
        return;                 // one uploader, one transfer, one terminal signal
    m_started = true;

    // Snapshot now, on the GUI thread: forProfile() reads the keychain, which must not
    // happen off the main thread, and the worker only ever gets value copies.
    const UploadConfig cfg = UploadConfig::forProfile(m_profileId);
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
        CURL *curl = curl_easy_init();
        if (!curl) {
            postFailed(self, tr("Upload failed: could not initialize the FTP transport."));
            return;
        }

        ReadCtx readCtx{&file, cancel, false};
        ProgressCtx progressCtx{self, cancel, {}, -1};
        progressCtx.since.start();

        const QByteArray urlUtf8 = url.toUtf8();
        curl_easy_setopt(curl, CURLOPT_URL, urlUtf8.constData());
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, &readCallback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &readCtx);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(file.size()));
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
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressCtx);

        // Only when named - an empty username means curl's built-in anonymous login.
        const QByteArray user = username.toUtf8();
        const QByteArray pass = password.toUtf8();
        if (!username.isEmpty()) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, user.constData());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, pass.constData());
        }

        // Explicit TLS = AUTH TLS negotiated on the plain port; require it for control
        // AND data (never CURLUSESSL_TRY) so a misconfigured server can't silently
        // downgrade the transfer to plaintext. Implicit TLS already rides the ftps://
        // scheme; asking for ALL there is redundant but states the same intent.
        if (encryption != FtpEncryption::None)
            curl_easy_setopt(curl, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));

        const CURLcode rc = curl_easy_perform(curl);
        long response = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
        curl_easy_cleanup(curl);

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
        if (rc == CURLE_LOGIN_DENIED) {
            postFailed(self, username.isEmpty()
                                 ? tr("Upload failed: the server refused an anonymous login.")
                                 : tr("Upload failed: the server rejected the login for %1.")
                                       .arg(username));
            return;
        }
        // curl_easy_strerror is generic; the FTP reply code (when the server got far
        // enough to send one) is what usually pins down the cause. Never the password.
        const QString detail = QString::fromUtf8(curl_easy_strerror(rc));
        postFailed(self, response > 0 ? tr("Upload failed: %1 (FTP %2).").arg(detail).arg(response)
                                      : tr("Upload failed: %1.").arg(detail));
    });
}

} // namespace Upload
