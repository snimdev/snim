#include <QtTest>

#include "recording/strategies/LinuxPipeline.h"

using namespace Recording::LinuxPipeline;

class tst_LinuxPipeline : public QObject
{
    Q_OBJECT

private slots:
    void x11OnlyInAnX11Session()
    {
        QCOMPARE(videoSourceFor(u"xcb", false, true, true), VideoSource::X11);
        QCOMPARE(videoSourceFor(u"xcb", false, true, false), VideoSource::X11);
        QCOMPARE(videoSourceFor(u"xcb", true, true, true), VideoSource::Portal);   // XWayland
        QCOMPARE(videoSourceFor(u"wayland", true, true, true), VideoSource::Portal);
        QCOMPARE(videoSourceFor(u"offscreen", false, true, true), VideoSource::Portal);
        QCOMPARE(videoSourceFor(u"", false, true, true), VideoSource::Portal);
    }

    void x11WithoutXimagesrcFallsBackToThePortal()
    {
        QCOMPARE(videoSourceFor(u"xcb", false, false, true), VideoSource::Portal);
        // Neither works: name ximagesrc, the piece an X11 desktop is likelier to get.
        QCOMPARE(videoSourceFor(u"xcb", false, false, false), VideoSource::X11);
        QCOMPARE(videoSourceFor(u"wayland", true, false, false), VideoSource::Portal);
    }

    void eachSourceNeedsOnlyItsOwnElement()
    {
        const QList<const char *> x11 = requiredElements(VideoSource::X11);
        const QList<const char *> portal = requiredElements(VideoSource::Portal);
        const auto has = [](const QList<const char *> &list, const char *name) {
            for (const char *element : list) {
                if (qstrcmp(element, name) == 0)
                    return true;
            }
            return false;
        };
        QVERIFY(has(x11, "ximagesrc"));
        QVERIFY(!has(x11, "pipewiresrc"));
        QVERIFY(has(portal, "pipewiresrc"));
        QVERIFY(!has(portal, "ximagesrc"));
        for (const char *shared : {"videorate", "videocrop", "videoscale", "videoconvert",
                                   "capsfilter", "valve", "mp4mux"}) {
            QVERIFY(has(x11, shared));
            QVERIFY(has(portal, shared));
        }
    }

    void x11SourceUsesInclusiveEnds()
    {
        QCOMPARE(x11Source(QRect(100, 50, 640, 360), false, 30),
                 QStringLiteral("ximagesrc name=src use-damage=false show-pointer=false "
                                "startx=100 starty=50 endx=739 endy=409 "
                                "! video/x-raw,framerate=30/1"));
        QVERIFY(x11Source(QRect(0, 0, 3840, 1080), true, 60)
                    .contains(QStringLiteral("show-pointer=true startx=0 starty=0 "
                                             "endx=3839 endy=1079")));
    }

    void bothSourcesShareTheChain()
    {
        const QString x11 = videoChain(x11Source(QRect(0, 0, 64, 64), true, 30), 30,
                                       QStringLiteral("x264enc"), QStringLiteral("mp4mux"));
        const QString portal = videoChain(portalSource(), 30, QStringLiteral("x264enc"),
                                          QStringLiteral("mp4mux"));
        QVERIFY(x11.startsWith(QStringLiteral("ximagesrc name=src ")));
        QVERIFY(portal.startsWith(QStringLiteral("pipewiresrc name=src ")));

        const QString tail = QStringLiteral(" ! valve name=videovalve drop=false "
                                            "! videorate drop-only=true max-rate=30 "
                                            "skip-to-first=true ! videocrop name=crop "
                                            "! videoscale ! videoconvert ! capsfilter "
                                            "name=outcaps caps=video/x-raw,pixel-aspect-ratio=1/1 "
                                            "! queue ! x264enc ! queue ! mp4mux name=mux "
                                            "! filesink name=sink");
        QVERIFY(x11.endsWith(tail));
        QVERIFY(portal.endsWith(tail));
    }
};

QTEST_MAIN(tst_LinuxPipeline)
#include "tst_linuxpipeline.moc"
