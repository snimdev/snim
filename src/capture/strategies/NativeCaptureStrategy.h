#ifndef CAPTURE_NATIVECAPTURESTRATEGY_H
#define CAPTURE_NATIVECAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QScreen>
#include <QApplication>
#include <QTimer>
#include <QVector>
#include <QRect>
#include <QList>

namespace Capture {

class AreaSelector;

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
    void teardownSelectors(QList<AreaSelector*> *selectors);
    [[nodiscard]] QPixmap cropSelection(const QRect &area,
                                        const QSharedPointer<OverlayAnnotations> &annotations) const;
    void onCopyRequested(const QRect &area, const QSharedPointer<OverlayAnnotations> &annotations);
    void onSaveRequested(const QRect &area, const QSharedPointer<OverlayAnnotations> &annotations);

    QPixmap m_fullScreenshot; // Store for area selection
    QRect m_virtualGeometry;  // Store virtual desktop geometry
};

} // namespace Capture

#endif // CAPTURE_NATIVECAPTURESTRATEGY_H
