#ifndef CORE_DESKTOP_H
#define CORE_DESKTOP_H

#include <QString>
#include <QStringList>

/**
 * Pure checks on the desktop name, shared by the capture and hotkey choices so each
 * takes the desktop as an input and stays testable without that desktop.
 */
namespace Core {

// XDG_CURRENT_DESKTOP is a colon-separated list such as "ubuntu:GNOME".
inline bool desktopIs(const QString &currentDesktop, const QString &name)
{
    const QStringList parts = currentDesktop.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

} // namespace Core

#endif // CORE_DESKTOP_H
