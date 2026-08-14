#include <QtTest>

#include <QMap>
#include <QPixmap>
#include <QTimer>
#include <memory>

#include "screen/sources/DesktopFrameSource.h"
#include "screen/sources/FrameSourceChain.h"
#include "screen/sources/StrategySelection.h"

using namespace Screen;
using Type = SourceType;

namespace {

// What a fake source answers when grabbed.
struct Script {
    enum Kind { Frame, Fail, Cancel } kind = Frame;
    QSize pixels{200, 100};
    qreal dpr = 2.0;
    QRect geometry{0, 0, 100, 50};
    bool async = false;
};

class FakeSource : public DesktopFrameSource
{
public:
    FakeSource(const Script &script, QObject *parent) : DesktopFrameSource(parent), m_script(script) {}

    void grab() override
    {
        if (m_script.async)
            QTimer::singleShot(0, this, [this] { answer(); });
        else
            answer();
    }

private:
    void answer()
    {
        switch (m_script.kind) {
        case Script::Frame: {
            QPixmap frame(m_script.pixels);
            frame.fill(Qt::red);
            frame.setDevicePixelRatio(m_script.dpr);
            emit frameReady(frame, m_script.geometry);
            return;
        }
        case Script::Fail:
            emit frameFailed(QStringLiteral("broken"), false);
            return;
        case Script::Cancel:
            emit frameFailed(QStringLiteral("dismissed"), true);
            return;
        }
    }

    Script m_script;
};

// The sources one desktop offers, by type, and which of them the walk asked.
struct FakeDesktop {
    QMap<Type, Script> offered;
    QList<Type> asked;
    // Like the ScreenCast source: once it fails (a cancel aside), it stops offering itself.
    QList<Type> withdrawOnFailure;

    FrameSourceChain::Factory factory()
    {
        return [this](Type type, QObject *parent) -> std::unique_ptr<DesktopFrameSource> {
            if (!offered.contains(type))
                return nullptr;
            asked.append(type);
            auto source = std::make_unique<FakeSource>(offered.value(type), parent);
            if (withdrawOnFailure.contains(type))
                QObject::connect(source.get(), &DesktopFrameSource::frameFailed,
                                 [this, type](const QString &, bool cancelled) {
                                     if (!cancelled)
                                         offered.remove(type);
                                 });
            return source;
        };
    }
};

Script failing()
{
    return {Script::Fail};
}

Script delivering(QSize pixels = {200, 100})
{
    Script script;
    script.pixels = pixels;
    return script;
}

} // namespace

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

        FakeDesktop fake;
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
        FakeDesktop fake;
        fake.offered.insert(Type::Screencast, {Script::Cancel});
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
        FakeDesktop fake;
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
        FakeDesktop fake;
        fake.offered.insert(Type::Portal, delivering());
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        chain.grab({Type::Screencast, Type::Portal});

        QCOMPARE(fake.asked, QList<Type>{Type::Portal});
        QCOMPARE(ready.count(), 1);
    }

    void aWalkEndingOnAMissingSourceSaysSo()
    {
        FakeDesktop fake;
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
        FakeDesktop fake;
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
        FakeDesktop fake;
        fake.offered.insert(Type::Screencast, {Script::Cancel});
        fake.withdrawOnFailure.append(Type::Screencast);
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        chain.grab({Type::Screencast, Type::Portal});
        chain.grab({Type::Screencast, Type::Portal});
        QCOMPARE(fake.asked, (QList<Type>{Type::Screencast, Type::Screencast}));
    }

    void anAsynchronousWalkIgnoresAGrabInFlight()
    {
        FakeDesktop fake;
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
        FakeDesktop fake;
        Script onePick = delivering({100, 50});
        onePick.dpr = 1.0;
        onePick.geometry = QRect(0, 0, 200, 50);
        fake.offered.insert(Type::Portal, onePick);
        FrameSourceChain chain(QStringLiteral("Test"), fake.factory());
        QSignalSpy ready(&chain, &FrameSourceChain::frameReady);
        chain.grab({Type::Portal});
        QCOMPARE(ready.count(), 1);
        QCOMPARE(ready.first().at(0).value<QPixmap>().size(), QSize(100, 50));
        QCOMPARE(ready.first().at(1).toRect(), QRect(0, 0, 200, 50));
    }

    void aTurnedDownFrameCountsAsAFailure()
    {
        FakeDesktop fake;
        Script onePick = delivering({100, 50});
        onePick.dpr = 1.0;
        onePick.geometry = QRect(0, 0, 200, 50);
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
