#include "WaylandCaptureStrategy.h"
#include <QApplication>
#include <QWidget>
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

        connectToScreenshotToolSignals();

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
            qDebug() << "Portal interface not available, falling back";
            return useFallbackCapture();
        }

        // Generate a unique token for this request
        m_requestToken = generateSessionToken();

        // Prepare options for the screenshot
        QVariantMap options;
        options["handle_token"] = m_requestToken;

        // Set options to minimize or avoid dialogs
        options["modal"] = false; // Don't make the dialog modal
        options["cursor"] = true; // Include cursor in screenshot

        // For full screen capture, use non-interactive mode
        if (!m_captureArea) {
            options["interactive"] = false; // Force non-interactive for full screen
            // Try to set a specific output/screen to avoid selection dialog
            // This is implementation-specific but might help
            options["output"] = ""; // Empty string might default to current screen
        } else {
            options["interactive"] = true; // Interactive mode for area selection
        }

        // Get the parent window identifier - try both Wayland and X11 formats
        QString parentWindow = "";
        if (QApplication::activeWindow()) {
            // Try Wayland format first
            QString waylandDisplay = QProcessEnvironment::systemEnvironment().value("WAYLAND_DISPLAY");
            if (!waylandDisplay.isEmpty()) {
                parentWindow = QString("wayland:");
            } else {
                // Fallback to X11 format
                parentWindow = QString("x11:%1").arg(static_cast<qulonglong>(QApplication::activeWindow()->winId()));
            }
        }

        qDebug() << "Requesting screenshot via portal with token:" << m_requestToken;
        qDebug() << "Interactive mode:" << options["interactive"].toBool();
        qDebug() << "Parent window:" << parentWindow;

        // Call the Screenshot method asynchronously
        QDBusPendingCall pendingCall = m_portalInterface->asyncCall(
            "Screenshot",
            parentWindow,
            QVariant::fromValue(options)
        );

        const auto *watcher = new QDBusPendingCallWatcher(pendingCall, this);
        connect(watcher, &QDBusPendingCallWatcher::finished,
                this, &WaylandCaptureStrategy::onPortalResponse);

        return true;
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

    void WaylandCaptureStrategy::onPortalResponse(QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            qDebug() << "Portal screenshot request failed:" << reply.error().message();
            emit screenshotFailed("Portal request failed: " + reply.error().message());
            return;
        }

        // The reply contains the object path for the Request
        QDBusObjectPath requestPath = reply.value();
        qDebug() << "Portal request created at:" << requestPath.path();

        // The actual response will come via the Response signal
        // which is handled by connectToPortalSignals()
    }

    void WaylandCaptureStrategy::handleScreenshotResponse(uint response, const QVariantMap &results) {
        qDebug() << "Portal screenshot response:" << response << results;

        if (response != 0) {
            // User cancelled or error occurred
            emit screenshotFailed("Screenshot was cancelled or failed");
            return;
        }

        // Extract the URI from the results
        if (!results.contains("uri")) {
            emit screenshotFailed("No screenshot URI in portal response");
            return;
        }

        QString uri = results["uri"].toString();
        qDebug() << "Screenshot saved to:" << uri;

        // Convert URI to local path and load the image
        QUrl url(uri);
        QString localPath = url.toLocalFile();

        if (localPath.isEmpty()) {
            emit screenshotFailed("Invalid screenshot URI");
            return;
        }

        QPixmap screenshot(localPath);
        if (screenshot.isNull()) {
            emit screenshotFailed("Failed to load screenshot from " + localPath);
            return;
        }

        emit screenshotReady(screenshot);

        // Clean up the temporary file created by the portal
        QFile::remove(localPath);
    }


    void WaylandCaptureStrategy::onScreenshotReady(const QPixmap &screenshot) {
        // Simply show the screenshot dialog - the strategy handles all the complexity
        showScreenshotDialog(screenshot);
    }

    void WaylandCaptureStrategy::onScreenshotFailed(const QString &error) {
        QMessageBox::warning(nullptr, "Screenshot Failed",
                             QString("Failed to capture screenshot: %1").arg(error));
    }


    void WaylandCaptureStrategy::showScreenshotDialog(const QPixmap &screenshot) {
        auto *dialog = new Core::ScreenshotDialog(screenshot);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->exec();
    }


    void WaylandCaptureStrategy::connectToScreenshotToolSignals() {
        // Connect strategy signals
        connect(this, &Capture::WaylandCaptureStrategy::screenshotReady, this,
                &WaylandCaptureStrategy::onScreenshotReady);
        connect(this, &Capture::WaylandCaptureStrategy::screenshotFailed, this,
                &WaylandCaptureStrategy::onScreenshotFailed);
    }

    void WaylandCaptureStrategy::connectToPortalSignals() {
        if (!m_sessionBus.isConnected()) {
            qWarning() << "D-Bus session bus not connected";
            return;
        }

        // Connect to the Response signal for screenshot requests
        m_sessionBus.connect(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(handleScreenshotResponse(uint, QVariantMap))
        );
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
