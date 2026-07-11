#ifndef CAPTURE_SCREENCASTCAPTURESTRATEGY_H
#define CAPTURE_SCREENCASTCAPTURESTRATEGY_H

#include "WaylandCaptureStrategy.h"
#include "screen/sources/ScreencastFrameSource.h"

namespace Capture {

/**
 * Silent screenshots where the compositor cannot be asked directly (GNOME, any
 * Flatpak): the desktop comes from a ScreencastFrameSource, whose ScreenCast session
 * the user approved once. Extends the Screenshot-portal strategy and falls back to it
 * whenever the session cannot deliver, but never after the user cancelled the picker.
 */
class ScreencastCaptureStrategy : public WaylandCaptureStrategy
{
    Q_OBJECT

public:
    explicit ScreencastCaptureStrategy(QObject *parent = nullptr);
    ~ScreencastCaptureStrategy() override;

    void captureFullScreen() override;
    void captureArea() override;
    // Same as full screen, as on the Screenshot-portal path it extends.
    void captureWindow() override;

    bool isAvailable() const override;
    QString name() const override { return QStringLiteral("ScreenCast Portal Capture"); }

signals:
    // No consent stored yet: the portal is about to show its screen picker.
    // lastPickMissedScreens: the pick it remembered left a screen out.
    void sourcePickerExpected(bool lastPickMissedScreens);

private:
    void begin(bool showSelector);
    void fallBack(const QString &reason);
    // The Screenshot-portal path this strategy extends.
    void usePortal();

    Screen::ScreencastFrameSource *m_source = nullptr;
    bool m_busy = false;
    bool m_showSelector = false;
};

} // namespace Capture

#endif // CAPTURE_SCREENCASTCAPTURESTRATEGY_H
