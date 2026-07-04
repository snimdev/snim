#ifndef SCREEN_KWINFRAMESOURCE_H
#define SCREEN_KWINFRAMESOURCE_H

#include "DesktopFrameSource.h"

#include <QImage>
#include <QList>
#include <QVariantMap>

namespace Screen {

/**
 * Every screen straight from KWin's org.kde.KWin.ScreenShot2, pixels streamed over a
 * pipe, composited at the highest scale. Never falls back to CaptureInteractive: a
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

    static QVariantMap buildOptions(bool includeCursor = true, bool nativeResolution = true);

    // Raw pixels from the pipe KWin wrote, described by the reply's metadata. Owns fd.
    static QImage readImageFromPipe(int fd, const QVariantMap &metadata);

    // Per-screen images placed by their logical positions, at the highest DPR.
    static QImage compositeScreenImages(const QList<QImage> &images);

private:
    void captureScreen(const QString &method, const QVariantList &args);
    void screenDone();

    quint32 m_apiVersion = 0;
    QList<QImage> m_images;
    QString m_lastError;
    int m_pending = 0;
    bool m_busy = false;
    bool m_denied = false;
    bool m_cancelled = false;
};

} // namespace Screen

#endif // SCREEN_KWINFRAMESOURCE_H
