#ifndef SCREEN_FRAMESOURCEFACTORY_H
#define SCREEN_FRAMESOURCEFACTORY_H

#include <QObject>
#include <QString>

#include <memory>

namespace Screen {

class DesktopFrameSource;

// The ways to the screen this desktop may offer; the capture strategies follow them.
enum class SourceType {
    Auto,           // Automatically select best available strategy
    KWin,           // KWin ScreenShot2 D-Bus (preferred on KDE Plasma)
    Portal,         // The Screenshot portal on Wayland
    Screencast,     // One frame from a restored ScreenCast portal session
    Screencopy,     // Native Wayland screencopy (wlroots-family compositors)
    Native          // Force native Qt strategy
};

/**
 * Creates the full-desktop frame source behind each SourceType and picks the type this
 * desktop prefers, for the capture strategies and the frozen frame alike.
 */
class FrameSourceFactory
{
public:
    // Null for Auto, a source not built in, or one this session does not offer. The portal
    // and Qt grab sources exist wherever they are built; their grab says what is missing.
    [[nodiscard]] static std::unique_ptr<DesktopFrameSource> create(SourceType type,
                                                                    QObject *parent = nullptr);

    // StrategySelection over this desktop; SNIM_CAPTURE_STRATEGY forces a type, for testing.
    [[nodiscard]] static SourceType defaultType();

    // What logs and messages call each type.
    [[nodiscard]] static QString typeName(SourceType type);

private:
    FrameSourceFactory() = default;
};

} // namespace Screen

#endif // SCREEN_FRAMESOURCEFACTORY_H
