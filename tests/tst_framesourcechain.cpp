#include <QtTest>

#include "FakeFrameSources.h"
#include "screen/sources/StrategySelection.h"

using namespace Screen;
using namespace FakeFrames;

// The one walk behind screenshots and the frozen frame, over fake sources.
class tst_FrameSourceChain : public QObject
{
    Q_OBJECT

private slots:
    void walksEachDesktopsChainInOrder_data()
    {
        QTest::addColumn<QString>("desktop");
        QTest::addColumn<bool>("flatpak");
        QTest::addColumn<QList<Type>>("asked");
        QTest::newRow("KDE") << "KDE" << false << QList<Type>{Type::KWin, Type::Portal};
        QTest::newRow("KDE Flatpak") << "KDE" << true << QList<Type>{Type::Portal};
        QTest::newRow("GNOME") << "GNOME" << false << QList<Type>{Type::Screencast, Type::Portal};
        QTest::newRow("sway") << "sway" << false << QList<Type>{Type::Screencopy, Type::Portal};
        QTest::newRow("X11") << "XFCE" << false << QList<Type>{Type::Native};
    }

    void walksEachDesktopsChainInOrder()
    {
        QFETCH(QString, desktop);
        QFETCH(bool, flatpak);
        QFETCH(QList<Type>, asked);

        // Every source this desktop has fails, so the walk shows its whole order.
        const bool wayland = desktop != QLatin1String("XFCE");
        StrategySelection::Probes probes;
        probes.kwin = [desktop] { return desktop == QLatin1String("KDE"); };
        probes.screencopy = [wayland] { return wayland; };
        probes.screencast = [wayland] { return wayland; };
        probes.portal = [wayland] { return wayland; };
        const Type chosen = StrategySelection::choose(desktop, flatpak, probes);

        Desktop fake;
        for (const Type type : {Type::KWin, Type::Screencast, Type::Screencopy, Type::Portal,
                                Type::Native})
            fake.offered.insert(type, failing());
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        QSignalSpy failed(&chain, &FrameSourceChain::failed);
        chain.grab(StrategySelection::frameSourceChain(chosen));

        QCOMPARE(fake.asked, asked);
        QCOMPARE(ready.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("broken"));
        QCOMPARE(failed.first().at(1).toBool(), false);
    }

    void aCancelStopsTheWalk()
    {
        Desktop fake;
        fake.offered.insert(Type::Screencast, cancelling());
        fake.offered.insert(Type::Portal, delivering());
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        QSignalSpy failed(&chain, &FrameSourceChain::failed);
        chain.grab({Type::Screencast, Type::Portal});

        QCOMPARE(fake.asked, QList<Type>{Type::Screencast});
        QCOMPARE(ready.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("dismissed"));
        QCOMPARE(failed.first().at(1).toBool(), true);
    }

    void aFailurePassesOnToTheNextSource()
    {
        Desktop fake;
        fake.offered.insert(Type::Screencopy, failing());
        fake.offered.insert(Type::Portal, delivering({300, 150}));
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        QSignalSpy failed(&chain, &FrameSourceChain::failed);
        chain.grab({Type::Screencopy, Type::Portal});

        QCOMPARE(fake.asked, (QList<Type>{Type::Screencopy, Type::Portal}));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().at(0).value<QPixmap>().size(), QSize(300, 150));
    }

    void aMissingSourceIsSkipped()
    {
        Desktop fake;
        fake.offered.insert(Type::Portal, delivering());
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        chain.grab({Type::Screencast, Type::Portal});

        QCOMPARE(fake.asked, QList<Type>{Type::Portal});
        QCOMPARE(ready.count(), 1);
    }

    void aWalkEndingOnAMissingSourceSaysSo()
    {
        Desktop fake;
        fake.offered.insert(Type::Screencopy, failing());
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy failed(&chain, &FrameSourceChain::failed);
        chain.grab({Type::Screencopy, Type::Portal});
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("broken"));
        QVERIFY(chain.endedOnMissingSource());

        // Nothing offered at all: the first missing source is the reason.
        fake.offered.clear();
        chain.grab({Type::Screencopy, Type::Portal});
        QCOMPARE(failed.count(), 2);
        QCOMPARE(failed.last().at(0).toString(), QStringLiteral("Wayland screencopy is not available"));
        QVERIFY(chain.endedOnMissingSource());

        // A source that ran and failed last is a failure, not a missing method.
        fake.offered.insert(Type::Portal, failing());
        chain.grab({Type::Screencopy, Type::Portal});
        QCOMPARE(failed.count(), 3);
        QVERIFY(!chain.endedOnMissingSource());
    }

    void aSourceThatWithdrawsAfterFailingIsSkippedForTheRun()
    {
        Desktop fake;
        fake.offered.insert(Type::Screencast, failing());
        fake.offered.insert(Type::Portal, delivering());
        fake.withdrawOnFailure.append(Type::Screencast);
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);

        chain.grab({Type::Screencast, Type::Portal});
        QCOMPARE(fake.asked, (QList<Type>{Type::Screencast, Type::Portal}));
        // Every later grab asks the factory again, so the failed source is not asked twice.
        chain.grab({Type::Screencast, Type::Portal});
        chain.grab({Type::Screencast, Type::Portal});
        QCOMPARE(fake.asked, (QList<Type>{Type::Screencast, Type::Portal, Type::Portal, Type::Portal}));
        QCOMPARE(ready.count(), 3);
    }

    void aCancelDoesNotWithdrawTheSource()
    {
        Desktop fake;
        fake.offered.insert(Type::Screencast, cancelling());
        fake.withdrawOnFailure.append(Type::Screencast);
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        chain.grab({Type::Screencast, Type::Portal});
        chain.grab({Type::Screencast, Type::Portal});
        QCOMPARE(fake.asked, (QList<Type>{Type::Screencast, Type::Screencast}));
    }

    void anAsynchronousWalkIgnoresAGrabInFlight()
    {
        Desktop fake;
        Script slowFailure = failing();
        slowFailure.async = true;
        Script slowFrame = delivering();
        slowFrame.async = true;
        fake.offered.insert(Type::Screencopy, slowFailure);
        fake.offered.insert(Type::Portal, slowFrame);
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        QSignalSpy failed(&chain, &FrameSourceChain::failed);

        chain.grab({Type::Screencopy, Type::Portal});
        chain.grab({Type::Portal});
        QCOMPARE(ready.count(), 0);
        QVERIFY(ready.wait(1000));
        QCOMPARE(fake.asked, (QList<Type>{Type::Screencopy, Type::Portal}));
        QCOMPARE(ready.count(), 1);
        QCOMPARE(failed.count(), 0);
    }

    void aPartialFrameGoesOutUnchecked()
    {
        // One monitor of two, picked in a system dialog.
        Desktop fake;
        const Script onePick = delivering({100, 50}, 1.0, QRect(0, 0, 200, 50));
        fake.offered.insert(Type::Portal, onePick);
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        chain.grab({Type::Portal});
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().at(0).value<QPixmap>().size(), QSize(100, 50));
        QCOMPARE(ready.first().at(1).toRect(), QRect(0, 0, 200, 50));
    }

    void aScreenPickerIsAnnounced()
    {
        Desktop fake;
        Script firstPick = delivering();
        firstPick.picker = true;
        fake.offered.insert(Type::Screencast, firstPick);
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy picker(&chain, &FrameSourceChain::sourcePickerExpected);
        chain.grab({Type::Screencast});
        QCOMPARE(picker.count(), 1);
        QCOMPARE(picker.first().at(0).toBool(), true);
    }

    void aTurnedDownFrameCountsAsAFailure()
    {
        Desktop fake;
        const Script onePick = delivering({100, 50}, 1.0, QRect(0, 0, 200, 50));
        fake.offered.insert(Type::Screencast, onePick);
        fake.offered.insert(Type::Portal, delivering());
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QList<Type> checked;
        chain.setFrameCheck([&checked](const QPixmap &frame, const QRect &geometry, Type type) {
            checked.append(type);
            return frameCoversGeometry(frame.size(), frame.devicePixelRatio(), geometry)
                       ? QString() : QStringLiteral("partial");
        });
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        QSignalSpy failed(&chain, &FrameSourceChain::failed);

        chain.grab({Type::Screencast, Type::Portal});
        QCOMPARE(checked, (QList<Type>{Type::Screencast, Type::Portal}));
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().at(0).value<QPixmap>().size(), QSize(200, 100));

        // Turned down to the end: the check's reason is the failure's.
        chain.grab({Type::Screencast});
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("partial"));
        QCOMPARE(failed.first().at(1).toBool(), false);
    }
};

QTEST_MAIN(tst_FrameSourceChain)
#include "tst_framesourcechain.moc"
