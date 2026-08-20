#include "core/Portal.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDebug>
#include <QSet>
#include <QUuid>

namespace Core::Portal {

namespace {

const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");
const QString kResponse = QStringLiteral("Response");

// Probes run on the GUI thread, so a stuck portal may only hold it this long.
constexpr int kProbeTimeoutMs = 2000;

} // namespace

QString newToken()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

QString sessionHandle(const QVariantMap &results)
{
    const QVariant handle = results.value(QStringLiteral("session_handle"));
    const QString path = handle.toString();
    if (path.isEmpty() && handle.canConvert<QDBusObjectPath>())
        return handle.value<QDBusObjectPath>().path();
    return path;
}

QVariant property(const QString &interface, const QString &name)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    msg.setArguments({interface, name});
    const QDBusMessage reply =
        QDBusConnection::sessionBus().call(msg, QDBus::Block, kProbeTimeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};
    return reply.arguments().constFirst().value<QDBusVariant>().variant();
}

bool hasInterface(const QString &interface)
{
    static QSet<QString> answered;
    if (answered.contains(interface))
        return true;
    if (!property(interface, QStringLiteral("version")).isValid())
        return false;
    answered.insert(interface);
    return true;
}

void sendRequest(const QString &interface, const QString &method, const QVariantList &args,
                 QObject *context, const std::function<void(const QString &error)> &onError)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, interface, method);
    msg.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg),
                                                context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context,
                     [interface, method, onError](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (!reply.isError())
            return;
        qWarning().noquote() << interface.section(QLatin1Char('.'), -1) << method << "failed:"
                             << reply.error().message();
        onError(reply.error().message());
    });
}

void closeSession(const QString &sessionPath)
{
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
        kService, sessionPath, QStringLiteral("org.freedesktop.portal.Session"),
        QStringLiteral("Close")));
}

bool Request::listen(QObject *receiver, const char *slot)
{
    stop();
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_token = newToken();
    m_path = pathFor(bus.baseService(), m_token);
    if (!bus.connect(kService, m_path, kRequest, kResponse, receiver, slot)) {
        qWarning() << "Failed to connect portal Response signal on path:" << m_path;
        m_path.clear();
        return false;
    }
    m_receiver = receiver;
    m_slot = slot;
    return true;
}

void Request::stop()
{
    if (!m_path.isEmpty())
        QDBusConnection::sessionBus().disconnect(kService, m_path, kRequest, kResponse,
                                                 m_receiver, m_slot);
    m_path.clear();
}

QString Request::pathFor(const QString &uniqueName, const QString &token)
{
    QString sender = uniqueName;
    sender.remove(QLatin1Char(':')).replace(QLatin1Char('.'), QLatin1Char('_'));
    return kPath + QStringLiteral("/request/") + sender + QLatin1Char('/') + token;
}

} // namespace Core::Portal
