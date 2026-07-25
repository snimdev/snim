#include <QtTest>

#include "screen/WindowEnumerator.h"
#include "screen/X11WindowFilter.h"

using namespace Screen::X11WindowFilter;

// Which X11 client windows the window picker offers, and what of each it shows.
class tst_X11WindowFilter : public QObject
{
    Q_OBJECT

private slots:
    void offersAViewableAppWindow()
    {
        Candidate w;
        w.viewable = true;
        w.bounds = QRect(100, 100, 640, 480);
        QVERIFY(isPickable(w));
    }

    void skipsWhatIsNotAWindowOnScreen()
    {
        Candidate base;
        base.viewable = true;
        base.bounds = QRect(100, 100, 640, 480);

        Candidate unmapped = base;
        unmapped.viewable = false;   // on another desktop, or not shown yet
        QVERIFY(!isPickable(unmapped));

        Candidate minimized = base;
        minimized.hidden = true;
        QVERIFY(!isPickable(minimized));

        Candidate panel = base;
        panel.desktopOrDock = true;   // the wallpaper window, panels and docks
        QVERIFY(!isPickable(panel));

        Candidate own = base;
        own.own = true;   // the camera bubble, an editor left open
        QVERIFY(!isPickable(own));

        Candidate empty = base;
        empty.bounds = QRect();
        QVERIFY(!isPickable(empty));
    }

    void addsTheWindowManagersFrame()
    {
        // xfwm4: 5 px borders and a 29 px title bar around the client.
        QCOMPARE(visibleRect(QRect(105, 129, 500, 300), QMargins(5, 29, 5, 5), {}),
                 QRect(100, 100, 510, 334));
    }

    void dropsAClientDrawnShadow()
    {
        // A client-side decorated window draws its shadow inside its own rect.
        QCOMPARE(visibleRect(QRect(80, 70, 560, 380), {}, QMargins(20, 15, 20, 25)),
                 QRect(100, 85, 520, 340));
    }

    void cropsTheRecordedWindowDownToWhatIsVisible()
    {
        // The frame window of a manager with invisible resize borders, 10 px all round.
        const QRect frame(90, 90, 530, 354);
        const QRect visible = visibleRect(QRect(105, 129, 500, 300), QMargins(5, 29, 5, 5), {});
        QCOMPARE(cropMargins(frame, visible), QMargins(10, 10, 10, 10));
        // Recording the window itself when it is all that is visible crops nothing.
        QCOMPARE(cropMargins(visible, visible), QMargins());
        // A shadow inside the client is cropped off a frameless client.
        const QRect csd(80, 70, 560, 380);
        QCOMPARE(cropMargins(csd, visibleRect(csd, {}, QMargins(20, 15, 20, 25))),
                 QMargins(20, 15, 20, 25));
        QCOMPARE(cropMargins(frame, QRect(2000, 0, 10, 10)), QMargins());
    }

    void theStubOrAnX11LessSessionOffersNoWindows()
    {
        // The offscreen platform has no X connection: the picker falls back to screens.
        QVERIFY(Screen::enumerateWindowInfos().isEmpty());
    }
};

QTEST_MAIN(tst_X11WindowFilter)
#include "tst_x11windowfilter.moc"
