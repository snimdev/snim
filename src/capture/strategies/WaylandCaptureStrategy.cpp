#include "WaylandCaptureStrategy.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QWindow>

#include "screen/AreaSelector.h"
#include "capture/CaptureGeometry.h"
#include "screen/sources/PortalFrameSource.h"

namespace Capture {
    WaylandCaptureStrategy::WaylandCaptureStrategy(QObject *parent)
        : CaptureStrategy(parent)
          , m_tempFile(nullptr)
          , m_fallbackProcess(nullptr)
          , m_captureArea(false)
          , m_portalSource(new Screen::PortalFrameSource(this)) {
        // Both full screen and area land in the selector on this path.
        connect(m_portalSource, &Screen::DesktopFrameSource::frameReady, this,
                [this](const QPixmap &frame, const QRect &virtualGeometry) {
                    showAreaSelector(frame, virtualGeometry);
                });
        connect(m_portalSource, &Screen::DesktopFrameSource::frameFailed, this,
                [this](const QString &reason, bool) {
                    qDebug() << "Portal screenshot failed:" << reason;
                    emit screenshotFailed(QStringLiteral("Portal screenshot failed: %1").arg(reason));
                });
    }

    WaylandCaptureStrategy::~WaylandCaptureStrategy() {
        cleanupTempFile();
        if (m_fallbackProcess) {
            m_fallbackProcess->kill();
            m_fallbackProcess->waitForFinished(3000);
            m_fallbackProcess->deleteLater();
        }
    }

    void WaylandCaptureStrategy::captureFullScreen() {
        m_captureArea = false;

        // For full screen capture, prefer fallback tools over portal
        // to avoid permission dialogs completely
        if (useFallbackCapture()) {
            return; // Success with fallback tool
        }

        qDebug() << "No fallback tools available, using portal";

        // Only use portal if no fallback tools are available
        if (Screen::PortalFrameSource::isPortalReachable()) {
            m_portalSource->grab();
        } else {
            emit screenshotFailed(
                "No screenshot method available. Please install spectacle, grim, or gnome-screenshot.");
        }
    }

    void WaylandCaptureStrategy::captureArea() {
        m_captureArea = true;

        // For area capture, go straight to portal (fallback tools like
        // spectacle open their own editor instead of returning the image).

        if (Screen::PortalFrameSource::isPortalReachable()) {
            m_portalSource->grab();
        } else {
            emit screenshotFailed("No screenshot method available for area capture.");
        }
    }

    void WaylandCaptureStrategy::captureWindow() {
        // Use the same method as full screen for now
        captureFullScreen();
    }

    bool WaylandCaptureStrategy::isAvailable() const {
        return Screen::isWaylandSession()
               && (Screen::PortalFrameSource::isPortalReachable() || hasAvailableFallbackTools());
    }

    // Every terminal action (accept, cancel, copy, save) tears down ALL per-screen
    // overlays first, so nothing is left covering the screen or the save dialog.
    static void tearDownSelectors(QList<Screen::AreaSelector*> *selectors) {
        for (auto *sel : *selectors) {
            sel->blockSignals(true);  // Block any further signals
            sel->disconnect();         // Disconnect all signals
            sel->close();              // Close immediately
            sel->deleteLater();        // Schedule for deletion
        }

        // Clear the list and delete it
        selectors->clear();
        delete selectors;
    }

    void WaylandCaptureStrategy::showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry) {
        qDebug() << "Virtual desktop geometry:" << virtualGeometry;
        qDebug() << "Screenshot size:" << screenshot.size() << "DPR:" << screenshot.devicePixelRatio();
        qDebug() << "All screens:";

        QList<QScreen*> screens = QGuiApplication::screens();
        QList<Screen::AreaSelector*> *selectors = new QList<Screen::AreaSelector*>();

        // Create one AreaSelector widget per screen
        for (QScreen *screen : screens) {
            QRect screenGeometry = screen->geometry();
            qDebug() << "  - Creating selector for" << screen->name() << screenGeometry;

            auto *selector = new Screen::AreaSelector();
            selector->setScreenshot(screenshot);
            selector->setVirtualGeometry(virtualGeometry);
            selector->setScreenOffset(screenGeometry.topLeft());
            selector->setActionsEnabled(quickActionsEnabled());

            // Position the selector on this specific screen
            selector->setGeometry(screenGeometry);
            selector->setWindowState(Qt::WindowFullScreen);
            selector->winId(); // ensure the native window exists before placing it
            if (QWindow *wh = selector->windowHandle())
                wh->setScreen(screen);
            selector->showFullScreen();

            selectors->append(selector);
        }

        const auto annotations = attachAnnotations(*selectors, screenshot, virtualGeometry);

        // Connect all selectors to the same handler - use a shared pointer approach
        for (auto *selector : *selectors) {
            // Each handler copies the session first: teardown disconnects its own lambda.
            connect(selector, &Screen::AreaSelector::areaSelected,
                    this, [this, selectors, screenshot, virtualGeometry, annotations](const QRect &area) {
                        const auto session = annotations;
                        // Tear every overlay down first, to prevent multiple triggers
                        tearDownSelectors(selectors);

                        if (area.isEmpty()) {
                            // User cancelled (pressed Escape)
                            qDebug() << "Area selection cancelled";
                            //emit screenshotCancelled();
                            return;
                        }

                        // The area is in virtual desktop logical coordinates
                        qDebug() << "Selected area (logical):" << area;
                        qDebug() << "Physical area (screenshot coords):"
                                 << physicalCropRect(area, virtualGeometry,
                                                     screenshot.devicePixelRatio(), screenshot.size());

                        emitSelection(screenshot, virtualGeometry, area, session);
                    });
            connect(selector, &Screen::AreaSelector::copyRequested,
                    this, [this, selectors, screenshot, virtualGeometry, annotations](const QRect &area) {
                        const auto session = annotations;
                        tearDownSelectors(selectors);
                        copyAreaToClipboard(screenshot, virtualGeometry, area, session);
                    });
            connect(selector, &Screen::AreaSelector::saveRequested,
                    this, [this, selectors, screenshot, virtualGeometry, annotations](const QRect &area) {
                        const auto session = annotations;
                        // Teardown first, or the save dialog opens behind the overlay.
                        tearDownSelectors(selectors);
                        saveAreaToFile(screenshot, virtualGeometry, area, session);
                    });
        }
    }


    bool WaylandCaptureStrategy::hasAvailableFallbackTools() const {
        return Screen::PortalFrameSource::hasFallbackTool(m_captureArea);
    }

    bool WaylandCaptureStrategy::useFallbackCapture() {
        // Try using external tools as fallback
        QStringList candidates;

        candidates << "spectacle" << "grim" << "gnome-screenshot" << "flameshot";

        for (const QString &tool: candidates) {
            QString toolPath = QStandardPaths::findExecutable(tool);
            if (!toolPath.isEmpty()) {
                return executeScreenshotTool(tool);
            }
        }

        // Return false without emitting error - let caller handle fallback
        return false;
    }

    bool WaylandCaptureStrategy::executeScreenshotTool(const QString &tool) {
        cleanupTempFile();
        m_tempFile = new QTemporaryFile(QDir::tempPath() + "/screenshot_XXXXXX.png", this);
        if (!m_tempFile->open()) {
            emit screenshotFailed("Failed to create temporary file");
            return false;
        }

        qDebug() << "Executing screenshot tool:" << tool;

        if (m_fallbackProcess) {
            m_fallbackProcess->deleteLater();
        }

        m_fallbackProcess = new QProcess(this);
        connect(
            m_fallbackProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &WaylandCaptureStrategy::processFinished
        );

        // Handle different tools with proper arguments
        if (tool == "spectacle") {
            QStringList args;
            if (m_captureArea) {
                args << "-r" << "-n" << "-o" << m_tempFile->fileName();
            } else {
                args << "-b -S -m -s -n" << "-o" << m_tempFile->fileName();
            }
            qDebug() << "Spectacle args:" << args;
            m_fallbackProcess->start(tool, args);
        } else if (tool == "grim") {
            if (m_captureArea) {
                // For grim with slurp, we need to use shell execution
                QString command = QString("grim -g \"$(slurp)\" \"%1\"").arg(m_tempFile->fileName());
                qDebug() << "Grim with slurp command:" << command;
                m_fallbackProcess->start("/bin/sh", QStringList() << "-c" << command);
            } else {
                QStringList args;
                args << m_tempFile->fileName();
                qDebug() << "Grim args:" << args;
                m_fallbackProcess->start(tool, args);
            }
        } else if (tool == "gnome-screenshot") {
            QStringList args;
            if (m_captureArea) {
                args << "--area" << "--file=" + m_tempFile->fileName();
            } else {
                args << "--file=" + m_tempFile->fileName();
            }
            qDebug() << "Gnome-screenshot args:" << args;
            m_fallbackProcess->start(tool, args);
        } else if (tool == "flameshot") {
            QStringList args;
            if (m_captureArea) {
                args << "gui" << "--path" << QFileInfo(m_tempFile->fileName()).path()
                        << "--filename" << QFileInfo(m_tempFile->fileName()).baseName();
            } else {
                args << "full" << "--path" << m_tempFile->fileName();
            }
            qDebug() << "Flameshot args:" << args;
            m_fallbackProcess->start(tool, args);
        } else {
            emit screenshotFailed("Unknown screenshot tool: " + tool);
            return false;
        }

        return true;
    }

    void WaylandCaptureStrategy::processFinished(int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "Process finished with exit code:" << exitCode << "status:" << exitStatus;

        if (m_fallbackProcess) {
            QString errorOutput = m_fallbackProcess->readAllStandardError();
            QString standardOutput = m_fallbackProcess->readAllStandardOutput();

            if (!errorOutput.isEmpty()) {
                qDebug() << "Process stderr:" << errorOutput;
            }
            if (!standardOutput.isEmpty()) {
                qDebug() << "Process stdout:" << standardOutput;
            }
        }

        if (exitCode == 0 && m_tempFile && QFile::exists(m_tempFile->fileName())) {
            QPixmap screenshot(m_tempFile->fileName());
            if (!screenshot.isNull()) {
                qDebug() << "Screenshot captured successfully with fallback tool";
                emit screenshotReady(screenshot);
            } else {
                qDebug() << "Failed to load screenshot from file:" << m_tempFile->fileName();
                emit screenshotFailed("Failed to load screenshot from file");
            }
        } else {
            QString errorMsg = QString("Screenshot tool failed or was cancelled (exit code: %1)").arg(exitCode);
            if (m_tempFile && !QFile::exists(m_tempFile->fileName())) {
                errorMsg += " - output file was not created";
            }
            qDebug() << errorMsg;
            emit screenshotFailed(errorMsg);
        }

        cleanupTempFile();
    }

    void WaylandCaptureStrategy::cleanupTempFile() {
        if (m_tempFile) {
            m_tempFile->deleteLater();
            m_tempFile = nullptr;
        }
    }
} // namespace Capture
