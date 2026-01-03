#include <QtTest>
#include <memory>

#include "capture/CaptureFactory.h"
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
#ifdef Q_OS_MACOS
        QVERIFY(CaptureFactory::getDefaultStrategyType() == CaptureFactory::StrategyType::Native);
        QVERIFY(!CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::KWin));
        QVERIFY(!CaptureFactory::isStrategyAvailable(CaptureFactory::StrategyType::Wayland));
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
