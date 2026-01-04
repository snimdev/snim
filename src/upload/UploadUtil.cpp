#include "upload/UploadUtil.h"
#include "upload/SigV4.h"

#include <QFileInfo>
#include <QStringList>
#include <QUrl>
#include <QUuid>

namespace Upload::Util {

QString sanitizeHint(const QString &keyHint)
{
    QString name = QFileInfo(keyHint).fileName();
    return name.isEmpty() ? QStringLiteral("upload.bin") : name;
}

QString uniqueRemoteName(const QString &keyHint)
{
    const QString unique = QUuid::createUuid().toString(QUuid::Id128).left(8);
    return unique + QLatin1Char('-') + sanitizeHint(keyHint);
}

QString buildRemotePath(const QString &remoteDir, const QString &fileName)
{
    const bool absolute = remoteDir.startsWith(QLatin1Char('/'));
    const QStringList parts = (remoteDir + QLatin1Char('/') + fileName)
                                  .split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QString joined = parts.join(QLatin1Char('/'));
    return absolute ? QLatin1Char('/') + joined : joined;
}

QString joinPublicUrl(const QString &publicBaseUrl, const QString &remoteName)
{
    QString base = publicBaseUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    // Same encoder the S3 request URL uses, so a public link keeps matching the object key.
    return base + QLatin1Char('/') + SigV4::awsUriEncode(remoteName, /*encodeSlash=*/false);
}

QString buildFtpUrl(const UploadConfig &cfg, const QString &remotePath)
{
    QString url = cfg.ftpEncryption == FtpEncryption::Implicit ? QStringLiteral("ftps://")
                                                               : QStringLiteral("ftp://");
    url += cfg.host;
    if (cfg.port > 0)
        url += QLatin1Char(':') + QString::number(cfg.port);
    // Percent-encode each segment (spaces, UTF-8, ...) - curl's URL parser rejects raw
    // spaces - while the separators stay literal so curl still CWDs per directory.
    const bool absolute = remotePath.startsWith(QLatin1Char('/'));
    QStringList segments = remotePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (QString &s : segments)
        s = QString::fromLatin1(QUrl::toPercentEncoding(s));
    const QString path = segments.join(QLatin1Char('/'));
    return url + QLatin1Char('/') + (absolute ? QStringLiteral("%2F") + path : path);
}

} // namespace Upload::Util
