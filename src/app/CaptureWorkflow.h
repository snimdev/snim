#ifndef APP_CAPTUREWORKFLOW_H
#define APP_CAPTUREWORKFLOW_H

#include "core/DesktopIntegration.h"
#include "editor/annotations/AnnotationSet.h"

#include <QObject>
#include <functional>
#include <memory>

class QAction;
class QPixmap;

namespace Capture {
class CaptureStrategy;
} // namespace Capture

namespace App {

class TrayMenu;
class UploadWorkflow;

/**
 * Mediator for the screenshot flow: drives the capture strategy, opens each shot in
 * the image editor and hands its upload requests to the UploadWorkflow. Also owns the
 * Linux KWin authorization prompt and the tray's desktop-integration action.
 */
class CaptureWorkflow : public QObject
{
    Q_OBJECT

public:
    CaptureWorkflow(TrayMenu &tray, UploadWorkflow &upload, QObject *parent = nullptr);
    ~CaptureWorkflow() override;

    // Is the KWin authorization offer worth making at all? Pure so it can be tested
    // without a live capture; the caller supplies the three live facts.
    [[nodiscard]] static bool kwinPromptApplicable(bool dismissed, Core::DesktopIntegration::Status status,
                                                   bool alreadyShown);

public slots:
    void captureArea() const;
    void captureWindow() const;
    void captureFullScreen() const;
    // Also where a recording's frame edit lands.
    void openImageEditor(const QPixmap &image, const Editor::AnnotationSet &annotations);
    // At startup: a user desktop entry hiding this packaged install's own is offered for removal.
    void offerStaleEntryRemoval();

private slots:
    void onScreenshotReady(const QPixmap &screenshot, const Editor::AnnotationSet &annotations);

private:
    // KWin refused ScreenShot2: offer to install the desktop entry that authorizes it,
    // then resume the pending capture (true retries it, false takes the slow fallback).
    void askForKWinAuthorization(const std::function<void(bool)> &resume);

    // Repairs the desktop entry and reports the outcome: the tray action.
    void runDesktopIntegrationSetup();
    void refreshDesktopIntegrationAction() const;

    UploadWorkflow &m_upload;
    QAction *m_desktopIntegrationAction = nullptr;   // Linux only, hidden once the entry is in place
    bool m_kwinAuthPromptShown = false;      // the offer is made once per run
    std::unique_ptr<Capture::CaptureStrategy> m_captureStrategy;
};

} // namespace App

#endif // APP_CAPTUREWORKFLOW_H
