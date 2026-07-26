#ifndef SCREEN_KWINFRAMESOURCE_H
#define SCREEN_KWINFRAMESOURCE_H

#include "DesktopFrameSource.h"
#include "screen/DesktopStitch.h"

#include <QImage>
#include <QList>
#include <QVariantMap>

#include <functional>

namespace Screen {

/**
 * Every screen straight from KWin's org.kde.KWin.ScreenShot2, pixels streamed over a
 * pipe, stitched at the highest scale. Never falls back to CaptureInteractive: a
 * refusal (no desktop entry authorizing the interface) fails with wasDenied() set, and
 * the caller decides what comes next.
 */
class KWinFrameSource : public DesktopFrameSource
{
    Q_OBJECT

public:
    explicit KWinFrameSource(QObject *parent = nullptr);

    void grab() override;

    // Whether KWin's ScreenShot2 service is on the session bus.
    [[nodiscard]] static bool isServiceRegistered();

    // 0 when KWin did not answer the Version property.
    [[nodiscard]] quint32 apiVersion() const { return m_apiVersion; }

    // Whether the last failure was KWin refusing this app.
    [[nodiscard]] bool wasDenied() const { return m_denied; }

    // Off by default: a frame frozen under a selector must not show a stale pointer.
    void setIncludeCursor(bool include) { m_includeCursor = include; }

    static QVariantMap buildOptions(bool includeCursor = true);

    // One ScreenShot2 reply: the image, or why there is none.
    struct Shot {
        QImage image;
        QString error;
        bool denied = false;      // no desktop entry authorizes this app
        bool cancelled = false;   // the user dismissed KWin's interactive pick
    };
    using ShotHandler = std::function<void(const Shot &)>;

    // One ScreenShot2 call, its pixels read off the GUI thread. done runs later on
    // context's thread, never once context is gone.
    static void call(const QString &method, const QVariantList &args, int timeoutMs,
                     QObject *context, ShotHandler done);

    // Raw pixels from the pipe KWin wrote, described by the reply's metadata. Owns fd.
    // Runs on a worker thread, so it never touches QGuiApplication.
    static QImage readImageFromPipe(int fd, const QVariantMap &metadata);

private:
    // logical: where the requested screen sits, known before the call goes out.
    void captureScreen(const QString &method, const QVariantList &args, const QRect &logical);
    void screenDone();

    quint32 m_apiVersion = 0;
    QList<PlacedFrame> m_frames;
    QString m_lastError;
    int m_pending = 0;
    bool m_includeCursor = false;
    bool m_busy = false;
    bool m_failed = false;    // a screen failed, so the grab does too
    bool m_denied = false;
    bool m_cancelled = false;
};

} // namespace Screen

#endif // SCREEN_KWINFRAMESOURCE_H
