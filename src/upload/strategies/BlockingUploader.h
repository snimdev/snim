#ifndef UPLOAD_BLOCKINGUPLOADER_H
#define UPLOAD_BLOCKINGUPLOADER_H

#include "upload/Uploader.h"
#include "upload/UploadConfig.h"

#include <atomic>
#include <functional>
#include <memory>

class QIODevice;

namespace Upload {

/**
 * Base for the uploaders whose transport blocks (libcurl, libssh2): a Template Method
 * over upload() and testConnection(). The base names the remote file, runs the
 * subclass's transfer on a QtConcurrent thread with the local file (or, for a test, an
 * in-memory probe) as the body, and turns the outcome into the one terminal signal.
 *
 * The worker never touches this object: it holds value copies plus a shared cancel flag
 * (set by the destructor), and every signal is marshaled back through qApp with a
 * QPointer check, so the uploader may be destroyed mid-transfer - which is exactly what
 * the owner does (deleteLater() from the terminal-signal handler). Single-shot: one
 * uploader, one call, exactly one terminal signal.
 */
class BlockingUploader : public Uploader
{
    Q_OBJECT

public:
    ~BlockingUploader() override;

    void upload(const QString &localPath, const QString &keyHint) final;
    // Writes a tiny probe through the same transfer and removes it again: the only check
    // that proves the login, the remote directory (created on demand, exactly as an
    // upload would) and write permission all work.
    void testConnection() final;

protected:
    BlockingUploader(const UploadConfig &config, QObject *parent);

    struct Job {
        QString remoteName;      // unique name, see Util::uniqueRemoteName
        QString remotePath;      // remoteDir + remoteName
        bool testing = false;    // the connection probe: remove it once written
    };
    struct Outcome {
        enum class Status { Done, Failed, Cancelled } status = Status::Failed;
        QString error;                  // Failed: what to tell the user
        bool probeLeft = false;         // Done while testing: written but not removable
        std::function<void()> onDone;   // GUI thread, runs even if the uploader is gone
    };
    // Runs on a pool thread and may only use what it captured by value.
    using Transfer = std::function<Outcome(QIODevice &body, const Job &job,
                                           const std::atomic_bool &cancel)>;

    // GUI thread: the work for this call, ready to leave the thread.
    [[nodiscard]] virtual Transfer transfer() const = 0;
    // The link to hand back when no public base URL is set.
    [[nodiscard]] virtual QUrl fallbackUrl(const QString &remotePath) const = 0;

    const UploadConfig m_config;

private:
    void start(const QString &localPath, const QString &keyHint, bool testing);

    std::shared_ptr<std::atomic_bool> m_cancel = std::make_shared<std::atomic_bool>(false);
    bool m_started = false;
};

} // namespace Upload

#endif // UPLOAD_BLOCKINGUPLOADER_H
