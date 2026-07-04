#ifndef SCREEN_SCREENCASTFRAMESOURCE_H
#define SCREEN_SCREENCASTFRAMESOURCE_H

#include "DesktopFrameSource.h"

#include <QElapsedTimer>

class QTimer;

namespace Screen {
class ScreenCastPortalSession;
} // namespace Screen

namespace Screen {

/**
 * One frame per monitor from a ScreenCast portal session that the user approved once
 * and the portal restores from a persisted token, stitched into the virtual desktop;
 * the session is closed right after. Silent where the compositor cannot be asked
 * directly (GNOME, any Flatpak) once that first pick is made.
 */
class ScreencastFrameSource : public DesktopFrameSource
{
    Q_OBJECT

public:
    // Shared by screenshots and the recording selector, so one pick covers both.
    static constexpr char kRestoreTokenKey[] = "capture/screencastRestoreToken";

    explicit ScreencastFrameSource(QObject *parent = nullptr);
    ~ScreencastFrameSource() override;

    void grab() override;

    // Wayland, a ScreenCast portal that persists consent, and the module's frame grabber.
    [[nodiscard]] static bool isSupported();

    // Whether an earlier pick was remembered, so the next grab should be silent.
    [[nodiscard]] static bool hasRestoreToken();

signals:
    // No consent stored yet: the portal is about to show its screen picker.
    void sourcePickerExpected();

private:
    void handleReady(int pipewireFd);
    void fail(const QString &reason, bool cancelled = false);

    ScreenCastPortalSession *m_session = nullptr;
    QTimer *m_timeout = nullptr;
    QElapsedTimer m_clock;
    bool m_busy = false;
    bool m_pickerExpected = false;
};

} // namespace Screen

#endif // SCREEN_SCREENCASTFRAMESOURCE_H
