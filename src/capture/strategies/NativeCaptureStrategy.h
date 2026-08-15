#ifndef CAPTURE_NATIVECAPTURESTRATEGY_H
#define CAPTURE_NATIVECAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QPixmap>

namespace Capture {

/**
 * Native Qt capture through QScreen: macOS, Windows and X11. The area and window pickers
 * draw over every screen grabbed at once.
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

    QString name() const override { return "Native Qt Capture"; }

private:
    QPixmap captureScreen();
    // Freezes every screen, then opens the selector on the next tick.
    void grabAndSelect(bool windowPick);
};

} // namespace Capture

#endif // CAPTURE_NATIVECAPTURESTRATEGY_H
