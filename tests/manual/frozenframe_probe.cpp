#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <cstdio>
#include <functional>

#include "screen/FrozenFrameGrabber.h"

// Manual check, not a ctest: one frozen frame for the recording selector, saved to
// argv[1]. SNIM_CAPTURE_STRATEGY picks the first source like it does for screenshots;
// argv[2] borrows another app name's settings, such as a stored ScreenCast restore token.
// SNIM_PROBE_REGRAB_MS grabs once more that long after the first, SNIM_PROBE_TICK_MS
// prints a heartbeat that stops whenever the GUI thread is blocked, and
// SNIM_PROBE_LINGER_MS keeps the app running after the last grab.
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
    const int regrabMs = qEnvironmentVariableIntValue("SNIM_PROBE_REGRAB_MS");
    const int tickMs = qEnvironmentVariableIntValue("SNIM_PROBE_TICK_MS");
    const int lingerMs = qEnvironmentVariableIntValue("SNIM_PROBE_LINGER_MS");

    Screen::FrozenFrameGrabber grabber;
    QElapsedTimer clock;
    clock.start();
    int status = 1;
    int grabsLeft = regrabMs > 0 ? 2 : 1;

    QTimer ticker;
    if (tickMs > 0) {
        QObject::connect(&ticker, &QTimer::timeout, &app, [&clock] {
            std::fprintf(stderr, "probe: tick at %lld ms\n", clock.elapsed());
        });
        ticker.start(tickMs);
    }

    std::function<void()> grabOnce = [&] {
        const qint64 started = clock.elapsed();
        std::fprintf(stderr, "probe: grab at %lld ms\n", started);
        grabber.grab([&, started](const QPixmap &frozen, const QRect &virtualGeometry) {
            if (frozen.isNull()) {
                std::fprintf(stderr, "probe: failed after %lld ms: %s\n", clock.elapsed() - started,
                             qPrintable(grabber.lastError()));
                status = 1;
            } else {
                const bool saved = frozen.save(out);
                std::fprintf(stderr, "probe: %dx%d at DPR %.2f over %d,%d %dx%d in %lld ms, "
                                     "saved=%d to %s\n",
                             frozen.width(), frozen.height(), frozen.devicePixelRatio(),
                             virtualGeometry.x(), virtualGeometry.y(), virtualGeometry.width(),
                             virtualGeometry.height(), clock.elapsed() - started, saved,
                             qPrintable(out));
                status = saved ? 0 : 1;
            }
            if (--grabsLeft > 0) {
                QTimer::singleShot(regrabMs, &app, grabOnce);
                return;
            }
            // Let a ScreenCast session's Close call leave before the process does.
            QTimer::singleShot(qMax(200, lingerMs), &app, [&app, &status] { app.exit(status); });
        });
    };
    QTimer::singleShot(0, &app, grabOnce);
    QTimer::singleShot(150000, &app, [&app] {
        std::fprintf(stderr, "probe: gave up waiting\n");
        app.exit(4);
    });
    return app.exec();
}
