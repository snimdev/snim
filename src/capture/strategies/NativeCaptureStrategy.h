#ifndef CAPTURE_NATIVECAPTURESTRATEGY_H
#define CAPTURE_NATIVECAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include "capture/CaptureGeometry.h"
#include <QScreen>
#include <QApplication>
#include <QTimer>
#include <QVector>
#include <QRect>
#include <QList>
#include <utility>

namespace Screen {
class AreaSelector;
} // namespace Screen

namespace Capture {

/**
 * Native Qt capture strategy using QScreen for X11/traditional systems
 */
class NativeCaptureStrategy : public CaptureStrategy
{
    Q_OBJECT

public:
    explicit NativeCaptureStrategy(QObject *parent = nullptr);
    ~NativeCaptureStrategy() override = default;

    void captureFullScreen() override;
    void captureArea() override;
    void captureWindow() override;

    bool isAvailable() const override;
    QString name() const override { return "Native Qt Capture"; }

private:
    void onAreaSelected(const QRect &area, const QSharedPointer<OverlayAnnotations> &annotations);
    QPixmap captureScreen();
    QPixmap captureAllScreens();
    void showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry,
                          bool windowPick = false, const QVector<QRect> &windows = {});
    void onCopyRequested(const QRect &area, const QSharedPointer<OverlayAnnotations> &annotations);
    void onSaveRequested(const QRect &area, const QSharedPointer<OverlayAnnotations> &annotations);
    // The frame and its geometry to crop area from: one screen's own grab when it holds area.
    [[nodiscard]] std::pair<QPixmap, QRect> cropSource(const QRect &area) const;
    void clearFrames();

    QPixmap m_fullScreenshot; // Store for area selection
    QRect m_virtualGeometry;  // Store virtual desktop geometry
    QList<ScreenGrab> m_screenGrabs; // Windows only: each screen's own grab
};

} // namespace Capture

#endif // CAPTURE_NATIVECAPTURESTRATEGY_H
