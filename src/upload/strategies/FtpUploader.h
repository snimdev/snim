#ifndef UPLOAD_FTPUPLOADER_H
#define UPLOAD_FTPUPLOADER_H

#include "upload/Uploader.h"
#include "upload/UploadConfig.h"

#include <atomic>
#include <memory>
#include <optional>

namespace Upload {

/**
 * FTP / FTPS uploader built on libcurl's blocking easy API - Qt6 dropped FTP from
 * QNetworkAccessManager, so there is no Qt-native path. Compiled only where libcurl was
 * found (see CMakeLists); elsewhere the factory hands back a Stub that says so.
 *
 * upload() stays on the GUI thread just long enough to snapshot the config (the keychain
 * read must not leave the main thread) and pre-compute the remote name/URLs, then runs
 * the transfer on a QtConcurrent thread. The worker never touches this object: it holds
 * value copies plus a shared cancel flag, and every signal is marshaled back through
 * qApp with a QPointer null-check, so the uploader may be destroyed mid-transfer - which
 * is exactly what the owner does (deleteLater() from the terminal-signal handler).
 * Single-shot: one uploader, one upload(), exactly one uploaded()/failed().
 */
class FtpUploader : public Uploader
{
    Q_OBJECT

public:
    // profileId empty = the default profile (resolved at upload time).
    explicit FtpUploader(const QString &profileId = QString(), QObject *parent = nullptr);
    // Explicit config, used by UploaderFactory::createForConfig so the Settings dialog can
    // test values the user has typed but not saved yet: every lookup then reads this
    // snapshot instead of the profile store + keychain.
    explicit FtpUploader(const UploadConfig &config, QObject *parent = nullptr);
    ~FtpUploader() override;

    void upload(const QString &localPath, const QString &keyHint) override;
    void testConnection() override;
    void cancel() override;
    [[nodiscard]] bool isConfigured() const override;
    [[nodiscard]] QString name() const override { return QStringLiteral("FTP"); }

private:
    // The config this uploader works against: the explicit snapshot when it was
    // constructed with one, else the profile resolved fresh (secret included).
    [[nodiscard]] UploadConfig activeConfig() const;

    QString m_profileId;                          // empty = default profile
    std::optional<UploadConfig> m_configOverride; // set = ignore m_profileId entirely
    // Shared with the worker (which may outlive us): set by cancel() and the destructor,
    // polled from curl's read/progress callbacks.
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_started = false;                       // single-shot guard
};

} // namespace Upload

#endif // UPLOAD_FTPUPLOADER_H
