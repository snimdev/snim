#ifndef CAPTURE_FRAMESOURCECAPTURESTRATEGY_H
#define CAPTURE_FRAMESOURCECAPTURESTRATEGY_H

#include "CaptureStrategy.h"
#include "screen/sources/FrameSourceChain.h"

#include <QList>

namespace Capture {

/**
 * Screenshots from the full-desktop frame sources, walked in chain order until one
 * delivers: wlroots screencopy or a remembered ScreenCast session, then the Screenshot
 * portal. Every Wayland path but KWin's. A cancel in a system dialog ends the capture
 * quietly; any other failure passes on to the next source.
 */
class FrameSourceCaptureStrategy : public CaptureStrategy
{
    Q_OBJECT

public:
    // types: the frame sources to try, in order, as StrategySelection::frameSourceChain lists them.
    explicit FrameSourceCaptureStrategy(const QList<Screen::SourceType> &types,
                                        QObject *parent = nullptr);
    // Test seam: the sources come from factory instead of this desktop.
    FrameSourceCaptureStrategy(const QList<Screen::SourceType> &types,
                               Screen::FrameSourceChain::Factory factory, QObject *parent = nullptr);

    void captureFullScreen() override;
    void captureArea() override;
    // Wayland lists no windows: the user draws around the one they want.
    void captureWindow() override;

    [[nodiscard]] QString name() const override;

signals:
    // The system is about to ask which screens to share; lastPickMissedScreens: one was left out.
    void sourcePickerExpected(bool lastPickMissedScreens);

private:
    void grab(bool showSelector);
    void walkFailed(const QString &reason, bool cancelled);

    QList<Screen::SourceType> m_types;
    Screen::FrameSourceChain *m_chain;
    bool m_showSelector = false;
};

} // namespace Capture

#endif // CAPTURE_FRAMESOURCECAPTURESTRATEGY_H
