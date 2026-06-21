#ifndef CAPTURE_SCREENCASTCAPTURESTRATEGY_H
#define CAPTURE_SCREENCASTCAPTURESTRATEGY_H

#include "WaylandCaptureStrategy.h"
#include "capture/sources/ScreencastFrameSource.h"

namespace Capture {

/**
 * Silent screenshots where the compositor cannot be asked directly (GNOME, any
 * Flatpak): the desktop comes from a ScreencastFrameSource, whose ScreenCast session
 * the user approved once. Extends the Screenshot-portal strategy and falls back to it
 * whenever the session cannot deliver.
 */
class ScreencastCaptureStrategy : public WaylandCaptureStrategy
{
    Q_OBJECT

public:
    static constexpr const char *kRestoreTokenKey = ScreencastFrameSource::kRestoreTokenKey;

    explicit ScreencastCaptureStrategy(QObject *parent = nullptr);
    ~ScreencastCaptureStrategy() override;

    void captureFullScreen() override;
    void captureArea() override;
    // Same as full screen, as on the Screenshot-portal path it extends.
    void captureWindow() override;

    bool isAvailable() const override;
    QString name() const override { return QStringLiteral("ScreenCast Portal Capture"); }

    [[nodiscard]] static bool isSupported() { return ScreencastFrameSource::isSupported(); }
    [[nodiscard]] static bool hasRestoreToken() { return ScreencastFrameSource::hasRestoreToken(); }

signals:
    // No consent stored yet: the portal is about to show its screen picker.
    void sourcePickerExpected();

private:
    void begin(bool showSelector);
    void fallBack(const QString &reason);

    ScreencastFrameSource *m_source = nullptr;
    bool m_busy = false;
    bool m_showSelector = false;
};

} // namespace Capture

#endif // CAPTURE_SCREENCASTCAPTURESTRATEGY_H
