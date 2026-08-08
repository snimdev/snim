#ifndef UPLOAD_UPLOADER_H
#define UPLOAD_UPLOADER_H

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QUrl>

namespace Upload {

/**
 * Strategy interface for sending a local file to a remote and getting back a shareable
 * URL. S3 is the only concrete backend today; FTP/SFTP/custom-HTTP drop in as new
 * subclasses selected by UploaderFactory - mirroring CaptureStrategy / RecordingStrategy.
 *
 * Async, single-shot: upload() returns immediately; exactly one of uploaded()/failed()
 * fires later on this object's (GUI) thread. Lives on the GUI thread (QNetworkAccessManager
 * is not thread-safe) and is owned by App::UploadWorkflow so it can outlive the editor window.
 */
class Uploader : public QObject
{
    Q_OBJECT

public:
    explicit Uploader(QObject *parent = nullptr) : QObject(parent) {}

    // Upload localPath; keyHint is the desired remote filename (a unique prefix is added).
    virtual void upload(const QString &localPath, const QString &keyHint) = 0;
    [[nodiscard]] virtual bool isConfigured() const = 0;

    // Check the destination end to end: a backend that supports testing writes a tiny
    // probe file through the real transport and removes it again, which is the only
    // honest way to validate the credentials, the host key, the remote directory and
    // write permission. A tester is single-shot and separate from any upload use: build
    // one, test once, drop it. The default says so instead of pretending to succeed.
    virtual void testConnection()
    {
        QMetaObject::invokeMethod(this, [this] {
            emit testFinished(false, tr("Testing is not supported for this destination."));
        }, Qt::QueuedConnection);
    }

signals:
    void uploaded(const QUrl &publicUrl);
    void failed(const QString &error);
    // Exactly one emission per testConnection() call, always on this object's (GUI)
    // thread and never before the call returns, so a caller may connect afterwards.
    // ok=true can still carry a note - e.g. the probe was written but not removable.
    void testFinished(bool ok, const QString &message);
};

} // namespace Upload

#endif // UPLOAD_UPLOADER_H
