#ifndef UPLOAD_UPLOADUTIL_H
#define UPLOAD_UPLOADUTIL_H

#include "upload/UploadConfig.h"

#include <QString>

/**
 * Remote-name / path / URL helpers shared by every uploader backend. Pure string work -
 * no network, no keychain, no optional dependency - so they compile everywhere and are
 * unit-testable without a server.
 */
namespace Upload::Util {

// Strip any path components from the hint so a stray "../" can't escape the prefix.
[[nodiscard]] QString sanitizeHint(const QString &keyHint);

// The remote file name: a short uuid (avoids collisions / overwrites) + the safe hint.
[[nodiscard]] QString uniqueRemoteName(const QString &keyHint);

// Join a remote directory and a file name: doubled/trailing slashes collapse, a single
// leading '/' survives (= absolute), and an empty dir yields the bare file name
// (login-dir-relative).
[[nodiscard]] QString buildRemotePath(const QString &remoteDir, const QString &fileName);

// Public link: the base URL (trailing slashes trimmed) + the percent-encoded name.
[[nodiscard]] QString joinPublicUrl(const QString &publicBaseUrl, const QString &remoteName);

// Credential-free ftp:// (plain / explicit TLS) or ftps:// (implicit TLS) URL for
// libcurl; the port appears only when set. Path segments are percent-encoded (slashes
// stay literal). curl reads an FTP path as relative to the login directory, so an
// absolute remotePath's leading '/' becomes "%2F".
[[nodiscard]] QString buildFtpUrl(const UploadConfig &cfg, const QString &remotePath);

} // namespace Upload::Util

#endif // UPLOAD_UPLOADUTIL_H
