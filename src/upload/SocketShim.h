#ifndef UPLOAD_SOCKETSHIM_H
#define UPLOAD_SOCKETSHIM_H

#include <QString>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

/**
 * Adapter over POSIX sockets and Winsock: one blocking-TCP vocabulary for the uploaders
 * that open their own descriptor (libssh2 takes a socket, not a URL). It only papers over
 * what differs (handle type, invalid value, close, error text, timeouts, WSAStartup);
 * addrinfo and friends are the same on both and are used as-is.
 */
namespace Upload::SocketShim {

// Matches libssh2_socket_t, so a Handle goes straight into libssh2_session_handshake().
#ifdef Q_OS_WIN
using Handle = SOCKET;
inline constexpr Handle Invalid = INVALID_SOCKET;
#else
using Handle = int;
inline constexpr Handle Invalid = -1;
#endif

// getaddrinfo(), after starting Winsock once per process (a no-op on POSIX).
int resolve(const char *node, const char *service, const addrinfo *hints, addrinfo **res);
void freeResolved(addrinfo *res);
[[nodiscard]] QString resolveError(int rc);

[[nodiscard]] Handle open(const addrinfo *ai);
[[nodiscard]] bool connect(Handle s, const addrinfo *ai);
void close(Handle s);

// Text for the last failed open/connect on this thread; read it before any other call.
[[nodiscard]] QString lastError();

// Bounds every blocking read and write on s.
void setIoTimeout(Handle s, int seconds);

} // namespace Upload::SocketShim

#endif // UPLOAD_SOCKETSHIM_H
