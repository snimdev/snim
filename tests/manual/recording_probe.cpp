#include <QElapsedTimer>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <cstdio>
#include <memory>

#include "recording/strategies/LinuxRecorderModule.h"

// Manual check, not a ctest: records an area through the real portal recorder.
// Usage: snim_recording_probe <out.mp4> [seconds] [pause-at-seconds pause-length]
// SNIM_PROBE_AUDIO=1 records system audio too.
int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("SNIM_RECORDER_MODULE"))
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);

    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Snim"));
    QCoreApplication::setApplicationName(QStringLiteral("snim-recording-probe"));
    const QStringList args = app.arguments();
    const QString out = args.size() > 1 ? args.at(1) : QStringLiteral("probe.mp4");
    const int seconds = args.size() > 2 ? args.at(2).toInt() : 5;
    const int pauseAt = args.size() > 3 ? args.at(3).toInt() : -1;
    const int pauseFor = args.size() > 4 ? args.at(4).toInt() : 0;

    std::unique_ptr<Recording::RecordingStrategy> recorder(
        Recording::LinuxRecorderModule::create());
    if (!recorder || !recorder->isAvailable()) {
        std::fprintf(stderr, "probe: recorder unavailable: %s\n",
                     qPrintable(Recording::LinuxRecorderModule::missingPieces().join("; ")));
        return 3;
    }

    QElapsedTimer clock;
    qint64 firstDuration = -1;
    qint64 lastDuration = -1;
    QObject::connect(recorder.get(), &Recording::RecordingStrategy::durationChanged, &app,
                     [&](qint64 ms) {
                         if (firstDuration < 0)
                             firstDuration = ms;
                         lastDuration = ms;
                         std::fprintf(stderr, "probe: t=%lld ms duration=%lld ms\n",
                                      clock.elapsed(), ms);
                     });
    QObject::connect(recorder.get(), &Recording::RecordingStrategy::pausedChanged, &app,
                     [&](bool paused) {
                         std::fprintf(stderr, "probe: t=%lld ms paused=%d\n", clock.elapsed(),
                                      paused);
                     });
    QObject::connect(recorder.get(), &Recording::RecordingStrategy::started, &app, [&] {
        std::fprintf(stderr, "probe: started after %lld ms\n", clock.elapsed());
        clock.restart();
        if (pauseAt >= 0) {
            QTimer::singleShot(pauseAt * 1000, &app, [&] { recorder->pause(); });
            QTimer::singleShot((pauseAt + pauseFor) * 1000, &app, [&] { recorder->resume(); });
        }
        QTimer::singleShot((seconds + qMax(0, pauseFor)) * 1000, &app, [&] {
            std::fprintf(stderr, "probe: stopping at t=%lld ms\n", clock.elapsed());
            recorder->stop();
        });
    });
    QObject::connect(recorder.get(), &Recording::RecordingStrategy::finished, &app,
                     [&](const QString &path) {
                         std::fprintf(stderr, "probe: finished %s first=%lld last=%lld ms\n",
                                      qPrintable(path), firstDuration, lastDuration);
                         QTimer::singleShot(200, &app, [&app] { app.exit(0); });
                     });
    QObject::connect(recorder.get(), &Recording::RecordingStrategy::failed, &app,
                     [&](const QString &error) {
                         std::fprintf(stderr, "probe: failed after %lld ms: %s\n",
                                      clock.elapsed(), qPrintable(error));
                         QTimer::singleShot(200, &app, [&app] { app.exit(1); });
                     });

    // A heartbeat that stops printing means the GUI thread is blocked.
    QTimer heartbeat;
    QElapsedTimer beatClock;
    beatClock.start();
    QObject::connect(&heartbeat, &QTimer::timeout, &app, [&] {
        const qint64 gap = beatClock.restart();
        if (gap > 1500)
            std::fprintf(stderr, "probe: GUI thread was blocked for %lld ms\n", gap);
    });
    heartbeat.start(500);

    QTimer::singleShot(120000, &app, [&app] {
        std::fprintf(stderr, "probe: gave up waiting\n");
        app.exit(4);
    });

    const QRect screen = QGuiApplication::primaryScreen()->geometry();
    Recording::RecordTarget target;
    target.regionVirtual = QRect(screen.x() + 100, screen.y() + 100, 640, 360);
    target.fps = 30;
    target.captureCursor = false;
    target.captureSystemAudio = qEnvironmentVariableIsSet("SNIM_PROBE_AUDIO");
    clock.start();
    recorder->start(target, out);
    return app.exec();
}
