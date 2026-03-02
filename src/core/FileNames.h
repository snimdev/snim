#ifndef CORE_FILENAMES_H
#define CORE_FILENAMES_H

#include <QDateTime>
#include <QString>

namespace Core {

/**
 * Default names for the files Snim produces: "<prefix>-yyyy-MM-dd_HH-mm-ss.<ext>".
 * Header-only and pure so screenshots, recordings and their exports share one pattern.
 */
inline QString timestampedFileName(const QString &prefix, const QDateTime &when,
                                   const QString &extension)
{
    QString ext = extension;
    if (ext.startsWith(QLatin1Char('.')))
        ext.remove(0, 1);
    QString name = prefix + QLatin1Char('-')
                   + when.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    if (!ext.isEmpty())
        name += QLatin1Char('.') + ext;
    return name;
}

inline QString screenshotFileName(const QDateTime &when, const QString &extension)
{
    return timestampedFileName(QStringLiteral("snimshoot"), when, extension);
}

inline QString recordingFileName(const QDateTime &when, const QString &extension)
{
    return timestampedFileName(QStringLiteral("snimcapture"), when, extension);
}

} // namespace Core

#endif // CORE_FILENAMES_H
