#include "core/SingleInstance.h"

#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>

#include <windows.h>

namespace Core::SingleInstance {

namespace {

// Pipe names are machine-wide, unlike the Local\ mutex, so the session id keeps users apart.
QString serverName()
{
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    return QStringLiteral("dev.snim.Snim-%1").arg(session);
}

} // namespace

bool claim()
{
    // Never closed: Windows releases it when the process exits.
    const HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\dev.snim.Snim");
    return !(mutex && GetLastError() == ERROR_ALREADY_EXISTS);
}

void notifyRunningInstance()
{
    // The connection itself is the message.
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (socket.waitForConnected(1000))
        socket.disconnectFromServer();
}

void listen(QObject *parent, std::function<void()> onLaunch)
{
    auto *server = new QLocalServer(parent);
    server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server->listen(serverName())) {
        qWarning() << "[SingleInstance] cannot listen:" << server->errorString();
        return;
    }
    QObject::connect(server, &QLocalServer::newConnection, server,
                     [server, onLaunch = std::move(onLaunch)] {
                         while (QLocalSocket *socket = server->nextPendingConnection())
                             socket->deleteLater();
                         onLaunch();
                     });
}

} // namespace Core::SingleInstance
