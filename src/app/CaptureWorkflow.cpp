#include "app/CaptureWorkflow.h"
#include "app/TrayMenu.h"
#include "app/UploadWorkflow.h"
#include "capture/CaptureFactory.h"
#include "capture/strategies/CaptureStrategy.h"
#include "core/Perf.h"
#include "core/Settings.h"
#include "editor/image/ImageEditor.h"
#ifdef Q_OS_LINUX
#include "capture/strategies/KWinCaptureStrategy.h"
#endif

#include <QAction>
#include <QDebug>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>

namespace App {

    CaptureWorkflow::CaptureWorkflow(TrayMenu &tray, UploadWorkflow &upload, QObject *parent)
        : QObject(parent)
          , m_upload(upload)
          , m_desktopIntegrationAction(tray.desktopIntegrationAction()) {
        // Initialize capture strategy using factory
        m_captureStrategy = Capture::CaptureFactory::createStrategy(
                Capture::CaptureFactory::StrategyType::Auto,
                this
            );

        connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotReady, this, &CaptureWorkflow::onScreenshotReady);
        connect(m_captureStrategy.get(), &Capture::CaptureStrategy::screenshotFailed, this, [](const QString &error) {
            QMessageBox::warning(nullptr, "Screenshot Failed", error);
        });

#ifdef Q_OS_LINUX
        // The strategy launches no fallback while this gate is installed: the answer
        // decides, so the prompt never competes with the portal's fullscreen picker.
        if (auto *kwin = qobject_cast<Capture::KWinCaptureStrategy *>(m_captureStrategy.get()))
            kwin->setAuthorizationGate([this](const std::function<void(bool)> &resume) {
                askForKWinAuthorization(resume);
            });

        connect(m_desktopIntegrationAction, &QAction::triggered,
                this, &CaptureWorkflow::runDesktopIntegrationSetup);
        connect(tray.menu(), &QMenu::aboutToShow, this, &CaptureWorkflow::refreshDesktopIntegrationAction);
        refreshDesktopIntegrationAction();
#endif
    }

    CaptureWorkflow::~CaptureWorkflow() = default;

    void CaptureWorkflow::captureArea() const {
        qDebug() << "Capture area using strategy:" << m_captureStrategy->name();
        Core::Perf::markCaptureStart("area");

        // Use the strategy to capture area
        m_captureStrategy->captureArea();
    }

    void CaptureWorkflow::captureWindow() const {
        qDebug() << "Capture window using strategy:" << m_captureStrategy->name();
        Core::Perf::markCaptureStart("window");

        // Use the strategy to capture window
        m_captureStrategy->captureWindow();
    }

    void CaptureWorkflow::captureFullScreen() const {
        qDebug() << "Capture full screen using strategy:" << m_captureStrategy->name();
        Core::Perf::markCaptureStart("fullscreen");

        // The strategy emits screenshotReady, so this joins the normal editor flow.
        m_captureStrategy->captureFullScreen();
    }

    bool CaptureWorkflow::kwinPromptApplicable(const bool dismissed,
                                               const Core::DesktopIntegration::Status status,
                                               const bool alreadyShown,
                                               const bool fromAppImage) {
        if (dismissed)
            return false;
        // No desktop entry can point at a mount path that is gone by the next launch.
        if (fromAppImage)
            return false;
        // Entry already correct: the refusal has another cause, so offering a rewrite helps nobody.
        if (status == Core::DesktopIntegration::Status::Installed)
            return false;
        return !alreadyShown;
    }

    void CaptureWorkflow::askForKWinAuthorization(const std::function<void(bool)> &resume) {
        const bool fromAppImage = Core::DesktopIntegration::runningFromAppImage();
        if (!kwinPromptApplicable(Core::Settings::desktopIntegrationPromptDismissed(),
                                  Core::DesktopIntegration::status(), m_kwinAuthPromptShown,
                                  fromAppImage)) {
            // Only when the AppImage is the reason, and only once: there is nothing to offer.
            if (fromAppImage && !m_kwinAuthPromptShown) {
                m_kwinAuthPromptShown = true;
                qInfo() << "KWin matches the caller's real executable path, which an AppImage "
                           "remounts at a new path every launch, so the fast path needs the "
                           "extracted install";
            }
            resume(false);
            return;
        }
        m_kwinAuthPromptShown = true;

        QMessageBox box;
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("Enable fast screenshots"));
        box.setText(tr("KDE needs Snim's desktop entry to be registered before it allows "
                       "instant, dialog-free captures."));
        box.setInformativeText(tr("Without it every capture goes through the slower picker "
                                  "dialog. Snim can set this up now; it only writes a desktop "
                                  "entry and an icon into your local applications folder. "
                                  "Set up now and this capture will retry instantly."));
        QPushButton *setUp = box.addButton(tr("Set up now"), QMessageBox::AcceptRole);
        box.addButton(tr("Not now"), QMessageBox::RejectRole);
        QPushButton *never = box.addButton(tr("Never ask again"), QMessageBox::DestructiveRole);
        box.setDefaultButton(setUp);
        // The capture is still pending behind this box, so nudge it to the front.
        box.show();
        box.raise();
        box.activateWindow();
        box.exec();

        if (box.clickedButton() == setUp) {
            QString error;
            const bool installed = Core::DesktopIntegration::install(&error);
            refreshDesktopIntegrationAction();
            if (!installed) {
                QMessageBox::warning(nullptr, tr("Desktop integration"),
                                     tr("Could not set up the desktop entry:\n%1").arg(error));
            }
            // No confirmation box on success: the retried capture is the confirmation.
            resume(installed);
            return;
        }

        if (box.clickedButton() == never)
            Core::Settings::setDesktopIntegrationPromptDismissed(true);
        resume(false);
    }

    void CaptureWorkflow::runDesktopIntegrationSetup() {
        QString error;
        if (Core::DesktopIntegration::install(&error)) {
            QMessageBox::information(nullptr, tr("Desktop integration"),
                                     Core::DesktopIntegration::runningFromAppImage()
                                         ? tr("Done. Snim is in the application menu.")
                                         : tr("Done. The next capture uses the fast path."));
        } else {
            QMessageBox::warning(nullptr, tr("Desktop integration"),
                                 tr("Could not set up the desktop entry:\n%1").arg(error));
        }
        refreshDesktopIntegrationAction();
    }

    void CaptureWorkflow::refreshDesktopIntegrationAction() const {
        if (!m_desktopIntegrationAction)
            return;
        m_desktopIntegrationAction->setVisible(
            Core::DesktopIntegration::status() != Core::DesktopIntegration::Status::Installed);
    }

    void CaptureWorkflow::onScreenshotReady(const QPixmap &screenshot,
                                            const Editor::AnnotationSet &annotations) {
        // Fullscreen shows no overlay, so this is its visible-endpoint; area/window already reported.
        Core::Perf::reportCaptureShown("frame ready");
        qDebug() << "Screenshot ready, opening ImageEditor";
        openImageEditor(screenshot, annotations);
    }

    void CaptureWorkflow::openImageEditor(const QPixmap &image,
                                          const Editor::AnnotationSet &annotations) {
        auto *editor = new Editor::Image::ImageEditor(image);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        connect(editor, &Editor::Image::ImageEditor::uploadRequested,
                &m_upload, &UploadWorkflow::startUpload);
        if (!annotations.isEmpty())
            editor->importAnnotations(annotations);
        editor->show();
        editor->raise();
        editor->activateWindow();
    }

} // namespace App
