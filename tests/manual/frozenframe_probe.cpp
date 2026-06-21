#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <cstdio>

#include "screen/FrozenFrameGrabber.h"

// Manual check, not a ctest: one frozen frame for the recording selector, saved to
// argv[1]. SNIM_CAPTURE_STRATEGY picks the first source like it does for screenshots;
// argv[2] borrows another app name's settings, such as a stored ScreenCast restore token.
int main(int argc, char *argv[])
{
#ifdef SNIM_RECORDER_MODULE_PATH
    if (qEnvironmentVariableIsEmpty("SNIM_RECORDER_MODULE"))
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Snim"));
    QCoreApplication::setApplicationName(argc > 2 ? QString::fromLocal8Bit(argv[2])
                                                  : QStringLiteral("snim-frozenframe-probe"));
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("frozen.png");

    Screen::FrozenFrameGrabber grabber;
    QElapsedTimer clock;
    int status = 1;
    QTimer::singleShot(0, &app, [&] {
        clock.start();
        grabber.grab([&](const QPixmap &frozen, const QRect &virtualGeometry) {
            if (frozen.isNull()) {
                std::fprintf(stderr, "probe: failed after %lld ms: %s\n", clock.elapsed(),
                             qPrintable(grabber.lastError()));
            } else {
                const bool saved = frozen.save(out);
                std::fprintf(stderr, "probe: %dx%d at DPR %.2f over %d,%d %dx%d in %lld ms, "
                                     "saved=%d to %s\n",
                             frozen.width(), frozen.height(), frozen.devicePixelRatio(),
                             virtualGeometry.x(), virtualGeometry.y(), virtualGeometry.width(),
                             virtualGeometry.height(), clock.elapsed(), saved, qPrintable(out));
                status = saved ? 0 : 1;
            }
            // Let a ScreenCast session's Close call leave before the process does.
            QTimer::singleShot(200, &app, [&app, &status] { app.exit(status); });
        });
    });
    QTimer::singleShot(150000, &app, [&app] {
        std::fprintf(stderr, "probe: gave up waiting\n");
        app.exit(4);
    });
    return app.exec();
}
