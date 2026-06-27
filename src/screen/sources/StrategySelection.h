#ifndef SCREEN_STRATEGYSELECTION_H
#define SCREEN_STRATEGYSELECTION_H

#include "FrameSourceFactory.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

/**
 * The pure half of FrameSourceFactory's choice, taking the desktop and sandbox as inputs
 * so the matrix is testable without that desktop.
 */
namespace Screen::StrategySelection {

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

// Native screencopy beats both portals wherever its globals show up, except on KWin and Mutter.
inline bool prefersScreencopy(const QString &currentDesktop)
{
    return !desktopIs(currentDesktop, QStringLiteral("KDE"))
           && !desktopIs(currentDesktop, QStringLiteral("GNOME"));
}

// What the session offers; each probe runs only when the choice reaches it, unset is false.
struct Probes {
    std::function<bool()> kwin;          // KWin ScreenShot2 is registered
    std::function<bool()> screencopy;    // ext-image-copy-capture or wlr-screencopy advertised
    std::function<bool()> screencast;    // a persisting ScreenCast portal and the frame grabber
    std::function<bool()> portal;        // a Wayland session with the Screenshot portal or tools
};

inline bool probe(const std::function<bool()> &check)
{
    return check && check();
}

// The whole matrix; every pick still falls back to the Screenshot portal at runtime.
inline SourceType choose(const QString &currentDesktop, bool flatpak, const Probes &probes)
{
    if (!flatpak && probe(probes.kwin))
        return SourceType::KWin;
    if (prefersScreencopy(currentDesktop) && probe(probes.screencopy))
        return SourceType::Screencopy;
    if (prefersScreencast(currentDesktop, flatpak) && probe(probes.screencast))
        return SourceType::Screencast;
    if (probe(probes.portal))
        return SourceType::Wayland;
    return SourceType::Native;
}

// Where a full-desktop frame comes from, in order: the screenshot pick's own source, then
// the Screenshot portal, which asks with its dialog when a silent request is refused.
inline QList<SourceType> frameSourceChain(SourceType chosen)
{
    using Type = SourceType;
    switch (chosen) {
    case Type::KWin:
    case Type::Screencast:
    case Type::Screencopy:
        return {chosen, Type::Wayland};
    case Type::Native:
        return {Type::Native};
    case Type::Wayland:
    case Type::Auto:
        break;
    }
    return {Type::Wayland};
}

// A SNIM_CAPTURE_STRATEGY value; nothing for an empty or unknown one.
inline std::optional<SourceType> parseOverride(const QString &value)
{
    const QString name = value.trimmed().toLower();
    if (name == QLatin1String("kwin"))
        return SourceType::KWin;
    if (name == QLatin1String("screencast"))
        return SourceType::Screencast;
    if (name == QLatin1String("screencopy") || name == QLatin1String("wlroots"))
        return SourceType::Screencopy;
    if (name == QLatin1String("wayland") || name == QLatin1String("portal"))
        return SourceType::Wayland;
    if (name == QLatin1String("native"))
        return SourceType::Native;
    return std::nullopt;
}

} // namespace Screen::StrategySelection

#endif // SCREEN_STRATEGYSELECTION_H
