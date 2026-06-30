#ifndef CAPTURE_WAYLANDCAPTURESTRATEGY_H
#define CAPTURE_WAYLANDCAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include <QObject>
#include <QPixmap>
#include <QProcess>

class QTemporaryFile;

namespace Screen {
class PortalFrameSource;
} // namespace Screen

namespace Capture {

/**
 * Wayland capture strategy using XDG Desktop Portal and fallback tools
 */
class WaylandCaptureStrategy : public CaptureStrategy
{
    Q_OBJECT

public:
    explicit WaylandCaptureStrategy(QObject *parent = nullptr);
    ~WaylandCaptureStrategy() override;

    void captureFullScreen() override;
    void captureArea() override;
    void captureWindow() override;

    bool isAvailable() const override;
    QString name() const override { return "Wayland Portal Capture"; }

private slots:
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    bool useFallbackCapture();
    bool hasAvailableFallbackTools() const;
    bool executeScreenshotTool(const QString &tool);
    void cleanupTempFile();

    QTemporaryFile *m_tempFile;
    QProcess *m_fallbackProcess;
    bool m_captureArea;
    Screen::PortalFrameSource *m_portalSource;
};

} // namespace Capture

#endif // CAPTURE_WAYLANDCAPTURESTRATEGY_H
