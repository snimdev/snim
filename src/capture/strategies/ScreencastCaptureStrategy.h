#ifndef CAPTURE_SCREENCASTCAPTURESTRATEGY_H
#define CAPTURE_SCREENCASTCAPTURESTRATEGY_H

#include "WaylandCaptureStrategy.h"

#include <QElapsedTimer>

class QTimer;

namespace Recording {
class ScreenCastPortalSession;
} // namespace Recording

namespace Capture {

/**
 * Silent screenshots where the compositor cannot be asked directly (GNOME, any
 * Flatpak): one frame per monitor from a ScreenCast portal session that the user
 * approved once and the portal restores from a persisted token, stitched into the
 * virtual desktop, then the session is closed. Extends the Screenshot-portal strategy
 * and falls back to it whenever the session cannot deliver.
 */
class ScreencastCaptureStrategy : public WaylandCaptureStrategy
{
    Q_OBJECT

public:
    // Separate from the recorder's, so screenshot and recording consent stay independent.
    static constexpr char kRestoreTokenKey[] = "capture/screencastRestoreToken";

    explicit ScreencastCaptureStrategy(QObject *parent = nullptr);
    ~ScreencastCaptureStrategy() override;

    void captureFullScreen() override;
    void captureArea() override;
    // Same as full screen, as on the Screenshot-portal path it extends.
    void captureWindow() override;

    bool isAvailable() const override;
    QString name() const override { return QStringLiteral("ScreenCast Portal Capture"); }

    // Wayland, a ScreenCast portal that persists consent, and the module's frame grabber.
    [[nodiscard]] static bool isSupported();

    // Whether an earlier pick was remembered, so the next capture should be silent.
    [[nodiscard]] static bool hasRestoreToken();

signals:
    // No consent stored yet: the portal is about to show its screen picker.
    void sourcePickerExpected();

private:
    void begin(bool showSelector);
    void handleReady(int pipewireFd);
    void handleFailed(const QString &error);
    void fallBack(const QString &reason);
    void finish();

    Recording::ScreenCastPortalSession *m_session = nullptr;
    QTimer *m_timeout = nullptr;
    QElapsedTimer m_clock;
    bool m_busy = false;
    bool m_showSelector = false;
    bool m_pickerExpected = false;
};

} // namespace Capture

#endif // CAPTURE_SCREENCASTCAPTURESTRATEGY_H
