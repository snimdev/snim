#include <QtTest>
#include <memory>

#include "FakeFrameSources.h"
#include "capture/CaptureFactory.h"
#include "capture/strategies/CaptureStrategy.h"
#include "capture/strategies/FrameSourceCaptureStrategy.h"
#include "screen/AreaSelector.h"
#include "screen/sources/DesktopFrameSource.h"
#include "screen/sources/FrameSourceFactory.h"
#include "screen/sources/StrategySelection.h"

using namespace Capture;
namespace StrategySelection = Screen::StrategySelection;
using Screen::FrameSourceFactory;

namespace {

// Sets environment variables for one test and puts the old values back after it.
class ScopedEnv
{
public:
    ScopedEnv(std::initializer_list<std::pair<const char *, QByteArray>> values)
    {
        for (const auto &[name, value] : values) {
            m_saved.append({name, qEnvironmentVariableIsSet(name), qgetenv(name)});
            qputenv(name, value);
        }
    }
    ~ScopedEnv()
    {
        for (const Saved &saved : std::as_const(m_saved)) {
            if (saved.wasSet)
                qputenv(saved.name, saved.value);
            else
                qunsetenv(saved.name);
        }
    }
    Q_DISABLE_COPY_MOVE(ScopedEnv)

private:
    struct Saved {
        const char *name;
        bool wasSet;
        QByteArray value;
    };
    QList<Saved> m_saved;
};

// A strategy that only hands frames on, to test the delivery every source path shares.
class DeliveringStrategy : public CaptureStrategy
{
public:
    using CaptureStrategy::CaptureStrategy;
    using CaptureStrategy::deliverFrame;
    void captureFullScreen() override {}
    void captureArea() override {}
    void captureWindow() override {}
    QString name() const override { return QStringLiteral("Delivering"); }
};

QPixmap frameOf(int width, int height, qreal dpr)
{
    QPixmap frame(width, height);
    frame.fill(Qt::red);
    frame.setDevicePixelRatio(dpr);
    return frame;
}

QList<Screen::AreaSelector *> openSelectors()
{
    QList<Screen::AreaSelector *> selectors;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        if (auto *selector = qobject_cast<Screen::AreaSelector *>(widget); selector && selector->isVisible())
            selectors.append(selector);
    }
    return selectors;
}

using Type = Screen::SourceType;

// A 1x frame of the whole offscreen desktop.
FakeFrames::Script wholeDesktop()
{
    const QRect desktop = Screen::qtVirtualDesktop();
    return FakeFrames::delivering(desktop.size(), 1.0, desktop);
}

// Offscreen and outside a Wayland session, whatever session runs the tests.
ScopedEnv outsideWayland()
{
    return {{"WAYLAND_DISPLAY", QByteArray()}, {"XDG_SESSION_TYPE", QByteArrayLiteral("x11")}};
}

} // namespace

// Factory + Strategy: test only the side-effect-free surface. NEVER call
// captureArea/captureWindow/captureFullScreen; they grab the real screen and
// pop up full-screen overlays.
class tst_CaptureFactory : public QObject
{
    Q_OBJECT

private slots:
    void createsNativeStrategy()
    {
        auto s = CaptureFactory::createStrategy(CaptureFactory::StrategyType::Native, nullptr);
        QVERIFY(s);
        QCOMPARE(s->name(), QStringLiteral("Native Qt Capture"));
    }

    void autoSelectionProducesNonNull()
    {
        auto s = CaptureFactory::createStrategy();   // Auto
        QVERIFY(s);
    }

    void screencastIsPreferredWhereItBringsSilence()
    {
        using StrategySelection::prefersScreencast;
        // GNOME asks on every Screenshot portal call, sandboxed or not.
        QVERIFY(prefersScreencast(QStringLiteral("GNOME"), false));
        QVERIFY(prefersScreencast(QStringLiteral("ubuntu:GNOME"), false));
        QVERIFY(prefersScreencast(QStringLiteral("GNOME"), true));
        // KDE: KWin natively, the silent Screenshot portal inside Flatpak.
        QVERIFY(!prefersScreencast(QStringLiteral("KDE"), false));
        QVERIFY(!prefersScreencast(QStringLiteral("KDE"), true));
        // Any other desktop only when sandboxed; natively its own tools still apply.
        QVERIFY(prefersScreencast(QStringLiteral("sway"), true));
        QVERIFY(prefersScreencast(QString(), true));
        QVERIFY(!prefersScreencast(QStringLiteral("sway"), false));
        QVERIFY(!prefersScreencast(QStringLiteral("GNOME-Flashback"), false));
    }

    void parsesTheStrategyOverride()
    {
        using StrategySelection::parseOverride;
        using Type = CaptureFactory::StrategyType;
        QCOMPARE(parseOverride(QStringLiteral("screencast")), std::optional(Type::Screencast));
        QCOMPARE(parseOverride(QStringLiteral(" KWin ")), std::optional(Type::KWin));
        QCOMPARE(parseOverride(QStringLiteral("portal")), std::optional(Type::Portal));
        QCOMPARE(parseOverride(QStringLiteral("native")), std::optional(Type::Native));
        QCOMPARE(parseOverride(QStringLiteral(" Screencopy ")), std::optional(Type::Screencopy));
        QCOMPARE(parseOverride(QStringLiteral("wlroots")), std::optional(Type::Screencopy));
        QCOMPARE(parseOverride(QStringLiteral("wayland")), std::optional(Type::Portal));
        QVERIFY(!parseOverride(QString()));
        QVERIFY(!parseOverride(QStringLiteral("bogus")));
    }

    void theSessionEnvironmentIsRestored()
    {
        const QByteArray display = qgetenv("WAYLAND_DISPLAY");
        const bool displaySet = qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
        {
            const ScopedEnv session = outsideWayland();
            QVERIFY(!Screen::isWaylandSession());
        }
        QCOMPARE(qEnvironmentVariableIsSet("WAYLAND_DISPLAY"), displaySet);
        QCOMPARE(qgetenv("WAYLAND_DISPLAY"), display);
    }

    void theOverrideDecidesTheDefault()
    {
        const ScopedEnv forced{{"SNIM_CAPTURE_STRATEGY", QByteArrayLiteral("native")}};
        QCOMPARE(FrameSourceFactory::defaultType(), CaptureFactory::StrategyType::Native);
    }

    void anUnavailableScreencastStillYieldsAStrategy()
    {
        // Outside a Wayland session neither ScreenCast nor the portal behind it is offered.
        const ScopedEnv session = outsideWayland();
        auto s = CaptureFactory::createStrategy(CaptureFactory::StrategyType::Screencast);
        QVERIFY(s);
        QCOMPARE(s->name(), QStringLiteral("Native Qt Capture"));
    }

    void screencopyIsPreferredOffKdeAndGnome()
    {
        using StrategySelection::prefersScreencopy;
        QVERIFY(prefersScreencopy(QStringLiteral("sway")));
        QVERIFY(prefersScreencopy(QStringLiteral("Hyprland")));
        QVERIFY(prefersScreencopy(QStringLiteral("niri")));
        QVERIFY(prefersScreencopy(QStringLiteral("wayfire:wlroots")));
        QVERIFY(prefersScreencopy(QStringLiteral("COSMIC")));
        QVERIFY(prefersScreencopy(QString()));
        QVERIFY(!prefersScreencopy(QStringLiteral("KDE")));
        QVERIFY(!prefersScreencopy(QStringLiteral("ubuntu:GNOME")));
    }

    void selectionMatrix_data()
    {
        QTest::addColumn<QString>("desktop");
        QTest::addColumn<bool>("flatpak");
        QTest::addColumn<bool>("kwin");
        QTest::addColumn<bool>("screencopy");
        QTest::addColumn<bool>("screencast");
        QTest::addColumn<bool>("portal");
        QTest::addColumn<CaptureFactory::StrategyType>("expected");

        using Type = CaptureFactory::StrategyType;
        QTest::newRow("KDE native") << "KDE" << false << true << true << true << true << Type::KWin;
        QTest::newRow("KDE Flatpak") << "KDE" << true << true << true << true << true << Type::Portal;
        QTest::newRow("GNOME native") << "GNOME" << false << false << true << true << true << Type::Screencast;
        QTest::newRow("GNOME Flatpak") << "GNOME" << true << false << false << true << true << Type::Screencast;
        QTest::newRow("GNOME no ScreenCast") << "GNOME" << false << false << false << false << true << Type::Portal;
        QTest::newRow("sway native") << "sway" << false << false << true << true << true << Type::Screencopy;
        QTest::newRow("sway native, hidden globals") << "sway" << false << false << false << true << true << Type::Portal;
        QTest::newRow("sway Flatpak, globals shown") << "sway" << true << false << true << true << true << Type::Screencopy;
        QTest::newRow("sway Flatpak") << "sway" << true << false << false << true << true << Type::Screencast;
        QTest::newRow("sway Flatpak, old portal") << "sway" << true << false << false << false << true << Type::Portal;
        QTest::newRow("X11") << "XFCE" << false << false << false << false << false << Type::Native;
    }

    void selectionMatrix()
    {
        QFETCH(QString, desktop);
        QFETCH(bool, flatpak);
        QFETCH(bool, kwin);
        QFETCH(bool, screencopy);
        QFETCH(bool, screencast);
        QFETCH(bool, portal);
        QFETCH(CaptureFactory::StrategyType, expected);

        StrategySelection::Probes probes;
        probes.kwin = [kwin] { return kwin; };
        probes.screencopy = [screencopy] { return screencopy; };
        probes.screencast = [screencast] { return screencast; };
        probes.portal = [portal] { return portal; };
        QCOMPARE(StrategySelection::choose(desktop, flatpak, probes), expected);
    }

    void probesRunOnlyWhenReached()
    {
        // On KDE the Wayland globals are never even probed.
        bool screencopyProbed = false;
        StrategySelection::Probes probes;
        probes.kwin = [] { return true; };
        probes.screencopy = [&screencopyProbed] { return screencopyProbed = true; };
        QCOMPARE(StrategySelection::choose(QStringLiteral("KDE"), true, probes),
                 CaptureFactory::StrategyType::Native);
        QVERIFY(!screencopyProbed);
    }

    void screencopyIsNeverPickedOffscreen()
    {
        // No Wayland connection here: the factory must fall back, never hand out a dead strategy.
        auto s = CaptureFactory::createStrategy(CaptureFactory::StrategyType::Screencopy);
        QVERIFY(s);
        QVERIFY(!s->name().contains(QStringLiteral("screencopy"), Qt::CaseInsensitive));
        QVERIFY(FrameSourceFactory::defaultType() != CaptureFactory::StrategyType::Screencopy);
    }

    void theOverrideForcesScreencopy()
    {
        const ScopedEnv forced{{"SNIM_CAPTURE_STRATEGY", QByteArrayLiteral("wlroots")}};
        QCOMPARE(FrameSourceFactory::defaultType(), CaptureFactory::StrategyType::Screencopy);
        // Forced but unavailable still yields a working strategy.
        QVERIFY(CaptureFactory::createStrategy());
    }

    void theFrozenFrameFallsBackToTheScreenshotPortal_data()
    {
        using Type = CaptureFactory::StrategyType;
        QTest::addColumn<Type>("chosen");
        QTest::addColumn<QList<Type>>("chain");
        QTest::newRow("KDE") << Type::KWin << QList<Type>{Type::KWin, Type::Portal};
        QTest::newRow("GNOME") << Type::Screencast << QList<Type>{Type::Screencast, Type::Portal};
        QTest::newRow("wlroots") << Type::Screencopy << QList<Type>{Type::Screencopy, Type::Portal};
        QTest::newRow("portal only") << Type::Portal << QList<Type>{Type::Portal};
        QTest::newRow("unresolved") << Type::Auto << QList<Type>{Type::Portal};
        // X11, or a forced native grab: no portal behind Qt's own.
        QTest::newRow("native") << Type::Native << QList<Type>{Type::Native};
    }

    void theFrozenFrameFallsBackToTheScreenshotPortal()
    {
        using Type = CaptureFactory::StrategyType;
        QFETCH(Type, chosen);
        QFETCH(QList<Type>, chain);
        QCOMPARE(StrategySelection::frameSourceChain(chosen), chain);
    }

    void frameSourcesExistOnlyWhereTheyCanDeliver()
    {
        const auto native = FrameSourceFactory::create(CaptureFactory::StrategyType::Native);
        QVERIFY(native);
        QCOMPARE(FrameSourceFactory::typeName(CaptureFactory::StrategyType::Native),
                 QStringLiteral("Qt screen grab"));
        // Offscreen and no Wayland session: no screencopy and no ScreenCast session.
        const ScopedEnv session = outsideWayland();
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::Screencopy));
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::Screencast));
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        QCOMPARE(FrameSourceFactory::defaultType(), CaptureFactory::StrategyType::Native);
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::KWin));
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::Portal));
#endif
    }

    void aPartialPickGoesOutAsItIs()
    {
        // One monitor of a two-monitor desktop, picked in a system dialog.
        DeliveringStrategy strategy;
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.deliverFrame(frameOf(1920, 1080, 1.0), QRect(0, 0, 3840, 1080), true);
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().first().value<QPixmap>().size(), QSize(1920, 1080));
        QVERIFY(openSelectors().isEmpty());
    }

    void aWholeFrameGoesOutWithoutASelector()
    {
        DeliveringStrategy strategy;
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.deliverFrame(frameOf(7680, 2160, 2.0), QRect(-1920, 0, 3840, 1080), false);
        QCOMPARE(ready.count(), 1);
        QVERIFY(openSelectors().isEmpty());
    }

    void aWholeFrameOpensTheSelectorWhoseEscapeIsACancel()
    {
        DeliveringStrategy strategy;
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        QSignalSpy failed(&strategy, &CaptureStrategy::screenshotFailed);
        QSignalSpy cancelled(&strategy, &CaptureStrategy::screenshotCancelled);
        const QRect desktop = Screen::qtVirtualDesktop();
        strategy.deliverFrame(frameOf(desktop.width(), desktop.height(), 1.0), desktop, true);
        QCOMPARE(ready.count(), 0);
        const QList<Screen::AreaSelector *> selectors = openSelectors();
        QCOMPARE(selectors.size(), QGuiApplication::screens().size());

        selectors.first()->cancelSelection();
        QCOMPARE(cancelled.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(ready.count(), 0);
        QVERIFY(openSelectors().isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // The frame-source strategy over scripted sources: what each capture kind delivers.
    void aFullScreenShotGoesOutWithoutASelector()
    {
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Screencopy, wholeDesktop());
        FrameSourceCaptureStrategy strategy({Type::Screencopy, Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.captureFullScreen();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().first().value<QPixmap>().deviceIndependentSize(),
                 QSizeF(Screen::qtVirtualDesktop().size()));
        QVERIFY(openSelectors().isEmpty());
        QCOMPARE(fake.asked, QList<Type>{Type::Screencopy});
    }

    void anAreaShotOpensTheSelector()
    {
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Screencopy, wholeDesktop());
        FrameSourceCaptureStrategy strategy({Type::Screencopy, Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        QSignalSpy cancelled(&strategy, &CaptureStrategy::screenshotCancelled);
        strategy.captureArea();
        QCOMPARE(ready.count(), 0);
        const QList<Screen::AreaSelector *> selectors = openSelectors();
        QCOMPARE(selectors.size(), QGuiApplication::screens().size());
        selectors.first()->cancelSelection();
        QCOMPARE(cancelled.count(), 1);
        QVERIFY(openSelectors().isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void aWindowShotDrawsAnArea()
    {
        // Wayland lists no windows: a drag and Enter pick the area, as for an area shot.
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Portal, wholeDesktop());
        FrameSourceCaptureStrategy strategy({Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.captureWindow();
        const QList<Screen::AreaSelector *> selectors = openSelectors();
        QCOMPARE(selectors.size(), QGuiApplication::screens().size());
        Screen::AreaSelector *selector = selectors.first();
        QVERIFY(QTest::qWaitForWindowExposed(selector));
        QTest::mousePress(selector, Qt::LeftButton, Qt::NoModifier, QPoint(40, 40));
        QTest::mouseMove(selector, QPoint(240, 180));
        QTest::mouseRelease(selector, Qt::LeftButton, Qt::NoModifier, QPoint(240, 180));
        QTest::keyClick(selector, Qt::Key_Return);
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().first().value<QPixmap>().deviceIndependentSize(),
                 QSizeF(QRect(QPoint(40, 40), QPoint(240, 180)).size()));
        QVERIFY(openSelectors().isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void aPartialPickSkipsTheSelector()
    {
        // One monitor of two, picked in the ScreenCast dialog, for an area shot.
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Screencast,
                            FakeFrames::delivering({1920, 1080}, 1.0, QRect(0, 0, 3840, 1080)));
        FrameSourceCaptureStrategy strategy({Type::Screencast, Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.captureArea();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().first().value<QPixmap>().size(), QSize(1920, 1080));
        QVERIFY(openSelectors().isEmpty());
    }

    void aCancelInASystemDialogIsQuiet()
    {
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Screencast, FakeFrames::cancelling());
        fake.offered.insert(Type::Portal, wholeDesktop());
        FrameSourceCaptureStrategy strategy({Type::Screencast, Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        QSignalSpy failed(&strategy, &CaptureStrategy::screenshotFailed);
        QSignalSpy cancelled(&strategy, &CaptureStrategy::screenshotCancelled);
        strategy.captureFullScreen();
        QCOMPARE(cancelled.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(ready.count(), 0);
        QCOMPARE(fake.asked, QList<Type>{Type::Screencast});
    }

    void aFailedSourceHandsOverToThePortal()
    {
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Screencopy, FakeFrames::failing());
        fake.offered.insert(Type::Portal, wholeDesktop());
        FrameSourceCaptureStrategy strategy({Type::Screencopy, Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.captureFullScreen();
        QCOMPARE(ready.count(), 1);
        QCOMPARE(fake.asked, (QList<Type>{Type::Screencopy, Type::Portal}));
    }

    void aFailedScreencastIsSkippedForTheRun()
    {
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Screencast, FakeFrames::failing());
        fake.offered.insert(Type::Portal, wholeDesktop());
        fake.withdrawOnFailure.append(Type::Screencast);
        FrameSourceCaptureStrategy strategy({Type::Screencast, Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.captureFullScreen();
        strategy.captureFullScreen();
        QCOMPARE(ready.count(), 2);
        QCOMPARE(fake.asked, (QList<Type>{Type::Screencast, Type::Portal, Type::Portal}));
    }

    void failuresNameWhatWasMissing()
    {
        FakeFrames::Desktop fake;
        fake.offered.insert(Type::Screencopy, FakeFrames::failing());
        FrameSourceCaptureStrategy strategy({Type::Screencopy, Type::Portal}, fake.factory());
        QSignalSpy failed(&strategy, &CaptureStrategy::screenshotFailed);
        strategy.captureFullScreen();
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().first().toString(),
                 QStringLiteral("No screenshot method available: the Screenshot portal needs "
                                "xdg-desktop-portal and a backend for this desktop."));

        fake.offered.insert(Type::Portal, FakeFrames::failing());
        strategy.captureFullScreen();
        QCOMPARE(failed.count(), 2);
        QCOMPARE(failed.last().first().toString(), QStringLiteral("Portal screenshot failed: broken"));
    }

    void aCaptureInFlightIgnoresAnother()
    {
        FakeFrames::Desktop fake;
        FakeFrames::Script slow = wholeDesktop();
        slow.async = true;
        fake.offered.insert(Type::Portal, slow);
        FrameSourceCaptureStrategy strategy({Type::Portal}, fake.factory());
        QSignalSpy ready(&strategy, &CaptureStrategy::screenshotReady);
        strategy.captureFullScreen();
        strategy.captureArea();
        QVERIFY(ready.wait(1000));
        QCOMPARE(ready.count(), 1);
        QCOMPARE(fake.asked, QList<Type>{Type::Portal});
        QVERIFY(openSelectors().isEmpty());
    }

    void theScreenPickerHintReachesTheTray()
    {
        FakeFrames::Desktop fake;
        FakeFrames::Script firstPick = wholeDesktop();
        firstPick.picker = true;
        fake.offered.insert(Type::Screencast, firstPick);
        FrameSourceCaptureStrategy strategy({Type::Screencast, Type::Portal}, fake.factory());
        QSignalSpy picker(&strategy, &FrameSourceCaptureStrategy::sourcePickerExpected);
        strategy.captureFullScreen();
        QCOMPARE(picker.count(), 1);
        QCOMPARE(picker.first().first().toBool(), true);
    }

    void quickActionsToggle()
    {
        auto s = CaptureFactory::createStrategy(CaptureFactory::StrategyType::Native);
        QVERIFY(s->quickActionsEnabled());   // default on (normal capture)
        s->setQuickActionsEnabled(false);    // OCR text-snip path turns it off
        QVERIFY(!s->quickActionsEnabled());
    }
};

QTEST_MAIN(tst_CaptureFactory)
#include "tst_capturefactory.moc"
