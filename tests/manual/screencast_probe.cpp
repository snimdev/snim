#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <cstdio>

#include "screen/sources/ScreencastFrameSource.h"

// Manual check, not a ctest: one full-desktop frame from the ScreenCast source, saved to
// argv[1], with no fallback behind it. The first run shows the portal's picker; a human
// approves it once, later runs are silent.
int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("SNIM_RECORDER_MODULE"))
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);

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

    Screen::ScreencastFrameSource source;
    QElapsedTimer clock;
    QObject::connect(&source, &Screen::DesktopFrameSource::frameReady, &app,
                     [&](const QPixmap &pixmap) {
                         const bool saved = pixmap.save(out);
                         std::fprintf(stderr, "probe: %dx%d in %lld ms, saved=%d to %s\n",
                                      pixmap.width(), pixmap.height(), clock.elapsed(), saved,
                                      qPrintable(out));
                         // Let the session's Close call leave before the process does.
                         QTimer::singleShot(200, &app, [&app, saved] { app.exit(saved ? 0 : 1); });
                     });
    QObject::connect(&source, &Screen::DesktopFrameSource::frameFailed, &app,
                     [&](const QString &error, bool cancelled) {
                         std::fprintf(stderr, "probe: %s: %s\n", cancelled ? "cancelled" : "failed",
                                      qPrintable(error));
                         app.exit(cancelled ? 5 : 1);
                     });
    QTimer::singleShot(90000, &app, [&app] {
        std::fprintf(stderr, "probe: gave up waiting\n");
        app.exit(4);
    });

    clock.start();
    source.grab();
    return app.exec();
}
