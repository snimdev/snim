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

    // Whether the area selector should offer quick actions (Edit/Copy/Save toolbar).
    // On for normal capture; the OCR text-snip path turns it off.
    void setQuickActionsEnabled(bool enabled) { m_quickActionsEnabled = enabled; }
    [[nodiscard]] bool quickActionsEnabled() const { return m_quickActionsEnabled; }

signals:
    void screenshotReady(const QPixmap &pixmap);
    void screenshotFailed(const QString &error);

protected:
    bool m_quickActionsEnabled = true;
};

} // namespace Capture

#endif // CAPTURE_CAPTURESTRATEGY_H
