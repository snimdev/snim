#ifndef UPLOAD_SFTPUPLOADER_H
#define UPLOAD_SFTPUPLOADER_H

#include "upload/Uploader.h"

#include <atomic>
#include <memory>

namespace Upload {

/**
 * SFTP uploader built on libssh2's blocking API over a plain BSD socket - Qt has no
 * SFTP at all, and libssh2 wants a blocking descriptor it owns for the whole session,
 * which a detached QTcpSocket cannot promise. Compiled only where libssh2 was found
 * (see CMakeLists); elsewhere the factory hands back a Stub that says so.
 *
 * upload() stays on the GUI thread just long enough to snapshot the config (the keychain
 * read and the known-host pin lookup must not leave the main thread) and pre-compute the
 * remote path/URL, then runs the transfer on a QtConcurrent thread. The worker never
 * touches this object: it holds value copies plus a shared cancel flag, and every signal
 * is marshaled back through qApp with a QPointer null-check, so the uploader may be
 * destroyed mid-transfer - which is exactly what the owner does (deleteLater() from the
 * terminal-signal handler). Single-shot: one uploader, one upload(), exactly one
 * uploaded()/failed().
 *
 * Host keys are verified before authentication, in three tiers: ~/.ssh/known_hosts, then
 * Niceshot's own pin store (Upload::KnownHosts), then trust-on-first-use - and a key that
 * *contradicts* either store is a hard failure, never a prompt-free overwrite.
 */
class SftpUploader : public Uploader
{
    Q_OBJECT

public:
    // profileId empty = the default profile (resolved at upload time).
    explicit SftpUploader(const QString &profileId = QString(), QObject *parent = nullptr);
    ~SftpUploader() override;

    void upload(const QString &localPath, const QString &keyHint) override;
    void cancel() override;
    [[nodiscard]] bool isConfigured() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("SFTP"); }

private:
    QString m_profileId;                          // empty = default profile
    // Shared with the worker (which may outlive us): set by cancel() and the destructor,
    // polled once per written chunk.
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_started = false;                       // single-shot guard
};

} // namespace Upload

#endif // UPLOAD_SFTPUPLOADER_H
