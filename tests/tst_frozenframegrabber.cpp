#include <QtTest>
#include <QGuiApplication>

#include "screen/FrozenFrameGrabber.h"

using namespace Screen;

// The grabber's synchronous path and its user-facing failure text; the Wayland
// frame sources are exercised by the manual frozen frame probe instead.
class tst_FrozenFrameGrabber : public QObject
{
    Q_OBJECT

private slots:
    void offWaylandTheFrameArrivesInsideGrab()
    {
        FrozenFrameGrabber grabber;
        bool called = false;
        grabber.grab([&called](const QPixmap &frozen, const QRect &virtualGeometry) {
            called = true;
            QVERIFY(!frozen.isNull());
            QVERIFY(!virtualGeometry.isEmpty());
        });
        QVERIFY(called);
        QVERIFY(grabber.lastError().isEmpty());
    }

    void gnomeIsToldToPickTheScreens()
    {
        const QString text = FrozenFrameGrabber::failureMessage(
            QStringLiteral("ubuntu:GNOME"), QStringLiteral("the Screenshot portal failed"), false);
        QVERIFY(text.contains(QStringLiteral("the Screenshot portal failed")));
        QVERIFY(text.contains(QStringLiteral("choose your screens")));
    }

    void kdeIsPointedAtDesktopIntegration()
    {
        const QString text = FrozenFrameGrabber::failureMessage(QStringLiteral("KDE"), QString(), false);
        QVERIFY(text.startsWith(QStringLiteral("Could not capture the screen for selection.")));
        QVERIFY(text.contains(QStringLiteral("Set up desktop integration")));
    }

    void otherDesktopsAreToldToCheckThePortal()
    {
        const QString text = FrozenFrameGrabber::failureMessage(QStringLiteral("sway"),
                                                                QStringLiteral("no portal"), false);
        QVERIFY(text.contains(QStringLiteral("xdg-desktop-portal")));
    }

    void aCancelSaysSoOnEveryDesktop()
    {
        for (const QString &desktop : {QStringLiteral("GNOME"), QStringLiteral("KDE"), QString()}) {
            const QString text = FrozenFrameGrabber::failureMessage(desktop, QStringLiteral("x"), true);
            QVERIFY(text.contains(QStringLiteral("cancelled")));
        }
    }

    void withoutADesktopTheMessageStaysPlain()
    {
        QCOMPARE(FrozenFrameGrabber::failureMessage(QString(), QString(), false),
                 QStringLiteral("Could not capture the screen for selection."));
    }
};

QTEST_MAIN(tst_FrozenFrameGrabber)
#include "tst_frozenframegrabber.moc"
