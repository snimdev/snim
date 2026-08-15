#include "upload/strategies/BlockingUploader.h"
#include "core/PostToGui.h"
#include "upload/Util.h"

#include <QBuffer>
#include <QFile>
#include <QPointer>
#include <QtConcurrentRun>

namespace Upload {

BlockingUploader::BlockingUploader(const UploadConfig &config, QObject *parent)
    : Uploader(parent), m_config(config)
{
}

BlockingUploader::~BlockingUploader()
{
    m_cancel->store(true);   // a worker still running stops at its next check
}

void BlockingUploader::upload(const QString &localPath, const QString &keyHint)
{
    start(localPath, keyHint, false);
}

void BlockingUploader::testConnection()
{
    start(QString(), QStringLiteral("snim-connection-test.txt"), true);
}

void BlockingUploader::start(const QString &localPath, const QString &keyHint, bool testing)
{
    if (m_started)
        return;                 // one uploader, one transfer, one terminal signal
    m_started = true;

    Job job;
    job.remoteName = Util::uniqueRemoteName(keyHint);
    job.remotePath = Util::buildRemotePath(m_config.remoteDir, job.remoteName);
    job.testing = testing;
    // An explicit base maps the whole remote tree, so it joins the full remote path - minus
    // the absolute marker, which would double the separator. Without one the subclass's
    // credential-free URL stands in, so isComplete() never has to require a base.
    const QString relative = job.remotePath.startsWith(QLatin1Char('/')) ? job.remotePath.mid(1)
                                                                          : job.remotePath;
    const QUrl publicUrl = m_config.publicBaseUrl.isEmpty()
                               ? fallbackUrl(job.remotePath)
                               : QUrl(Util::joinPublicUrl(m_config.publicBaseUrl, relative));

    // Fire and forget: the worker reports back through posts, so nobody holds the QFuture.
    // tr() is the static BlockingUploader::tr(), so it needs no capture either.
    (void) QtConcurrent::run([self = QPointer<BlockingUploader>(this), cancel = m_cancel,
                              run = transfer(), job, localPath, publicUrl] {
        QFile file(localPath);
        QBuffer probe;
        probe.setData(QByteArrayLiteral("Snim connection test"));
        QIODevice &body = job.testing ? static_cast<QIODevice &>(probe) : file;
        Outcome out;
        if (!body.open(QIODevice::ReadOnly))
            out.error = tr("Could not open the file to upload.");
        else
            out = run(body, job, *cancel);
        if (out.status == Outcome::Status::Cancelled)
            return;   // the uploader is gone, nobody to tell

        Core::postToGui([self, out, job, publicUrl] {
            // The follow-up belongs to the app, not to this uploader (an SFTP pin), so it
            // runs first and even when the uploader is already gone.
            if (out.status == Outcome::Status::Done && out.onDone)
                out.onDone();
            if (!self)
                return;
            if (out.status == Outcome::Status::Failed && job.testing)
                emit self->testFinished(false, out.error);
            else if (out.status == Outcome::Status::Failed)
                emit self->failed(out.error);
            else if (!job.testing)
                emit self->uploaded(publicUrl);
            else if (out.probeLeft)   // the write, the thing being tested, still worked
                emit self->testFinished(true, tr("Connected, but the test file %1 could not be "
                                                 "removed. Delete it manually.")
                                                  .arg(job.remotePath));
            else
                emit self->testFinished(true, tr("Connected: uploaded and removed a test file."));
        });
    });
}

} // namespace Upload
