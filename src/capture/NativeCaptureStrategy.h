#ifndef CAPTURE_NATIVECAPTURESTRATEGY_H
#define CAPTURE_NATIVECAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QScreen>
#include <QApplication>
#include <QTimer>

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

private slots:
    void onAreaSelected(const QRect &area);

private:
    QPixmap captureScreen();
    QPixmap captureScreenArea();

    QPixmap m_fullScreenshot; // Store for area selection
};

} // namespace Capture

#endif // CAPTURE_NATIVECAPTURESTRATEGY_H
