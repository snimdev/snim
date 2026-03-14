#include <QtTest>

#include "editor/video/AnimationParams.h"

using namespace Editor::Video;

// Pure animation-export math (frame planning, downscale, filename). The encoders
// have their own tests; this locks the logic that feeds them.
class tst_AnimationParams : public QObject
{
    Q_OBJECT

private slots:
    void planFrames_basic()
    {
        // 2 s span at 10 fps -> 20 frames, first at inMs, evenly spaced, last < outMs.
        const QVector<qint64> t = planAnimationFrames(1000, 3000, 10);
        QCOMPARE(t.size(), 20);
        QCOMPARE(t.first(), qint64(1000));
        QVERIFY(t.last() < 3000);
        QCOMPARE(t.at(1) - t.at(0), qint64(100));   // 1/10 s step
    }

    void planFrames_flooring()
    {
        // 1.25 s at 10 fps floors to 12 frames (not 13).
        QCOMPARE(planAnimationFrames(0, 1250, 10).size(), 12);
    }

    void planFrames_subSecondAndDegenerate()
    {
        QCOMPARE(planAnimationFrames(0, 150, 10).size(), 1);   // <1 frame worth -> floor to 1
        QCOMPARE(planAnimationFrames(500, 500, 10).size(), 1); // empty span -> single frame
        const QVector<qint64> z = planAnimationFrames(500, 500, 10);
        QCOMPARE(z.first(), qint64(500));                      // sits at inMs, no out-1 underflow
    }

    void scaledSize()
    {
        QCOMPARE(animationScaledSize(QSize(1920, 1080), 600), QSize(600, 338));  // longest edge capped
        QCOMPARE(animationScaledSize(QSize(1080, 1920), 600), QSize(338, 600));  // portrait
        QCOMPARE(animationScaledSize(QSize(400, 300), 600), QSize(400, 300));    // no upscale
        QCOMPARE(animationScaledSize(QSize(400, 300), 0), QSize(400, 300));      // 0 = passthrough
        QCOMPARE(animationScaledSize(QSize(), 600), QSize());                    // empty passthrough
    }

    void clamping()
    {
        const AnimationParams g = AnimationParams{99, -5, -1}.clamped();
        QCOMPARE(g.fps, 50);
        QCOMPARE(g.maxWidth, 0);
        QCOMPARE(g.loopCount, 0);
        QCOMPARE(g.quality, 75);        // untouched default
        QVERIFY(!g.lossless);

        const AnimationParams hi = AnimationParams{10, 600, 0, 500, true}.clamped();
        QCOMPARE(hi.quality, 100);      // ceiling
        QVERIFY(hi.lossless);
        const AnimationParams lo = AnimationParams{10, 600, 0, -1, false}.clamped();
        QCOMPARE(lo.quality, 0);        // floor

        QCOMPARE(g.effort, 4);          // untouched default
        QVERIFY(g.minimizeSize);
        const AnimationParams fast = AnimationParams{10, 600, 0, 75, false, -2, false}.clamped();
        QCOMPARE(fast.effort, 0);       // floor
        QVERIFY(!fast.minimizeSize);    // a plain flag, never clamped
        const AnimationParams slow = AnimationParams{10, 600, 0, 75, false, 9, true}.clamped();
        QCOMPARE(slow.effort, 6);       // ceiling
    }

    void fileName()
    {
        QCOMPARE(animationFileNameFor("Snim_recording_2026.mp4", QStringLiteral("gif")),
                 QStringLiteral("Snim_recording_2026.gif"));
        QCOMPARE(animationFileNameFor("/tmp/clip.mov", QStringLiteral("gif")),
                 QStringLiteral("clip.gif"));
        QCOMPARE(animationFileNameFor("", QStringLiteral("gif")), QStringLiteral("recording.gif"));
        QCOMPARE(animationFileNameFor("/tmp/clip.mov", QStringLiteral("webp")),
                 QStringLiteral("clip.webp"));
    }
};

QTEST_MAIN(tst_AnimationParams)
#include "tst_animationparams.moc"
