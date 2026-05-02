#ifndef APP_UPLOADWORKFLOW_H
#define APP_UPLOADWORKFLOW_H

#include <QObject>
#include <QString>

namespace Upload {
class Uploader;
} // namespace Upload

namespace App {

class Notifier;

/**
 * Mediator for the upload flow: turns an editor's upload request into one Uploader,
 * reports progress through the Notifier and copies the link. It owns the in-flight
 * upload so it outlives the editor window that asked for it (the editor may close
 * mid-upload).
 */
class UploadWorkflow : public QObject
{
    Q_OBJECT

public:
    explicit UploadWorkflow(Notifier &notifier, QObject *parent = nullptr);
    ~UploadWorkflow() override;

    [[nodiscard]] bool isUploading() const { return m_uploader != nullptr; }

public slots:
    // One upload at a time; a second request is refused (and its temp file removed).
    void startUpload(const QString &localPath, const QString &suggestedName, bool deleteWhenDone,
                     const QString &profileId);

private:
    Notifier &m_notifier;
    Upload::Uploader *m_uploader = nullptr;   // current upload, parented to this
};

} // namespace App

#endif // APP_UPLOADWORKFLOW_H
