#ifndef UPLOAD_FTPUPLOADER_H
#define UPLOAD_FTPUPLOADER_H

#include "upload/strategies/BlockingUploader.h"

namespace Upload {

/**
 * FTP / FTPS uploader built on libcurl's blocking easy API - Qt6 dropped FTP from
 * QNetworkAccessManager, so there is no Qt-native path. Compiled only where libcurl was
 * found (see CMakeLists); elsewhere the factory hands back a Stub that says so.
 */
class FtpUploader : public BlockingUploader
{
    Q_OBJECT

public:
    // A complete config, secret included (see UploaderFactory).
    explicit FtpUploader(const UploadConfig &config, QObject *parent = nullptr);

protected:
    [[nodiscard]] Transfer transfer() const override;
    // The credential-free ftp(s):// request URL.
    [[nodiscard]] QUrl fallbackUrl(const QString &remotePath) const override;
};

} // namespace Upload

#endif // UPLOAD_FTPUPLOADER_H
