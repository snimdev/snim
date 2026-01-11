#ifndef UPLOAD_SFTPUPLOADER_H
#define UPLOAD_SFTPUPLOADER_H

#include "upload/Uploader.h"
#include "upload/UploadConfig.h"

#include <atomic>
#include <memory>
#include <optional>

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
 * Snim's own pin store (Upload::KnownHosts), then trust-on-first-use - and a key that
 * *contradicts* either store is a hard failure, never a prompt-free overwrite.
 */
class SftpUploader : public Uploader
{
    Q_OBJECT

public:
    // profileId empty = the default profile (resolved at upload time).
    explicit SftpUploader(const QString &profileId = QString(), QObject *parent = nullptr);
    // Explicit config, used by UploaderFactory::createForConfig so the Settings dialog can
    // test values the user has typed but not saved yet: every lookup then reads this
    // snapshot instead of the profile store + keychain. The host-key policy is unchanged -
    // a test pins on first use exactly like an upload does.
    explicit SftpUploader(const UploadConfig &config, QObject *parent = nullptr);
    ~SftpUploader() override;

    void upload(const QString &localPath, const QString &keyHint) override;
    void testConnection() override;
    void cancel() override;
    [[nodiscard]] bool isConfigured() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("SFTP"); }

private:
    // The config this uploader works against: the explicit snapshot when it was
    // constructed with one, else the profile resolved fresh (secret included).
    [[nodiscard]] UploadConfig activeConfig() const;

    QString m_profileId;                          // empty = default profile
    std::optional<UploadConfig> m_configOverride; // set = ignore m_profileId entirely
    // Shared with the worker (which may outlive us): set by cancel() and the destructor,
    // polled once per written chunk.
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_started = false;                       // single-shot guard
};

} // namespace Upload

#endif // UPLOAD_SFTPUPLOADER_H
