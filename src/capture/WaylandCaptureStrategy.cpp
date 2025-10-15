#include "WaylandCaptureStrategy.h"

#include <iostream>
#include <QApplication>
#include <QUuid>
#include <QScreen>
#include <QWidget>
#include <QWindow>
#include <QCursor>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingReply>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QProcessEnvironment>
#include <QDateTime>
#include <QRandomGenerator>
#include <QUrl>
#include <QFile>
#include <QMessageBox>

#include "AreaSelector.h"
#include "core/ScreenshotDialog.h"

namespace Capture {
    WaylandCaptureStrategy::WaylandCaptureStrategy(QObject *parent)
        : CaptureStrategy(parent)
          , m_portalInterface(nullptr)
          , m_sessionInterface(nullptr)
          , m_tempFile(nullptr)
          , m_fallbackProcess(nullptr)
          , m_captureArea(false)
          , m_sessionBus(QDBusConnection::sessionBus()) {
        // Initialize XDG Desktop Portal interface
        m_portalInterface = new QDBusInterface(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Screenshot",
            m_sessionBus,
            this
        );

        connectToPortalSignals();
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
        if (isPortalAvailable()) {
            usePortalCapture();
        } else {
            emit screenshotFailed(
                "No screenshot method available. Please install spectacle, grim, or gnome-screenshot.");
        }
    }

    void WaylandCaptureStrategy::captureArea() {
        m_captureArea = true;

        // For area capture, try fallback tools first, then portal
        //if (useFallbackCapture()) {
        //    return; // Success with fallback tool
        //}

        // Use portal for area selection if fallback failed
        if (isPortalAvailable()) {
            usePortalCapture();
        } else {
            emit screenshotFailed("No screenshot method available for area capture.");
        }
    }

    void WaylandCaptureStrategy::captureWindow() {
        // Use the same method as full screen for now
        captureFullScreen();
    }

    bool WaylandCaptureStrategy::isAvailable() const {
        return isWaylandSession() && (isPortalAvailable() || hasAvailableFallbackTools());
    }

    bool WaylandCaptureStrategy::isWaylandSession() {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString sessionType = env.value("XDG_SESSION_TYPE");
        QString waylandDisplay = env.value("WAYLAND_DISPLAY");

        return sessionType == "wayland" || !waylandDisplay.isEmpty();
    }

    bool WaylandCaptureStrategy::isPortalAvailable() const {
        if (!m_portalInterface || !m_portalInterface->isValid()) {
            return false;
        }

        // Check if the Screenshot portal is available
        QDBusInterface portalChecker(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.DBus.Properties",
            QDBusConnection::sessionBus()
        );

        return portalChecker.isValid();
    }

    bool WaylandCaptureStrategy::usePortalCapture() {
        if (!m_portalInterface || !m_portalInterface->isValid()) {
            qDebug() << "Portal interface not available";
            return useFallbackCapture();
        }

        // Generate unique token
        QString token = QUuid::createUuid().toString()
                .remove('-').remove('{').remove('}');

        // Pre-create Request interface to avoid race condition
        QString requestPath = "/org/freedesktop/portal/desktop/request/" +
                              QDBusConnection::sessionBus().baseService()
                              .remove(':').replace('.', '_') + "/" + token;

        QDBusInterface requestInterface(
            "org.freedesktop.portal.Desktop",
            requestPath,
            "org.freedesktop.portal.Request",
            QDBusConnection::sessionBus()
        );

        if (!requestInterface.isValid()) {
            qDebug() << "Failed to create request interface";
            return false;
        }

        // Setup event loop for synchronous operation
        QEventLoop loop;
        QPixmap result;
        bool success = false;

        // Connect Response signal BEFORE making the call
        QDBusConnection::sessionBus().connect(
            "org.freedesktop.portal.Desktop",
            requestPath,
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(handlePortalResponse(uint, QVariantMap))
        );

        // Prepare options
        QVariantMap options;
        options["handle_token"] = token;
        options["interactive"] = false; // Minimize dialogs
        options["modal"] = false;

        // Call Screenshot with empty parent window
        QDBusReply<QDBusObjectPath> reply = m_portalInterface->call(
            "Screenshot",
            QString(""), // Empty parent window
            QVariant::fromValue(options)
        );

        if (reply.isValid()) {
            qDebug() << "Screenshot call failed:" << reply.error().message();
            return false;
        }

        // Wait for response (implement timeout if needed)
        // Process the response in handlePortalResponse()

        return true;
    }

    void WaylandCaptureStrategy::handlePortalResponse(
        uint status,
        QVariantMap results) {
        if (status != 0) {
            qDebug() << "Portal request cancelled or failed";
            emit screenshotFailed("Portal request cancelled or failed");
            return;
        }

        QUrl uri = results["uri"].toString();
        QString filePath = uri.toLocalFile();

        QImage fullImage(filePath);
        if (fullImage.isNull()) {
            qDebug() << "Failed to load screenshot";
            QFile::remove(filePath);
            emit screenshotFailed("Failed to load screenshot");
            return;
        }

        // Calculate virtual desktop geometry and DPR
        QList<QScreen*> screens = QGuiApplication::screens();
        QRect virtualDesktop;
        for (QScreen *screen : screens) {
            virtualDesktop = virtualDesktop.united(screen->geometry());
        }

        // Calculate DPR from image size vs logical size
        qreal dprX = fullImage.width() * 1.0 / virtualDesktop.width();
        qreal dprY = fullImage.height() * 1.0 / virtualDesktop.height();
        qreal dpr = qMax(dprX, dprY);

        qDebug() << "Virtual desktop (logical):" << virtualDesktop;
        qDebug() << "Full image size (physical):" << fullImage.size();
        qDebug() << "Calculated DPR:" << dpr;

        // Convert to pixmap and set DPR
        QPixmap fullScreenshot = QPixmap::fromImage(fullImage);
        fullScreenshot.setDevicePixelRatio(dpr);

        QFile::remove(filePath);

        // Show area selector spanning all monitors
        showAreaSelector(fullScreenshot, virtualDesktop);
    }

    void WaylandCaptureStrategy::showAreaSelector(const QPixmap &screenshot, const QRect &virtualGeometry) {
        qDebug() << "Virtual desktop geometry:" << virtualGeometry;
        qDebug() << "Screenshot size:" << screenshot.size() << "DPR:" << screenshot.devicePixelRatio();
        qDebug() << "All screens:";

        QList<QScreen*> screens = QGuiApplication::screens();
        QList<Capture::AreaSelector*> selectors;

        // Create one AreaSelector widget per screen
        for (QScreen *screen : screens) {
            QRect screenGeometry = screen->geometry();
            qDebug() << "  - Creating selector for" << screen->name() << screenGeometry;

            auto *selector = new Capture::AreaSelector();
            selector->setScreenshot(screenshot);
            selector->setVirtualGeometry(virtualGeometry);
            selector->setScreenOffset(screenGeometry.topLeft());

            // Position the selector on this specific screen
            selector->setGeometry(screenGeometry);
            selector->setWindowState(Qt::WindowFullScreen);
            selector->windowHandle()->setScreen(screen);
            selector->showFullScreen();

            selectors.append(selector);

            // Connect to area selection - all selectors share the same signal
            connect(selector, &Capture::AreaSelector::areaSelected,
                    this, [this, selectors, screenshot, virtualGeometry](const QRect &area) {
                        // Close all selectors
                        for (auto *sel : selectors) {
                            sel->deleteLater();
                        }

                        if (area.isEmpty()) {
                            // User cancelled (pressed Escape)
                            qDebug() << "Area selection cancelled";
                            //emit screenshotCancelled();
                            return;
                        }

                        // The area is in virtual desktop logical coordinates
                        qreal dpr = screenshot.devicePixelRatio();

                        // Map from virtual desktop logical coordinates to screenshot physical coordinates
                        QRect physicalArea(
                            (area.x() - virtualGeometry.x()) * dpr,
                            (area.y() - virtualGeometry.y()) * dpr,
                            area.width() * dpr,
                            area.height() * dpr
                        );

                        // Ensure the crop rect is within bounds
                        physicalArea = physicalArea.intersected(screenshot.rect());

                        qDebug() << "Selected area (logical):" << area;
                        qDebug() << "Physical area (screenshot coords):" << physicalArea;

                        QPixmap finalScreenshot = screenshot.copy(physicalArea);
                        finalScreenshot.setDevicePixelRatio(dpr);

                        qDebug() << "Final screenshot size:" << finalScreenshot.size();

                        emit screenshotReady(finalScreenshot);
                    });
        }
    }


    QPixmap WaylandCaptureStrategy::cropToCurrentScreen(const QImage &fullImage) {
        // Get current screen based on mouse cursor position
        QScreen *currentScreen = nullptr;
        QPoint cursorPos = QCursor::pos();

        // Find which screen contains the cursor
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->geometry().contains(cursorPos)) {
                currentScreen = screen;
                break;
            }
        }

        // Fallback to active window's screen
        if (!currentScreen && QApplication::activeWindow()) {
            currentScreen = QApplication::activeWindow()->screen();
        }

        // Final fallback to primary screen
        if (!currentScreen) {
            currentScreen = QGuiApplication::primaryScreen();
        }

        // Calculate virtual desktop geometry
        QList<QScreen*> screens = QGuiApplication::screens();
        QRect virtualDesktop;
        for (QScreen *screen : screens) {
            virtualDesktop = virtualDesktop.united(screen->geometry());
        }

        // Calculate DPR from image size vs logical size
        qreal dprX = fullImage.width() * 1.0 / virtualDesktop.width();
        qreal dprY = fullImage.height() * 1.0 / virtualDesktop.height();
        qreal dpr = qMax(dprX, dprY);

        qDebug() << "Virtual desktop (logical):" << virtualDesktop;
        qDebug() << "Full image size (physical):" << fullImage.size();
        qDebug() << "Calculated DPR:" << dpr;
        qDebug() << "Current screen:" << currentScreen->name()
                 << currentScreen->geometry();

        // Get current screen geometry relative to virtual desktop
        QRect screenLogical = currentScreen->geometry();
        QPoint offset = screenLogical.topLeft() - virtualDesktop.topLeft();

        // Convert to physical coordinates
        QRect cropRect(
            offset.x() * dpr,
            offset.y() * dpr,
            screenLogical.width() * dpr,
            screenLogical.height() * dpr
        );

        // Ensure crop rect is within image bounds
        cropRect = cropRect.intersected(fullImage.rect());

        if (cropRect.isEmpty()) {
            qDebug() << "Invalid crop rectangle";
            return QPixmap();
        }

        qDebug() << "Cropping to:" << cropRect;

        // Crop the image
        QImage cropped = fullImage.copy(cropRect);

        // Convert to pixmap and set DPR
        QPixmap result = QPixmap::fromImage(cropped);
        result.setDevicePixelRatio(dpr);

        return result;
    }

    bool WaylandCaptureStrategy::hasAvailableFallbackTools() const {
        // Check if any fallback tools are available without executing them
        QStringList candidates;

        if (m_captureArea) {
            candidates << "spectacle" << "flameshot" << "gnome-screenshot";
        } else {
            candidates << "spectacle" << "grim" << "gnome-screenshot" << "flameshot";
        }

        for (const QString &tool: candidates) {
            QString toolPath = QStandardPaths::findExecutable(tool);
            if (!toolPath.isEmpty()) {
                return true;
            }
        }

        return false;
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

    void WaylandCaptureStrategy::connectToPortalSignals() {
        if (!m_sessionBus.isConnected()) {
            qWarning() << "D-Bus session bus not connected";
            return;
        }
    }

    QString WaylandCaptureStrategy::generateSessionToken() {
        // Generate a unique token for the portal request
        const quint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        const quint32 random = QRandomGenerator::global()->generate();
        return QString("niceshot_%1_%2").arg(timestamp).arg(random);
    }

    void WaylandCaptureStrategy::cleanupTempFile() {
        if (m_tempFile) {
            m_tempFile->deleteLater();
            m_tempFile = nullptr;
        }
    }
} // namespace Capture
