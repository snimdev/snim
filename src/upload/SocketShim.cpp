#include "upload/SocketShim.h"

#ifdef Q_OS_WIN
#include <mutex>
#else
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace Upload::SocketShim {

namespace {

#ifdef Q_OS_WIN
// Never paired with WSACleanup: sockets may still be open on a worker at exit.
std::once_flag g_wsaInit;
#endif

void ensureStarted()
{
#ifdef Q_OS_WIN
    std::call_once(g_wsaInit, [] {
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
    });
#endif
}

} // namespace

int resolve(const char *node, const char *service, const addrinfo *hints, addrinfo **res)
{
    ensureStarted();
    return ::getaddrinfo(node, service, hints, res);
}

void freeResolved(addrinfo *res)
{
    ::freeaddrinfo(res);
}

QString resolveError(int rc)
{
#ifdef Q_OS_WIN
    return qt_error_string(rc);   // getaddrinfo returns a WSA error code here
#else
    return QString::fromUtf8(::gai_strerror(rc));
#endif
}

Handle open(const addrinfo *ai)
{
    ensureStarted();
    return ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
}

bool connect(Handle s, const addrinfo *ai)
{
#ifdef Q_OS_WIN
    return ::connect(s, ai->ai_addr, static_cast<int>(ai->ai_addrlen)) == 0;
#else
    return ::connect(s, ai->ai_addr, ai->ai_addrlen) == 0;
#endif
}

void close(Handle s)
{
#ifdef Q_OS_WIN
    ::closesocket(s);
#else
    ::close(s);
#endif
}

QString lastError()
{
#ifdef Q_OS_WIN
    return qt_error_string(WSAGetLastError());
#else
    return QString::fromUtf8(std::strerror(errno));
#endif
}

void setIoTimeout(Handle s, int seconds)
{
#ifdef Q_OS_WIN
    const DWORD ms = static_cast<DWORD>(seconds) * 1000;
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&ms), sizeof(ms));
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&ms), sizeof(ms));
#else
    timeval tv{};
    tv.tv_sec = seconds;
    ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

} // namespace Upload::SocketShim
