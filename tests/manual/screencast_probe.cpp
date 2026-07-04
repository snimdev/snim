#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <cstdio>

#include "capture/strategies/ScreencastCaptureStrategy.h"
#include "screen/sources/ScreencastFrameSource.h"

// Manual check, not a ctest: one full-screen ScreenCast capture saved to argv[1].
// The first run shows the portal's picker; a human approves it once, later runs are silent.
static QtMessageHandler s_previous = nullptr;

static void failOnFallback(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    s_previous(type, context, message);
    if (message.contains(QStringLiteral("ScreenCast capture failed"))) {
        std::fprintf(stderr, "probe: the ScreenCast path failed, not testing the fallback\n");
        std::_Exit(2);
    }
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("SNIM_RECORDER_MODULE"))
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);
    s_previous = qInstallMessageHandler(failOnFallback);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Snim"));
    QCoreApplication::setApplicationName(QStringLiteral("snim-screencast-probe"));
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("screencast.png");

    if (!Screen::ScreencastFrameSource::isSupported()) {
        std::fprintf(stderr, "probe: ScreenCast capture is not supported here\n");
        return 3;
    }
    std::fprintf(stderr, "probe: restore token stored: %s\n",
                 Screen::ScreencastFrameSource::hasRestoreToken() ? "yes" : "no");

    Capture::ScreencastCaptureStrategy strategy;
    QElapsedTimer clock;
    QObject::connect(&strategy, &Capture::CaptureStrategy::screenshotReady, &app,
                     [&](const QPixmap &pixmap) {
                         const bool saved = pixmap.save(out);
                         std::fprintf(stderr, "probe: %dx%d in %lld ms, saved=%d to %s\n",
                                      pixmap.width(), pixmap.height(), clock.elapsed(), saved,
                                      qPrintable(out));
                         // Let the session's Close call leave before the process does.
                         QTimer::singleShot(200, &app, [&app, saved] { app.exit(saved ? 0 : 1); });
                     });
    QObject::connect(&strategy, &Capture::CaptureStrategy::screenshotFailed, &app,
                     [&](const QString &error) {
                         std::fprintf(stderr, "probe: failed: %s\n", qPrintable(error));
                         app.exit(1);
                     });
    QTimer::singleShot(90000, &app, [&app] {
        std::fprintf(stderr, "probe: gave up waiting\n");
        app.exit(4);
    });

    clock.start();
    strategy.captureFullScreen();
    return app.exec();
}
