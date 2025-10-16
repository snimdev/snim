#ifndef CAPTURE_CAPTURESTRATEGY_H
#define CAPTURE_CAPTURESTRATEGY_H

#include <QObject>
#include <QPixmap>
#include <QRect>

namespace Capture {

/**
 * Abstract base class for different screenshot capture strategies
 */
class CaptureStrategy : public QObject
{
    Q_OBJECT

public:
    explicit CaptureStrategy(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~CaptureStrategy() = default;

    // Pure virtual methods that each strategy must implement
    virtual void captureFullScreen() = 0;
    virtual void captureArea() = 0;
    virtual void captureWindow() = 0;

    // Check if this strategy is available on the current system
    virtual bool isAvailable() const = 0;

    // Get strategy name for debugging/logging
    virtual QString name() const = 0;

signals:
    void screenshotReady(const QPixmap &pixmap);
    void screenshotFailed(const QString &error);
};

} // namespace Capture

#endif // CAPTURE_CAPTURESTRATEGY_H
