#include <QApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QScreen>
#include <QTest>
#include <QTimer>
#include <cstdio>
#include <functional>
#include <memory>

#include "capture/CaptureFactory.h"
#include "capture/strategies/CaptureStrategy.h"
#include "record/RecordingController.h"
#include "record/RecordingStrategy.h"
#include "screen/AreaSelector.h"

// Manual check, not a ctest: one capture through the app's own strategy, picked like the app
// does (SNIM_CAPTURE_STRATEGY forces one), saved to argv[2].
// Usage: snim_capture_probe <full|area|window|cancel|record|record-window> <out.png>
// area and window drive the selector like a user: a drag from SNIM_PROBE_FROM to
// SNIM_PROBE_TO ("x,y" logical, by default a quarter in from the first screen's corners),
// then Enter; the native window pick clicks at SNIM_PROBE_FROM instead, and cancel presses
// Escape. Before committing it prints how each overlay draws the selection, so a selection
// spanning screens shows whether every overlay mirrors it. SNIM_PROBE_AGAIN=1 captures once
// more in the same run, to show what the first capture left behind. record and record-window
// drive the recording selector the same way, over a backend that only prints its target.
namespace {

QPoint pointFrom(const char *name, const QPoint &fallback)
{
    const QStringList parts = qEnvironmentVariable(name).split(QLatin1Char(','));
    if (parts.size() != 2)
        return fallback;
    return {parts[0].trimmed().toInt(), parts[1].trimmed().toInt()};
}

QList<Screen::AreaSelector *> visibleSelectors()
{
    QList<Screen::AreaSelector *> selectors;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *selector = qobject_cast<Screen::AreaSelector *>(widget); selector && selector->isVisible())
            selectors.append(selector);
    }
    return selectors;
}

Screen::AreaSelector *selectorAt(const QList<Screen::AreaSelector *> &selectors, const QPoint &virt)
{
    for (Screen::AreaSelector *selector : selectors)
        if (selector->geometry().contains(virt))
            return selector;
    return selectors.value(0);
}

// Brightness inside and outside the selection as each overlay paints it: a lit inside means
// that overlay draws the selection.
void reportOverlays(const QList<Screen::AreaSelector *> &selectors, const QRect &selection)
{
    for (Screen::AreaSelector *selector : selectors) {
        const QRect screen = selector->geometry();
        const QRect inside = selection.intersected(screen);
        if (inside.isEmpty()) {
            std::fprintf(stderr, "probe: overlay %d,%d %dx%d holds none of the selection\n",
                         screen.x(), screen.y(), screen.width(), screen.height());
            continue;
        }
        const QImage painted = selector->grab().toImage();
        const qreal dpr = painted.devicePixelRatio();
        const auto grayAt = [&](const QPoint &virt) {
            const QPoint local = (virt - screen.topLeft()) * dpr;
            return qGray(painted.pixel(local.x(), local.y()));
        };
        // Outside: the screen corner farthest from the selection's center.
        const QPoint center = inside.center();
        const QPoint corner(center.x() < screen.center().x() ? screen.right() - 2 : screen.left() + 2,
                            center.y() < screen.center().y() ? screen.bottom() - 2 : screen.top() + 2);
        std::fprintf(stderr, "probe: overlay %d,%d %dx%d draws inside gray %d, outside gray %d\n",
                     screen.x(), screen.y(), screen.width(), screen.height(), grayAt(center),
                     selection.contains(corner) ? -1 : grayAt(corner));
    }
}

// Records nothing: it prints the target the recording selector built, then cancels.
class PrintingRecorder : public Record::RecordingStrategy
{
public:
    void start(const Record::RecordTarget &target, const QString &) override
    {
        const QRect r = target.regionVirtual;
        std::fprintf(stderr, "probe: recording %s %d,%d %dx%d window 0x%llx\n",
                     target.kind == Record::RecordTarget::Kind::Window ? "window" : "region",
                     r.x(), r.y(), r.width(), r.height(), target.windowId);
        QTimer::singleShot(0, this, [this] { emit cancelled(); });
    }
    void stop() override {}
    bool isRecording() const override { return false; }
    bool isAvailable() const override { return true; }
    QString name() const override { return QStringLiteral("Printing"); }
};

} // namespace

int main(int argc, char *argv[])
{
#ifdef SNIM_RECORDER_MODULE_PATH
    if (qEnvironmentVariableIsEmpty("SNIM_RECORDER_MODULE"))
        qputenv("SNIM_RECORDER_MODULE", SNIM_RECORDER_MODULE_PATH);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Snim"));
    QCoreApplication::setApplicationName(QStringLiteral("snim-capture-probe"));
    const QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("full");
    const QString out = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("capture.png");

    for (const QScreen *s : QGuiApplication::screens())
        std::fprintf(stderr, "probe: screen %s %d,%d %dx%d dpr %.2f\n", qPrintable(s->name()),
                     s->geometry().x(), s->geometry().y(), s->geometry().width(),
                     s->geometry().height(), s->devicePixelRatio());

    std::unique_ptr<Capture::CaptureStrategy> strategy = Capture::CaptureFactory::createStrategy();
    Record::RecordingController recorder(std::make_unique<PrintingRecorder>());
    std::fprintf(stderr, "probe: strategy %s, mode %s\n", qPrintable(strategy->name()),
                 qPrintable(mode));

    const QRect first = QGuiApplication::primaryScreen()->geometry();
    const QPoint from = pointFrom("SNIM_PROBE_FROM", first.topLeft() + QPoint(first.width() / 4,
                                                                                first.height() / 4));
    const QPoint to = pointFrom("SNIM_PROBE_TO", first.topLeft() + QPoint(first.width() * 3 / 4,
                                                                            first.height() * 3 / 4));
    const QRect selection = QRect(from, to).normalized();

    QElapsedTimer clock;
    int capturesLeft = qEnvironmentVariableIsSet("SNIM_PROBE_AGAIN") ? 2 : 1;
    std::function<void()> capture;
    const auto finish = [&](int status) {
        if (--capturesLeft > 0) {
            QTimer::singleShot(500, &app, capture);
            return;
        }
        QTimer::singleShot(200, &app, [&app, status] { app.exit(status); });
    };
    QObject::connect(strategy.get(), &Capture::CaptureStrategy::screenshotReady, &app,
                     [&](const QPixmap &shot) {
        const bool saved = shot.save(out);
        std::fprintf(stderr, "probe: %dx%d at DPR %.2f (%.0fx%.0f logical) in %lld ms, saved=%d to %s\n",
                     shot.width(), shot.height(), shot.devicePixelRatio(),
                     shot.deviceIndependentSize().width(), shot.deviceIndependentSize().height(),
                     clock.elapsed(), saved, qPrintable(out));
        finish(saved ? 0 : 1);
    });
    QObject::connect(strategy.get(), &Capture::CaptureStrategy::screenshotFailed, &app,
                     [&](const QString &error) {
        std::fprintf(stderr, "probe: failed: %s\n", qPrintable(error));
        finish(1);
    });
    QObject::connect(strategy.get(), &Capture::CaptureStrategy::screenshotCancelled, &app, [&] {
        std::fprintf(stderr, "probe: cancelled\n");
        finish(mode == QLatin1String("cancel") ? 0 : 5);
    });
    QObject::connect(&recorder, &Record::RecordingController::recordingCancelled, &app,
                     [&] { finish(0); });
    QObject::connect(&recorder, &Record::RecordingController::recordingFailed, &app,
                     [&](const QString &error) {
        std::fprintf(stderr, "probe: recording failed: %s\n", qPrintable(error));
        finish(1);
    });

    // Drives the selector once every overlay is up.
    QTimer driver;
    driver.setInterval(100);
    QObject::connect(&driver, &QTimer::timeout, &app, [&] {
        const QList<Screen::AreaSelector *> selectors = visibleSelectors();
        if (selectors.size() < QGuiApplication::screens().size())
            return;
        driver.stop();
        for (Screen::AreaSelector *selector : selectors) {
            if (!QTest::qWaitForWindowExposed(selector, 3000))
                std::fprintf(stderr, "probe: an overlay was never exposed\n");
        }
        std::fprintf(stderr, "probe: %lld overlays up after %lld ms\n",
                     static_cast<long long>(selectors.size()), clock.elapsed());
        QTest::qWait(300);

        Screen::AreaSelector *pressed = selectorAt(selectors, from);
        const QPoint origin = pressed->geometry().topLeft();
        if (mode == QLatin1String("cancel")) {
            QTest::keyClick(pressed, Qt::Key_Escape);
            return;
        }
        const bool pick = mode == QLatin1String("record-window")
                          || (mode == QLatin1String("window")
                              && strategy->name() == QLatin1String("Native Qt Capture"));
        if (pick) {
            QTest::mouseMove(pressed, from - origin);
            QTest::qWait(100);
            QTest::mouseClick(pressed, Qt::LeftButton, Qt::NoModifier, from - origin);
            return;
        }
        QTest::mousePress(pressed, Qt::LeftButton, Qt::NoModifier, from - origin);
        QTest::mouseMove(pressed, (from + to) / 2 - origin);
        QTest::mouseMove(pressed, to - origin);
        QTest::mouseRelease(pressed, Qt::LeftButton, Qt::NoModifier, to - origin);
        QTest::qWait(200);
        std::fprintf(stderr, "probe: dragged %d,%d %dx%d\n", selection.x(), selection.y(),
                     selection.width(), selection.height());
        reportOverlays(visibleSelectors(), selection);
        QTest::keyClick(pressed, Qt::Key_Return);
    });

    QTimer::singleShot(60000, &app, [&app] {
        std::fprintf(stderr, "probe: gave up waiting\n");
        app.exit(4);
    });
    capture = [&] {
        clock.start();
        if (mode == QLatin1String("full")) {
            strategy->captureFullScreen();
            return;
        }
        driver.start();
        if (mode == QLatin1String("record"))
            recorder.recordArea();
        else if (mode == QLatin1String("record-window"))
            recorder.recordWindow();
        else if (mode == QLatin1String("window"))
            strategy->captureWindow();
        else
            strategy->captureArea();
    };
    QTimer::singleShot(0, &app, capture);
    return app.exec();
}
