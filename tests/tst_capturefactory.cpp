#include <QtTest>
#include <memory>

#include "capture/CaptureFactory.h"
#include "capture/strategies/CaptureStrategy.h"
#include "screen/sources/DesktopFrameSource.h"
#include "screen/sources/FrameSourceFactory.h"
#include "screen/sources/StrategySelection.h"

using namespace Capture;
namespace StrategySelection = Screen::StrategySelection;
using Screen::FrameSourceFactory;

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
        QVERIFY(s->isAvailable());
    }

    void autoSelectionProducesNonNull()
    {
        auto s = CaptureFactory::createStrategy();   // Auto
        QVERIFY(s);
    }

    void availabilityByType()
    {
        QVERIFY(CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::Native));
        QVERIFY(CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::Auto));
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        QVERIFY(CaptureFactory::getDefaultStrategyType() == CaptureFactory::StrategyType::Native);
        QVERIFY(!CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::KWin));
        QVERIFY(!CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::Wayland));
#endif
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
        QCOMPARE(parseOverride(QStringLiteral("portal")), std::optional(Type::Wayland));
        QCOMPARE(parseOverride(QStringLiteral("native")), std::optional(Type::Native));
        QCOMPARE(parseOverride(QStringLiteral(" Screencopy ")), std::optional(Type::Screencopy));
        QCOMPARE(parseOverride(QStringLiteral("wlroots")), std::optional(Type::Screencopy));
        QCOMPARE(parseOverride(QStringLiteral("wayland")), std::optional(Type::Wayland));
        QVERIFY(!parseOverride(QString()));
        QVERIFY(!parseOverride(QStringLiteral("bogus")));
    }

    void theOverrideDecidesTheDefault()
    {
        qputenv("SNIM_CAPTURE_STRATEGY", "native");
        QVERIFY(CaptureFactory::getDefaultStrategyType() == CaptureFactory::StrategyType::Native);
        qunsetenv("SNIM_CAPTURE_STRATEGY");
    }

    void anUnavailableScreencastStillYieldsAStrategy()
    {
        // Offscreen is no Wayland session, so this must land on a fallback, never null.
        qputenv("WAYLAND_DISPLAY", "");
        qputenv("XDG_SESSION_TYPE", "x11");
        QVERIFY(!CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::Screencast));
        auto s = CaptureFactory::createStrategy(CaptureFactory::StrategyType::Screencast);
        QVERIFY(s);
        QVERIFY(s->name() != QStringLiteral("ScreenCast Portal Capture"));
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
        QTest::newRow("KDE Flatpak") << "KDE" << true << true << true << true << true << Type::Wayland;
        QTest::newRow("GNOME native") << "GNOME" << false << false << true << true << true << Type::Screencast;
        QTest::newRow("GNOME Flatpak") << "GNOME" << true << false << false << true << true << Type::Screencast;
        QTest::newRow("GNOME no ScreenCast") << "GNOME" << false << false << false << false << true << Type::Wayland;
        QTest::newRow("sway native") << "sway" << false << false << true << true << true << Type::Screencopy;
        QTest::newRow("sway native, hidden globals") << "sway" << false << false << false << true << true << Type::Wayland;
        QTest::newRow("sway Flatpak, globals shown") << "sway" << true << false << true << true << true << Type::Screencopy;
        QTest::newRow("sway Flatpak") << "sway" << true << false << false << true << true << Type::Screencast;
        QTest::newRow("sway Flatpak, old portal") << "sway" << true << false << false << false << true << Type::Wayland;
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
        QVERIFY(!CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::Screencopy));
        auto s = CaptureFactory::createStrategy(CaptureFactory::StrategyType::Screencopy);
        QVERIFY(s);
        QVERIFY(s->name() != QStringLiteral("Wayland Screencopy"));
        QVERIFY(CaptureFactory::getDefaultStrategyType() != CaptureFactory::StrategyType::Screencopy);
    }

    void theOverrideForcesScreencopy()
    {
        qputenv("SNIM_CAPTURE_STRATEGY", "wlroots");
        QCOMPARE(CaptureFactory::getDefaultStrategyType(), CaptureFactory::StrategyType::Screencopy);
        // Forced but unavailable still yields a working strategy.
        QVERIFY(CaptureFactory::createStrategy());
        qunsetenv("SNIM_CAPTURE_STRATEGY");
    }

    void theFrozenFrameFallsBackToTheScreenshotPortal_data()
    {
        using Type = CaptureFactory::StrategyType;
        QTest::addColumn<Type>("chosen");
        QTest::addColumn<QList<Type>>("chain");
        QTest::newRow("KDE") << Type::KWin << QList<Type>{Type::KWin, Type::Wayland};
        QTest::newRow("GNOME") << Type::Screencast << QList<Type>{Type::Screencast, Type::Wayland};
        QTest::newRow("wlroots") << Type::Screencopy << QList<Type>{Type::Screencopy, Type::Wayland};
        QTest::newRow("portal only") << Type::Wayland << QList<Type>{Type::Wayland};
        QTest::newRow("unresolved") << Type::Auto << QList<Type>{Type::Wayland};
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
        QCOMPARE(native->name(), QStringLiteral("Qt screen grab"));
        // Offscreen and no Wayland session: no screencopy and no ScreenCast session.
        qputenv("WAYLAND_DISPLAY", "");
        qputenv("XDG_SESSION_TYPE", "x11");
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::Screencopy));
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::Screencast));
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::KWin));
        QVERIFY(!FrameSourceFactory::create(CaptureFactory::StrategyType::Wayland));
#endif
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
