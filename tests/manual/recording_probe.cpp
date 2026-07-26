#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QTimer>
#include <cstdio>
#include <memory>

#include "recording/RecordingFrameOverlay.h"
#include "recording/strategies/LinuxRecorderModule.h"
#include "screen/WindowEnumerator.h"

// Manual check, not a ctest: records an area through the real recorder (the portal, or
// ximagesrc in an X11 session).
// Usage: snim_recording_probe <out.mp4> [seconds] [pause-at-seconds pause-length]
// SNIM_PROBE_AUDIO=1 records system audio too, SNIM_PROBE_MIC=1 the default microphone. SNIM_PROBE_AREA picks what to record:
// "x,y,w,h" in logical coordinates, "full" for the whole desktop, "screen:N", "window"
// for the one the portal's picker chooses, "window:<X11 id>" or "window:top" for the
// topmost one the window picker offers.
// SNIM_PROBE_FRAME=1 shows the recording frame overlay while capturing, like the app.
// SNIM_PROBE_STOP_MS=n stops n ms after start, first frame or not.
int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("SNIM_RECORDER_MODULE"))
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);

    QApplication app(argc, argv);
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

    QRect area;
    std::unique_ptr<Recording::RecordingFrameOverlay> frame;
    if (qEnvironmentVariableIsSet("SNIM_PROBE_FRAME"))
        frame = std::make_unique<Recording::RecordingFrameOverlay>();

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
        if (frame)
            frame->showForRegion(area);
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
                         if (frame)
                             frame->hide();
                         QTimer::singleShot(200, &app, [&app] { app.exit(0); });
                     });
    QObject::connect(recorder.get(), &Recording::RecordingStrategy::cancelled, &app, [&] {
        std::fprintf(stderr, "probe: cancelled after %lld ms, output %s\n", clock.elapsed(),
                     QFile::exists(out) ? "left behind" : "removed");
        QTimer::singleShot(200, &app, [&app] { app.exit(5); });
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

    QRect desktop;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const QScreen *s = screens.at(i);
        std::fprintf(stderr, "probe: screen %d %s %d,%d %dx%d dpr=%.2f\n", i,
                     qPrintable(s->name()), s->geometry().x(), s->geometry().y(),
                     s->geometry().width(), s->geometry().height(), s->devicePixelRatio());
        desktop = desktop.united(s->geometry());
    }

    const QRect screen = QGuiApplication::primaryScreen()->geometry();
    area = QRect(screen.x() + 100, screen.y() + 100, 640, 360);
    const QString spec = qEnvironmentVariable("SNIM_PROBE_AREA");
    if (spec == QLatin1String("full")) {
        area = desktop;
    } else if (spec.startsWith(QLatin1String("screen:"))) {
        area = screens.value(spec.mid(7).toInt(), screens.first())->geometry();
    } else if (const QStringList parts = spec.split(QLatin1Char(',')); parts.size() == 4) {
        area = QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
    }
    Recording::RecordTarget target;
    if (spec.startsWith(QLatin1String("window"))) {
        target.kind = Recording::RecordTarget::Kind::Window;
        target.windowId = spec.mid(7).toULongLong(nullptr, 0);
        area = QRect();
        if (spec == QLatin1String("window:top")) {
            const QVector<Screen::WindowInfo> windows = Screen::enumerateWindowInfos();
            for (const Screen::WindowInfo &w : windows) {
                std::fprintf(stderr, "probe: window 0x%llx at %d,%d %dx%d\n", w.id, w.rect.x(),
                             w.rect.y(), w.rect.width(), w.rect.height());
            }
            if (!windows.isEmpty()) {
                target.windowId = windows.first().id;
                area = windows.first().rect;
            }
        }
        target.systemPicker = target.windowId == 0;
    }
    std::fprintf(stderr, "probe: %s recording %d,%d %dx%d window 0x%llx\n",
                 qPrintable(recorder->name()), area.x(), area.y(), area.width(), area.height(),
                 target.windowId);
    target.regionVirtual = area;
    target.fps = 30;
    target.captureCursor = qEnvironmentVariableIsSet("SNIM_PROBE_CURSOR");
    target.retinaCapture = !qEnvironmentVariableIsSet("SNIM_PROBE_LOGICAL");
    target.captureSystemAudio = qEnvironmentVariableIsSet("SNIM_PROBE_AUDIO");
    target.captureMic = qEnvironmentVariableIsSet("SNIM_PROBE_MIC");
    clock.start();
    recorder->start(target, out);
    if (const int stopMs = qEnvironmentVariableIntValue("SNIM_PROBE_STOP_MS"); stopMs > 0) {
        QTimer::singleShot(stopMs, &app, [&] {
            std::fprintf(stderr, "probe: early stop at t=%lld ms\n", clock.elapsed());
            recorder->stop();
        });
    }
    return app.exec();
}
