#include "app/Application.h"
#include "app/CaptureWorkflow.h"
#include "app/RecordingWorkflow.h"
#include "app/SettingsDialog.h"
#include "app/TextSnipWorkflow.h"
#include "app/TrayMenu.h"
#include "app/UploadWorkflow.h"
#include "core/Perf.h"
#include <QAction>
#include <QTimer>
#include <QMessageBox>
#include <QPushButton>

#include "core/UpdateCheck.h"
#include "core/Version.h"
#include "screen/AreaSelector.h"
#ifdef Q_OS_WIN
#include "core/SingleInstance.h"
#endif
#include "hotkeys/GlobalHotkeyManager.h"
#include "hotkeys/HotkeyBindings.h"
#include "ocr/OCRService.h"
#ifdef SNIM_HAVE_LINUX_RECORDER
#include "record/strategies/LinuxRecorderModule.h"
#endif
#include <QDesktopServices>
#include <QUrl>
#include <algorithm>

namespace App {
    namespace {
        bool selectionOverlayVisible() {
            // A stateless widget scan on purpose, not a latch: the overlay's Esc-cancel
            // path emits no completion signal, so a flag would stay stuck.
            const auto widgets = QApplication::topLevelWidgets();
            return std::any_of(widgets.cbegin(), widgets.cend(), [](const QWidget *w) {
                return w->isVisible() && qobject_cast<const Screen::AreaSelector *>(w) != nullptr;
            });
        }
    } // namespace

    Application::Application(int &argc, char **argv)
        : QApplication(argc, argv) {
        setQuitOnLastWindowClosed(false);

        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            QMessageBox::critical(nullptr, "Snim",
                                  "System tray is not available on this system.");
            return;
        }

        m_tray = std::make_unique<TrayMenu>(this);
        m_uploadWorkflow = std::make_unique<UploadWorkflow>(*m_tray, this);
        m_captureWorkflow = std::make_unique<CaptureWorkflow>(*m_tray, *m_uploadWorkflow, this);

        // Initialize text snip capture
        m_textSnipWorkflow = std::make_unique<TextSnipWorkflow>(this);

        m_recordingWorkflow = std::make_unique<RecordingWorkflow>(*m_tray, *m_captureWorkflow,
                                                                  *m_uploadWorkflow, *m_tray, this);

        setupSystemTray();

#ifdef Q_OS_WIN
        // main() lets one Snim run per session; a second launch pings this one instead.
        Core::SingleInstance::listen(this, [this] {
            m_tray->notify(tr("Snim"), tr("Snim is already running"));
        });
#endif

        // Deferred: the backends register against a running event loop.
        QTimer::singleShot(0, this, [this] {
            m_hotkeyManager = std::make_unique<Hotkeys::GlobalHotkeyManager>(this);
            connect(m_hotkeyManager.get(), &Hotkeys::GlobalHotkeyManager::actionTriggered,
                    this, [this](Hotkeys::HotkeyAction action) {
                        if (hotkeyBlockedBySelection(action))
                            return;
                        // trigger() on a disabled action is a no-op, so the OCR /
                        // recorder availability gating carries over unchanged.
                        if (QAction *target = m_tray->actionFor(action))
                            target->trigger();
                    });
            connect(m_hotkeyManager.get(), &Hotkeys::GlobalHotkeyManager::registrationFailed,
                    this, [this](const QString &message) {
                        m_tray->notify(tr("Hotkey unavailable"), message,
                                       QSystemTrayIcon::Warning, 5000);
                    });
            m_hotkeyManager->applyBindings();
            m_tray->refreshShortcutHints();
        });

        // Deferred so startup completes before a dialog blocks it.
        QTimer::singleShot(0, m_recordingWorkflow.get(), &RecordingWorkflow::offerUnsavedRecordings);
        QTimer::singleShot(0, m_captureWorkflow.get(), &CaptureWorkflow::offerStaleEntryRemoval);
    }

    Application::~Application() {
        if (m_tray) {
            m_tray->hide();
        }
    }

    void Application::setupSystemTray() {
        CaptureWorkflow *capture = m_captureWorkflow.get();
        connect(m_tray->captureAreaAction(), &QAction::triggered, capture, &CaptureWorkflow::captureArea);
        connect(m_tray->captureWindowAction(), &QAction::triggered, capture, &CaptureWorkflow::captureWindow);
        connect(m_tray->captureFullScreenAction(), &QAction::triggered, capture, &CaptureWorkflow::captureFullScreen);
        connect(m_tray->textSnipAction(), &QAction::triggered, this, &Application::captureTextSnip);
        connect(m_tray->recordAreaAction(), &QAction::triggered,
                m_recordingWorkflow.get(), &RecordingWorkflow::toggleAreaRecording);
        connect(m_tray->recordWindowAction(), &QAction::triggered,
                m_recordingWorkflow.get(), &RecordingWorkflow::startWindowRecording);

        connect(m_tray->settingsAction(), &QAction::triggered, this, &Application::showSettings);
        connect(m_tray->aboutAction(), &QAction::triggered, this, &Application::showAbout);
        connect(m_tray->checkUpdatesAction(), &QAction::triggered, this, &Application::checkForUpdates);

        connect(m_tray->quitAction(), &QAction::triggered, this, &Application::quit);
        connect(m_tray.get(), &TrayMenu::doubleClicked, capture, &CaptureWorkflow::captureArea);

        m_tray->show();
    }

    bool Application::hotkeyBlockedBySelection(const Hotkeys::HotkeyAction action) const {
        if (!selectionOverlayVisible())
            return false;

        // Record Area doubles as Stop, so it still passes while a recording runs.
        if (action == Hotkeys::HotkeyAction::RecordArea)
            return !(m_recordingWorkflow && m_recordingWorkflow->isRecording());
        return true;
    }

    void Application::showAbout() {
        // Rich text so the website is a clickable link.
        QString aboutText = QStringLiteral("<b>Snim %1</b><br><br>")
                                .arg(QString::fromLatin1(Core::Version::kVersion).toHtmlEscaped())
                           + "A screenshot tool with editing capabilities.<br><br>"
                           "Darko Gjorgjijoski<br>"
                           "<a href=\"https://snim.dev\">snim.dev</a><br><br>"
                           "Shortcuts:";

        bool anyBound = false;
        for (const Hotkeys::HotkeyAction action : Hotkeys::allHotkeyActions()) {
            if (action == Hotkeys::HotkeyAction::OcrTextSnip && !OCR::OCRService::isAvailable())
                continue;
            const QKeySequence seq = Hotkeys::HotkeyBindings::sequence(action);
            if (seq.isEmpty())
                continue;
            aboutText += "<br>• " + Hotkeys::hotkeyActionDescription(action).toHtmlEscaped() + ": "
                         + seq.toString(QKeySequence::NativeText).toHtmlEscaped();
            anyBound = true;
        }
        if (!anyBound)
            aboutText += "<br>• None configured";

        QMessageBox::about(nullptr, "About Snim", aboutText);
    }

    void Application::checkForUpdates() {
        const QString current = QString::fromLatin1(Core::Version::kVersion);

        // Disabled for the duration, so the action cannot queue a second query.
        m_tray->checkUpdatesAction()->setEnabled(false);
        Core::UpdateCheck::checkLatest(this, [this, current](const Core::UpdateCheck::Result &result) {
            m_tray->checkUpdatesAction()->setEnabled(true);

            if (!result.ok) {
                QMessageBox::warning(nullptr, tr("Check for updates"),
                                     tr("Could not check for updates.\n\n%1").arg(result.error));
                return;
            }

            if (!result.newer) {
                QMessageBox::information(nullptr, tr("Check for updates"),
                                         result.error.isEmpty()
                                             ? tr("Snim %1 is up to date.").arg(current)
                                             : result.error);
                return;
            }

            QMessageBox box;
            box.setIcon(QMessageBox::Information);
            box.setWindowTitle(tr("Check for updates"));
            box.setText(tr("Snim %1 is available.").arg(result.latestTag));
            box.setInformativeText(tr("You are running %1.").arg(current));
            QPushButton *openPage = box.addButton(tr("Open release page"), QMessageBox::AcceptRole);
            box.addButton(tr("Later"), QMessageBox::RejectRole);
            box.exec();

            if (box.clickedButton() == openPage && result.releaseUrl.isValid())
                QDesktopServices::openUrl(result.releaseUrl);
        });
    }

    void Application::showSettings() {
        auto *settingsDialog = new SettingsDialog();
        settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, [this] {
            if (m_hotkeyManager)
                m_hotkeyManager->applyBindings();
            m_tray->refreshShortcutHints();
        });
        settingsDialog->exec();
    }

    void Application::captureTextSnip() {
        qDebug() << "Starting text snip with OCR";
        Core::Perf::markCaptureStart("textsnip");
        m_textSnipWorkflow->startTextSnip();
    }

    void Application::quit() {
        // An active recording finalizes first, without opening the editor.
        if (m_recordingWorkflow)
            m_recordingWorkflow->finishBeforeQuit();
        QApplication::quit();
    }

    QList<Core::SelfTest::Check> Application::selfTestChecks() {
        QList<Core::SelfTest::Check> checks;
#ifdef SNIM_HAVE_LINUX_RECORDER
        checks.append({QStringLiteral("recorder"), [](QString *detail) {
            const auto check = Record::LinuxRecorderModule::checkElements();
            *detail = check.missing.isEmpty()
                          ? check.found.join(QStringLiteral(", "))
                          : QStringLiteral("missing ") + check.missing.join(QStringLiteral("; "));
            return check.missing.isEmpty();
        }});
#endif
        return checks;
    }

} // namespace App
