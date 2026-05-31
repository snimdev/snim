#ifndef CAPTURE_STRATEGYSELECTION_H
#define CAPTURE_STRATEGYSELECTION_H

#include "CaptureFactory.h"

#include <QString>
#include <QStringList>

#include <optional>

/**
 * The pure half of CaptureFactory's choice, taking the desktop and sandbox as inputs
 * so the matrix is testable without that desktop.
 */
namespace Capture::StrategySelection {

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

// GNOME's Screenshot portal asks on every call, and so does any sandboxed app outside
// KDE; on KDE in Flatpak the silent Screenshot request already works.
inline bool prefersScreencast(const QString &currentDesktop, bool flatpak)
{
    if (desktopIs(currentDesktop, QStringLiteral("GNOME")))
        return true;
    return flatpak && !desktopIs(currentDesktop, QStringLiteral("KDE"));
}

// A SNIM_CAPTURE_STRATEGY value; nothing for an empty or unknown one.
inline std::optional<CaptureFactory::StrategyType> parseOverride(const QString &value)
{
    const QString name = value.trimmed().toLower();
    if (name == QLatin1String("kwin"))
        return CaptureFactory::StrategyType::KWin;
    if (name == QLatin1String("screencast"))
        return CaptureFactory::StrategyType::Screencast;
    if (name == QLatin1String("wayland") || name == QLatin1String("portal"))
        return CaptureFactory::StrategyType::Wayland;
    if (name == QLatin1String("native"))
        return CaptureFactory::StrategyType::Native;
    return std::nullopt;
}

} // namespace Capture::StrategySelection

#endif // CAPTURE_STRATEGYSELECTION_H
