#include <QtTest>
#include <memory>

#include "capture/CaptureFactory.h"
#include "capture/StrategySelection.h"
#include "capture/strategies/CaptureStrategy.h"

using namespace Capture;

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
