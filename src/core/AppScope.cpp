#include "core/AppScope.h"

#include <QFile>
#include <QIODevice>
#include <QLatin1String>
#include <QList>

#ifdef Q_OS_LINUX
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QPair>
#include <QThread>
#include <QVariant>

#include <unistd.h>

// StartTransientUnit takes a(sv) properties plus a(sa(sv)) aux, which QtDBus cannot
// marshal on its own. Global scope is required: Q_DECLARE_METATYPE cannot sit in a namespace.
using SnimScopeProperty = QPair<QString, QDBusVariant>;
using SnimScopeProperties = QList<SnimScopeProperty>;
using SnimScopeAux = QPair<QString, SnimScopeProperties>;
using SnimScopeAuxList = QList<SnimScopeAux>;

Q_DECLARE_METATYPE(SnimScopeProperty)
Q_DECLARE_METATYPE(SnimScopeProperties)
Q_DECLARE_METATYPE(SnimScopeAux)
Q_DECLARE_METATYPE(SnimScopeAuxList)

QDBusArgument &operator<<(QDBusArgument &arg, const SnimScopeProperty &property)
{
    arg.beginStructure();
    arg << property.first << property.second;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, SnimScopeProperty &property)
{
    arg.beginStructure();
    arg >> property.first >> property.second;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const SnimScopeAux &aux)
{
    arg.beginStructure();
    arg << aux.first << aux.second;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, SnimScopeAux &aux)
{
    arg.beginStructure();
    arg >> aux.first >> aux.second;
    arg.endStructure();
    return arg;
}
#endif // Q_OS_LINUX

namespace Core::AppScope {

bool unitLineIndicatesAppScope(QStringView cgroupLine)
{
    // v2 lines are "0::<path>", v1 "<id>:<controllers>:<path>"; the unit is the leaf either way.
    const qsizetype slash = cgroupLine.lastIndexOf(u'/');
    if (slash < 0)
        return false;

    const QStringView unit = cgroupLine.sliced(slash + 1).trimmed();
    if (!unit.endsWith(QLatin1String(".scope")) && !unit.endsWith(QLatin1String(".service")))
        return false;
    // systemd names app units app-[<launcher>-]<app id>-<random>.{scope,service}.
    if (!unit.startsWith(QLatin1String("app-")))
        return false;
    // A terminal parks its children in vte-spawn scopes, which name no application at all.
    return !unit.sliced(4).startsWith(QLatin1String("vte-spawn-"));
}

bool inAppScope()
{
#ifdef Q_OS_LINUX
    QFile cgroup(QStringLiteral("/proc/self/cgroup"));
    if (!cgroup.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // readAll(), never readLine(): a /proc file reports size 0, so atEnd() is true at once.
    const QString contents = QString::fromUtf8(cgroup.readAll());
    const QList<QStringView> lines = QStringView(contents).split(u'\n', Qt::SkipEmptyParts);
    for (const QStringView line : lines) {
        if (unitLineIndicatesAppScope(line))
            return true;
    }
    return false;
#else
    return true;
#endif
}

bool adoptAppScope(QString *errorOut)
{
#ifdef Q_OS_LINUX
    qDBusRegisterMetaType<SnimScopeProperty>();
    qDBusRegisterMetaType<SnimScopeProperties>();
    qDBusRegisterMetaType<SnimScopeAux>();
    qDBusRegisterMetaType<SnimScopeAuxList>();
    // The PIDs value has to reach systemd as an 'au', so its list needs a D-Bus type too.
    qDBusRegisterMetaType<QList<uint>>();

    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        if (errorOut) {
            const QDBusError error = bus.lastError();
            *errorOut = error.isValid() ? error.message()
                                        : QStringLiteral("no session bus connection");
        }
        return false;
    }

    const auto pid = static_cast<uint>(getpid());

    SnimScopeProperties properties;
    properties.append({QStringLiteral("PIDs"),
                       QDBusVariant(QVariant::fromValue(QList<uint>{pid}))});
    // Lets systemd reap the scope as soon as this process is gone, failed run included.
    properties.append({QStringLiteral("CollectMode"),
                       QDBusVariant(QVariant(QStringLiteral("inactive-or-failed")))});

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.systemd1"),
        QStringLiteral("/org/freedesktop/systemd1"),
        QStringLiteral("org.freedesktop.systemd1.Manager"),
        QStringLiteral("StartTransientUnit"));
    // The unit name is what the portal reads the app id back out of.
    message.setArguments({QStringLiteral("app-dev.snim.Snim-%1.scope").arg(pid),
                          QStringLiteral("fail"),
                          QVariant::fromValue(properties),
                          QVariant::fromValue(SnimScopeAuxList{})});

    // Blocking, but bounded by the default D-Bus timeout and called once, before the UI runs.
    const QDBusMessage reply = bus.call(message);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (errorOut) {
            const QDBusError error(reply);
            *errorOut = error.message().isEmpty() ? error.name() : error.message();
        }
        return false;
    }

    // The reply only says the job was queued; the move itself lands a few ms later, and the
    // portal reads the cgroup on its first request, one event loop turn after this returns.
    for (int attempt = 0; attempt < 250; ++attempt) {
        if (inAppScope())
            return true;
        QThread::msleep(1);
    }

    if (errorOut)
        *errorOut = QStringLiteral("the scope was started but this process is not in it yet");
    return false;
#else
    if (errorOut)
        *errorOut = QStringLiteral("app scopes are a systemd concept");
    return false;
#endif
}

} // namespace Core::AppScope
