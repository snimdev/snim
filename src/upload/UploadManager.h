#ifndef UPLOAD_UPLOADMANAGER_H
#define UPLOAD_UPLOADMANAGER_H

#include <QObject>
#include <QPixmap>

namespace Upload {

// Forward declarations for future upload destinations
class FTPUploader;
class CloudinaryUploader;
class UploadDestination;

/**
 * @brief Upload manager for handling screenshot uploads to various destinations
 *
 * This class will manage different upload destinations such as:
 * - FTP servers
 * - Cloudinary
 * - Custom cloud storage providers
 * - Local network shares
 */
class UploadManager : public QObject
{
    Q_OBJECT

public:
    explicit UploadManager(QObject *parent = nullptr);

    // Future methods for upload functionality
    // void addDestination(UploadDestination *destination);
    // void uploadScreenshot(const QPixmap &screenshot, const QString &filename);
    // void configureDestination(const QString &destinationType);

signals:
    // Future signals for upload progress and completion
    // void uploadStarted(const QString &filename);
    // void uploadProgress(int percentage);
    // void uploadCompleted(const QString &url);
    // void uploadFailed(const QString &error);

private:
    // Future member variables for upload destinations
    // QList<UploadDestination*> m_destinations;
    // QString m_defaultDestination;
};

} // namespace Upload

#endif // UPLOAD_UPLOADMANAGER_H
