#include "upload/strategies/SftpUploader.h"
#include "upload/KnownHosts.h"
#include "upload/SocketShim.h"
#include "upload/UploadConfig.h"
#include "upload/Util.h"

#include <QDir>
#include <QFile>
#include <QUrl>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <mutex>
#include <optional>

namespace Upload {

namespace {

// libssh2_init() is not thread-safe, so it runs once from the constructor (GUI thread)
// before any worker exists. There is deliberately no libssh2_exit(): libssh2 stays
// initialized for the process lifetime - tearing it down while another upload is in
// flight would be far worse than leaking a one-time allocation.
std::once_flag g_ssh2Init;

// libssh2's own description of the last failure - far more useful than the numeric code.
// The buffer belongs to the session (want_buf = 0), so it is copied, never freed here.
// Never contains the password or passphrase: libssh2 does not echo credentials.
QString sessionError(LIBSSH2_SESSION *session)
{
    char *msg = nullptr;
    int len = 0;
    libssh2_session_last_error(session, &msg, &len, 0);
    return (msg && len > 0) ? QString::fromUtf8(msg, len) : QString();
}

// Blocking connect to the first address that answers. Returns the socket, or Invalid with
// *detail filled in. IPv4/IPv6 agnostic via AF_UNSPEC, which is why this loops over addrinfo.
SocketShim::Handle connectSocket(const QString &host, int port, QString *detail)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;        // whichever of A / AAAA resolves and answers
    hints.ai_socktype = SOCK_STREAM;

    const QByteArray hostUtf8 = host.toUtf8();
    const QByteArray portStr = QByteArray::number(port);
    addrinfo *res = nullptr;
    const int rc = SocketShim::resolve(hostUtf8.constData(), portStr.constData(), &hints, &res);
    if (rc != 0) {
        *detail = SocketShim::resolveError(rc);
        return SocketShim::Invalid;
    }

    SocketShim::Handle fd = SocketShim::Invalid;
    for (addrinfo *ai = res; ai; ai = ai->ai_next) {
        fd = SocketShim::open(ai);
        if (fd == SocketShim::Invalid)
            continue;
        if (SocketShim::connect(fd, ai))
            break;
        *detail = SocketShim::lastError();
        SocketShim::close(fd);
        fd = SocketShim::Invalid;
    }
    SocketShim::freeResolved(res);

    if (fd != SocketShim::Invalid) {
        // The worker has no event loop, so a peer that stops answering would otherwise
        // hang this thread forever: bound every blocking read/write at 30 s. (connect()
        // itself is not covered - it falls back to the OS TCP connect timeout.)
        SocketShim::setIoTimeout(fd, 30);
    }
    return fd;
}

// The server's key in OpenSSH presentation format ("SHA256:<base64, no padding>"), so a
// user can compare it byte for byte with `ssh-keyscan` / the ssh client's own prompt.
QString hostKeyFingerprint(LIBSSH2_SESSION *session)
{
    const char *hash = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (!hash)
        return QString();
    return QStringLiteral("SHA256:")
           + QString::fromLatin1(
               QByteArray(hash, 32).toBase64(QByteArray::Base64Encoding
                                             | QByteArray::OmitTrailingEquals));
}

enum class HostKeyVerdict { Match, Mismatch, Unknown };

// Tier 1 of the host-key policy: what ~/.ssh/known_hosts has to say. A missing or
// unreadable file is simply "no entries" (Unknown), not an error - plenty of machines
// have never run ssh. `hosts` is owned by the caller (it must outlive the session).
HostKeyVerdict checkSystemKnownHosts(LIBSSH2_KNOWNHOSTS *hosts, const QString &host, int port,
                                     const char *key, size_t keyLen)
{
    if (!hosts)
        return HostKeyVerdict::Unknown;

    const QByteArray file = QFile::encodeName(QDir::homePath() + QStringLiteral("/.ssh/known_hosts"));
    libssh2_knownhost_readfile(hosts, file.constData(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    const QByteArray hostUtf8 = host.toUtf8();
    // checkp (not check) so the [host]:port form of a non-22 entry is matched too. The
    // key arrives raw from the handshake, hence KEYENC_RAW.
    const int rc = libssh2_knownhost_checkp(hosts, hostUtf8.constData(), port, key, keyLen,
                                            LIBSSH2_KNOWNHOST_TYPE_PLAIN
                                                | LIBSSH2_KNOWNHOST_KEYENC_RAW,
                                            nullptr);
    switch (rc) {
    case LIBSSH2_KNOWNHOST_CHECK_MATCH:    return HostKeyVerdict::Match;
    case LIBSSH2_KNOWNHOST_CHECK_MISMATCH: return HostKeyVerdict::Mismatch;
    default: break;   // NOTFOUND / FAILURE -> ask the app pin store next
    }
    return HostKeyVerdict::Unknown;
}

// Everything the worker allocates, torn down in reverse order by the destructor so the
// many early returns below cannot leak a socket or a session. knownhosts is released
// before the session it was created from, since it borrows that session for errors.
struct SshResources {
    SocketShim::Handle fd = SocketShim::Invalid;
    LIBSSH2_SESSION *session = nullptr;
    LIBSSH2_KNOWNHOSTS *hosts = nullptr;
    LIBSSH2_SFTP *sftp = nullptr;
    LIBSSH2_SFTP_HANDLE *handle = nullptr;

    SshResources() = default;
    SshResources(const SshResources &) = delete;
    SshResources &operator=(const SshResources &) = delete;

    ~SshResources()
    {
        if (handle)
            libssh2_sftp_close(handle);
        if (sftp)
            libssh2_sftp_shutdown(sftp);
        if (hosts)
            libssh2_knownhost_free(hosts);
        if (session) {
            libssh2_session_disconnect(session, "done");
            libssh2_session_free(session);
        }
        if (fd != SocketShim::Invalid)
            SocketShim::close(fd);
    }
};

// Everything the worker needs to open a session, by value - nothing reaches back into the
// uploader (or the keychain, or QSettings) off the GUI thread.
struct SessionParams {
    QString host;
    int port = 22;                   // already resolved: never 0
    QString username;
    QString secret;                  // password or key passphrase
    SftpAuthMode authMode = SftpAuthMode::Password;
    QString keyPath;
    std::optional<QString> pinned;   // Snim's pin for host:port, read on the GUI thread
};

// The outcome of establishSession(): either the session is up, or `error` says why in
// words meant for the user. `needsPin` reports that the host was unknown everywhere and
// its `fingerprint` must be pinned - but only once the whole exchange has succeeded, and
// only from the GUI thread (KnownHosts is QSettings I/O).
struct SessionResult {
    bool ok = false;
    QString error;
    bool needsPin = false;
    QString fingerprint;
};

// Steps 1-5, shared by the upload and the connection test: TCP connect, SSH
// handshake, the three-tier host-key policy (BEFORE any credential leaves this machine),
// authentication, and starting the SFTP subsystem. Deliberately the ONLY implementation
// of the host-key check - a second copy is how a security policy silently drifts.
SessionResult establishSession(SshResources &res, const SessionParams &p)
{
    SessionResult out;
    const QString &host = p.host;

    // 1. TCP.
    QString detail;
    res.fd = connectSocket(host, p.port, &detail);
    if (res.fd == SocketShim::Invalid) {
        out.error = detail.isEmpty()
                        ? SftpUploader::tr("Could not connect to %1.").arg(host)
                        : SftpUploader::tr("Could not connect to %1: %2.").arg(host, detail);
        return out;
    }

    // 2. SSH transport.
    res.session = libssh2_session_init();
    if (!res.session) {
        out.error = SftpUploader::tr("Upload failed: could not initialize the SFTP transport.");
        return out;
    }
    libssh2_session_set_timeout(res.session, 30000);   // ms, per blocking operation
    if (libssh2_session_handshake(res.session, res.fd) != 0) {
        const QString why = sessionError(res.session);
        out.error = why.isEmpty()
                        ? SftpUploader::tr("SSH handshake with %1 failed.").arg(host)
                        : SftpUploader::tr("SSH handshake with %1 failed: %2.").arg(host, why);
        return out;
    }

    // 3. Host key, BEFORE sending any credential. Three tiers, and a key that
    //    contradicts a stored one is always a hard failure - never trusted-on-
    //    first-use past a mismatch, which is precisely the MITM case.
    size_t keyLen = 0;
    int keyType = 0;
    const char *hostKey = libssh2_session_hostkey(res.session, &keyLen, &keyType);
    if (!hostKey || keyLen == 0) {
        out.error = SftpUploader::tr("Could not read the host key for %1.").arg(host);
        return out;
    }
    out.fingerprint = hostKeyFingerprint(res.session);

    bool trusted = false;
    res.hosts = libssh2_knownhost_init(res.session);
    switch (checkSystemKnownHosts(res.hosts, host, p.port, hostKey, keyLen)) {
    case HostKeyVerdict::Match:
        trusted = true;
        break;
    case HostKeyVerdict::Mismatch:
        out.error = SftpUploader::tr("The host key for %1 has changed - possible man-in-the-middle "
                                     "attack. Refusing to connect. If the server was reinstalled, "
                                     "remove its entry from ~/.ssh/known_hosts.").arg(host);
        return out;
    case HostKeyVerdict::Unknown:
        break;
    }
    if (!trusted && out.fingerprint.isEmpty()) {
        // Only reachable if libssh2 could not hash the key at all. Without a
        // fingerprint tiers 2 and 3 are meaningless - comparing against a pin would
        // report a bogus mismatch, and pinning "" would disable the check forever -
        // so stop here rather than proceed on an unverifiable key.
        out.error = SftpUploader::tr("Could not verify the host key for %1: its fingerprint is "
                                     "unavailable.").arg(host);
        return out;
    }
    if (!trusted && p.pinned) {
        if (*p.pinned == out.fingerprint) {
            trusted = true;
        } else {
            out.error = SftpUploader::tr("The host key for %1 has changed - possible "
                                         "man-in-the-middle attack. Refusing to connect. If the "
                                         "server was reinstalled, remove its entry from "
                                         "~/.ssh/known_hosts or Snim's remembered host "
                                         "keys.").arg(host);
            return out;
        }
    }
    if (!trusted) {
        // Unknown everywhere: trust on first use. The pin is only written once the
        // whole exchange has succeeded (Outcome::onDone in transfer()), so a server
        // that fails auth or the transfer never gets remembered.
        out.needsPin = true;
    }

    // 4. Authentication.
    const QByteArray userUtf8 = p.username.toUtf8();
    const QByteArray secretUtf8 = p.secret.toUtf8();
    int authRc = 0;
    if (p.authMode == SftpAuthMode::PrivateKey) {
        const QByteArray keyFile = QFile::encodeName(p.keyPath);
        // No separate public-key file: libssh2 derives it from the private key.
        // The passphrase may legitimately be empty for an unencrypted key.
        authRc = libssh2_userauth_publickey_fromfile(res.session, userUtf8.constData(),
                                                     nullptr, keyFile.constData(),
                                                     secretUtf8.constData());
    } else {
        authRc = libssh2_userauth_password(res.session, userUtf8.constData(),
                                           secretUtf8.constData());
    }
    if (authRc != 0) {
        QString message = SftpUploader::tr("Authentication failed for %1@%2.").arg(p.username, host);
        // LIBSSH2_ERROR_FILE is the one code that reliably means "the key file itself
        // was the problem" - missing, not a key, or the wrong passphrase - as opposed
        // to the server rejecting an otherwise valid key (AUTHENTICATION_FAILED /
        // PUBLICKEY_UNVERIFIED), which the generic wording already covers.
        if (p.authMode == SftpAuthMode::PrivateKey && authRc == LIBSSH2_ERROR_FILE) {
            const QString why = sessionError(res.session);
            message += QLatin1Char(' ')
                       + (why.isEmpty()
                              ? SftpUploader::tr("The private key file could not be read or "
                                                 "decrypted.")
                              : SftpUploader::tr("The private key file could not be read or "
                                                 "decrypted (%1).").arg(why));
        }
        out.error = message;
        return out;
    }

    // 5. SFTP subsystem.
    res.sftp = libssh2_sftp_init(res.session);
    if (!res.sftp) {
        const QString why = sessionError(res.session);
        out.error = why.isEmpty()
                        ? SftpUploader::tr("The server did not start an SFTP session.")
                        : SftpUploader::tr("The server did not start an SFTP session: %1.").arg(why);
        return out;
    }

    out.ok = true;
    return out;
}

// The GUI-thread half of preparing a worker: resolve the effective port (0 = 22) and read
// the pin store, so the worker gets nothing but value copies.
SessionParams sessionParamsFor(const UploadConfig &cfg)
{
    SessionParams p;
    p.host = cfg.host;
    p.port = cfg.port > 0 ? cfg.port : 22;
    p.username = cfg.username;
    p.secret = cfg.secretKey;
    p.authMode = cfg.sftpAuth;
    p.keyPath = cfg.privateKeyPath;
    p.pinned = KnownHosts::lookup(p.host, p.port);
    return p;
}

// mkdir -p for the directory part of remotePath. Every error is ignored on purpose: an
// existing directory reports FX_FAILURE or FX_FILE_ALREADY_EXISTS depending on the
// server, and a path that genuinely cannot be created fails loudly at open() instead.
void ensureRemoteDir(LIBSSH2_SFTP *sftp, const QString &remotePath)
{
    const int lastSlash = remotePath.lastIndexOf(QLatin1Char('/'));
    if (lastSlash <= 0)   // <= 0: no directory part, or a bare leading '/' (the root)
        return;
    const QString dir = remotePath.left(lastSlash);
    QString prefix = dir.startsWith(QLatin1Char('/')) ? QStringLiteral("/") : QString();
    const QStringList segments = dir.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        if (!prefix.isEmpty() && !prefix.endsWith(QLatin1Char('/')))
            prefix += QLatin1Char('/');
        prefix += segment;
        const QByteArray dirUtf8 = prefix.toUtf8();
        (void) libssh2_sftp_mkdir(sftp, dirUtf8.constData(), 0755);
    }
}

} // namespace

SftpUploader::SftpUploader(const UploadConfig &config, QObject *parent)
    : BlockingUploader(config, parent)
{
    std::call_once(g_ssh2Init, [] { libssh2_init(0); });
}

QUrl SftpUploader::fallbackUrl(const QString &remotePath) const
{
    // The user name is part of the public identity of the path and stays; the password
    // of course never appears.
    QUrl url;
    url.setScheme(QStringLiteral("sftp"));
    if (!m_config.username.isEmpty())
        url.setUserName(m_config.username);
    url.setHost(m_config.host);
    if (m_config.port > 0)
        url.setPort(m_config.port);
    // QUrl encodes the path for us; one leading slash, never two.
    url.setPath(remotePath.startsWith(QLatin1Char('/')) ? remotePath
                                                        : QLatin1Char('/') + remotePath);
    return url;
}

BlockingUploader::Transfer SftpUploader::transfer() const
{
    // One resolved port for the connect, the known_hosts lookup and the pin store alike -
    // pinning "host:0" and "host:22" separately would trust the same server twice. The
    // pin lookup is QSettings I/O, so it happens here and travels by value. tr() below is
    // the static SftpUploader::tr(), so the worker captures values only.
    return [params = sessionParamsFor(m_config)](QIODevice &body, const Job &job,
                                                 const std::atomic_bool &cancel) {
        Outcome out;
        // 1-5: TCP, handshake, host-key policy, auth, SFTP subsystem - the one place the
        // host-key rules live, for the upload and the test alike.
        SshResources res;
        const SessionResult session = establishSession(res, params);
        if (!session.ok) {
            out.error = session.error;
            return out;
        }
        ensureRemoteDir(res.sftp, job.remotePath);

        // 6. Create the remote file and stream into it.
        const QByteArray remoteUtf8 = job.remotePath.toUtf8();
        res.handle = libssh2_sftp_open(res.sftp, remoteUtf8.constData(),
                                       LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                       LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR
                                           | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
        if (!res.handle) {
            const QString why = sessionError(res.session);
            out.error = why.isEmpty()
                            ? tr("Could not create %1 on the server.").arg(job.remotePath)
                            : tr("Could not create %1 on the server: %2.").arg(job.remotePath, why);
            return out;
        }

        // Give up on the remote file: close the handle first (the unlink can race an
        // open handle otherwise), then drop the half-written file. Both are best-effort -
        // the failure we are about to report is the interesting one.
        auto discardPartial = [&res, &remoteUtf8] {
            if (res.handle) {
                libssh2_sftp_close(res.handle);
                res.handle = nullptr;
            }
            (void) libssh2_sftp_unlink(res.sftp, remoteUtf8.constData());
        };

        char buffer[32 * 1024];

        while (true) {
            if (cancel.load()) {
                discardPartial();
                out.status = Outcome::Status::Cancelled;
                return out;
            }
            const qint64 n = body.read(buffer, sizeof(buffer));
            if (n < 0) {
                discardPartial();
                out.error = tr("Upload failed: could not read the local file.");
                return out;
            }
            if (n == 0)
                break;                          // EOF

            // Partial writes are normal: the return is how many bytes the server took,
            // so a chunk may need several passes. 0 or negative means the transfer died.
            qint64 offset = 0;
            while (offset < n) {
                if (cancel.load()) {
                    discardPartial();
                    out.status = Outcome::Status::Cancelled;
                    return out;
                }
                const ssize_t written = libssh2_sftp_write(res.handle, buffer + offset,
                                                           static_cast<size_t>(n - offset));
                if (written <= 0) {
                    const QString why = sessionError(res.session);
                    discardPartial();
                    if (why.isEmpty())
                        out.error = job.testing ? tr("Test failed while writing to the server.")
                                                : tr("Upload failed while writing to the server.");
                    else
                        out.error = (job.testing
                                         ? tr("Test failed while writing to the server: %1.")
                                         : tr("Upload failed while writing to the server: %1."))
                                        .arg(why);
                    return out;
                }
                offset += written;
            }
        }

        // Close before reporting success: a write error can still surface here, and
        // publishing a link to a file the server never fully accepted would be a lie.
        const int closeRc = libssh2_sftp_close(res.handle);
        res.handle = nullptr;
        if (closeRc != 0) {
            const QString why = sessionError(res.session);
            (void) libssh2_sftp_unlink(res.sftp, remoteUtf8.constData());
            if (why.isEmpty())
                out.error = job.testing ? tr("Test failed while finishing the remote file.")
                                        : tr("Upload failed while finishing the remote file.");
            else
                out.error = (job.testing
                                 ? tr("Test failed while finishing the remote file: %1.")
                                 : tr("Upload failed while finishing the remote file: %1."))
                                .arg(why);
            return out;
        }

        // The probe goes again; a failed cleanup is a note, not a failure.
        if (job.testing)
            out.probeLeft = libssh2_sftp_unlink(res.sftp, remoteUtf8.constData()) != 0;
        out.status = Outcome::Status::Done;
        // Trust-on-first-use is only recorded once the whole exchange worked, and on the
        // GUI thread because KnownHosts is QSettings I/O.
        if (session.needsPin) {
            out.onDone = [host = params.host, port = params.port, pin = session.fingerprint] {
                KnownHosts::remember(host, port, pin);
            };
        }
        return out;
    };
}

} // namespace Upload
