#include <QtTest>
#include <memory>

#include "recording/RecordingFactory.h"
#include "recording/RecordingStrategy.h"

using namespace Recording;

// Factory + Strategy: test only the side-effect-free surface. NEVER call start();
// the real macOS backend would grab the screen and pop the permission prompt.
class tst_RecordingFactory : public QObject
{
    Q_OBJECT

private slots:
    void createsStub()
    {
        auto s = RecordingFactory::createStrategy(RecordingFactory::StrategyType::Stub);
        QVERIFY(s);
        QCOMPARE(s->name(), QStringLiteral("Unsupported"));
        QVERIFY(!s->isAvailable());
        QVERIFY(!s->isRecording());
    }

    void autoProducesNonNull()
    {
        auto s = RecordingFactory::createStrategy();   // Auto always resolves (stub fallback)
        QVERIFY(s);
    }

    void availabilityByType()
    {
        QVERIFY(RecordingFactory::isStrategyAvailable(RecordingFactory::StrategyType::Auto));
        QVERIFY(RecordingFactory::isStrategyAvailable(RecordingFactory::StrategyType::Stub));
#if defined(NICESHOT_HAVE_MAC_RECORDER)
        QCOMPARE(RecordingFactory::getDefaultStrategyType(), RecordingFactory::StrategyType::Mac);
#elif defined(NICESHOT_HAVE_LINUX_RECORDER)
        QCOMPARE(RecordingFactory::getDefaultStrategyType(), RecordingFactory::StrategyType::Linux);
#endif
    }

#ifdef NICESHOT_HAVE_LINUX_RECORDER
    void createsLinuxOrFallsBack()
    {
        auto s = RecordingFactory::createStrategy(RecordingFactory::StrategyType::Linux);
        QVERIFY(s);
        // Headless environments have no ScreenCast portal, so the stub is a valid answer.
        QVERIFY(s->name() == QStringLiteral("Portal/GStreamer")
                || s->name() == QStringLiteral("Unsupported"));
    }
#endif
};

QTEST_MAIN(tst_RecordingFactory)
#include "tst_recordingfactory.moc"
