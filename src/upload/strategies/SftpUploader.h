#ifndef UPLOAD_SFTPUPLOADER_H
#define UPLOAD_SFTPUPLOADER_H

#include "upload/strategies/BlockingUploader.h"

namespace Upload {

/**
 * SFTP uploader built on libssh2's blocking API over a plain BSD socket - Qt has no
 * SFTP at all, and libssh2 wants a blocking descriptor it owns for the whole session,
 * which a detached QTcpSocket cannot promise. Compiled only where libssh2 was found
 * (see CMakeLists); elsewhere the factory hands back a Stub that says so.
 *
 * Host keys are verified before authentication, in three tiers: ~/.ssh/known_hosts, then
 * Snim's own pin store (Upload::KnownHosts), then trust-on-first-use - and a key that
 * *contradicts* either store is a hard failure, never a prompt-free overwrite. A test is
 * a whole exchange too, so it pins an unknown key on first use just like an upload.
 */
class SftpUploader : public BlockingUploader
{
    Q_OBJECT

public:
    // A complete config, secret included (see UploaderFactory).
    explicit SftpUploader(const UploadConfig &config, QObject *parent = nullptr);

protected:
    [[nodiscard]] Transfer transfer() const override;
    // A credential-free sftp:// pseudo-URL of the remote file.
    [[nodiscard]] QUrl fallbackUrl(const QString &remotePath) const override;
};

} // namespace Upload

#endif // UPLOAD_SFTPUPLOADER_H
