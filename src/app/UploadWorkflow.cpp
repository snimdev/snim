#include "app/UploadWorkflow.h"
#include "app/Notifier.h"
#include "upload/Uploader.h"
#include "upload/UploaderFactory.h"

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>

namespace App {

    UploadWorkflow::UploadWorkflow(Notifier &notifier, QObject *parent)
        : QObject(parent)
          , m_notifier(notifier) {
    }

    UploadWorkflow::~UploadWorkflow() = default;

    void UploadWorkflow::startUpload(const QString &localPath, const QString &suggestedName,
                                     bool deleteWhenDone, const QString &profileId) {
        if (m_uploader) {   // one upload at a time
            m_notifier.notify(tr("Upload"), tr("An upload is already in progress."),
                              QSystemTrayIcon::Warning, 3000);
            if (deleteWhenDone)
                QFile::remove(localPath);
            return;
        }
        // Factory yields the configured S3 uploader for the chosen profile (empty =
        // default), or the stub if that profile isn't fully configured.
        m_uploader = Upload::UploaderFactory::create(
                         Upload::UploaderFactory::StrategyType::Auto, this, profileId).release();
        m_notifier.notify(tr("Uploading…"), QFileInfo(suggestedName).fileName(),
                          QSystemTrayIcon::Information, 2000);

        auto cleanup = [this, localPath, deleteWhenDone] {
            if (deleteWhenDone)
                QFile::remove(localPath);
            if (m_uploader) { m_uploader->deleteLater(); m_uploader = nullptr; }
        };
        connect(m_uploader, &Upload::Uploader::uploaded, this,
                [this, cleanup](const QUrl &url) {
                    QGuiApplication::clipboard()->setText(url.toString());
                    m_notifier.notify(tr("Uploaded - link copied"), url.toString(),
                                      QSystemTrayIcon::Information, 5000);
                    cleanup();
                });
        connect(m_uploader, &Upload::Uploader::failed, this,
                [this, cleanup](const QString &err) {
                    m_notifier.notify(tr("Upload failed"), err,
                                      QSystemTrayIcon::Warning, 6000);
                    cleanup();
                });
        m_uploader->upload(localPath, suggestedName);
    }

} // namespace App
