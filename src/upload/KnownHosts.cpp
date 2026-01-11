#include "upload/KnownHosts.h"

#include "core/Settings.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace Upload::KnownHosts {

namespace {

QString pinKey(const QString &host, int port)
{
    return host + QLatin1Char(':') + QString::number(port);
}

QJsonObject read()
{
    return QJsonDocument::fromJson(Core::Settings::uploadKnownHostKeys().toUtf8()).object();
}

} // namespace

std::optional<QString> lookup(const QString &host, int port)
{
    const QJsonValue v = read().value(pinKey(host, port));
    if (!v.isString())
        return std::nullopt;
    return v.toString();
}

void remember(const QString &host, int port, const QString &fingerprint)
{
    if (host.isEmpty() || fingerprint.isEmpty())
        return;
    QJsonObject o = read();
    o[pinKey(host, port)] = fingerprint;
    Core::Settings::setUploadKnownHostKeys(
        QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
}

} // namespace Upload::KnownHosts
