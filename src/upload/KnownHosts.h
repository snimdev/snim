#ifndef UPLOAD_KNOWNHOSTS_H
#define UPLOAD_KNOWNHOSTS_H

#include <QString>
#include <optional>

namespace Upload {

/**
 * Trust-on-first-use pin store for SFTP host keys: "host:port" -> fingerprint
 * ("SHA256:<base64>"), kept as a JSON object in QSettings via Core::Settings.
 *
 * App-level rather than a profile field on purpose: two profiles on the same host share
 * one pin, and a fingerprint learned mid-upload can't be dropped by the settings
 * dialog's working-copy commit. QSettings I/O, so call it from the GUI thread only.
 */
namespace KnownHosts {

// port is the effective port - resolve 0 to the protocol default before calling, or the
// same host pins twice.
std::optional<QString> lookup(const QString &host, int port);
void                   remember(const QString &host, int port, const QString &fingerprint);

} // namespace KnownHosts

} // namespace Upload

#endif // UPLOAD_KNOWNHOSTS_H
