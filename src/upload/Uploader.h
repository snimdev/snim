#ifndef UPLOAD_UPLOADER_H
#define UPLOAD_UPLOADER_H

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
 * is not thread-safe) and is owned by ScreenshotApp so it can outlive the editor window.
 */
class Uploader : public QObject
{
    Q_OBJECT

public:
    explicit Uploader(QObject *parent = nullptr) : QObject(parent) {}

    // Upload localPath; keyHint is the desired remote filename (a unique prefix is added).
    virtual void upload(const QString &localPath, const QString &keyHint) = 0;
    virtual void cancel() {}
    [[nodiscard]] virtual bool isConfigured() const = 0;
    [[nodiscard]] virtual QString name() const = 0;

signals:
    void started();
    void uploadProgress(qint64 sent, qint64 total);
    void uploaded(const QUrl &publicUrl);
    void failed(const QString &error);
};

} // namespace Upload

#endif // UPLOAD_UPLOADER_H
