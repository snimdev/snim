#include <QtTest>
#include <QCheckBox>
#include <QSpinBox>

#include "editor/video/AnimationOptionsDialog.h"

using namespace Editor::Video;

// The export options dialog is never exec()d here: values go straight into its fields.
class tst_AnimationOptionsDialog : public QObject
{
    Q_OBJECT

private slots:
    void showsTheInitialParams()
    {
        AnimationParams initial;
        initial.fps = 15;
        initial.maxWidth = 0;
        initial.quality = 40;
        initial.lossless = true;
        initial.loopCount = 3;
        AnimationOptionsDialog dialog(AnimationFormat::WebP, initial);

        QCOMPARE(dialog.findChild<QSpinBox *>("fps")->value(), 15);
        QCOMPARE(dialog.findChild<QSpinBox *>("maxWidth")->text(), QStringLiteral("Original"));
        QCOMPARE(dialog.findChild<QSpinBox *>("loop")->value(), 3);
        QVERIFY(dialog.findChild<QCheckBox *>("lossless")->isChecked());
        QVERIFY(!dialog.findChild<QSpinBox *>("quality")->isEnabled());
        QCOMPARE(dialog.windowTitle(), QStringLiteral("WebP Options"));
    }

    void returnsTheEditedParams()
    {
        AnimationParams initial;
        initial.effort = 6;
        AnimationOptionsDialog dialog(AnimationFormat::WebP, initial);
        dialog.findChild<QSpinBox *>("fps")->setValue(24);
        dialog.findChild<QSpinBox *>("maxWidth")->setValue(800);
        dialog.findChild<QSpinBox *>("quality")->setValue(90);
        dialog.findChild<QSpinBox *>("loop")->setValue(0);
        dialog.findChild<QCheckBox *>("lossless")->setChecked(true);
        dialog.findChild<QCheckBox *>("skip")->setChecked(true);

        const AnimationParams p = dialog.params();
        QCOMPARE(p.fps, 24);
        QCOMPARE(p.maxWidth, 800);
        QCOMPARE(p.quality, 90);
        QCOMPARE(p.loopCount, 0);
        QVERIFY(p.lossless);
        QCOMPARE(p.effort, 6);   // not shown, carried over
        QVERIFY(dialog.skipNextTime());
    }

    void gifHasNoLosslessOrQuality()
    {
        AnimationParams initial;
        initial.lossless = true;
        AnimationOptionsDialog dialog(AnimationFormat::Gif, initial);
        QVERIFY(dialog.findChild<QCheckBox *>("lossless")->isHidden());
        QVERIFY(dialog.findChild<QSpinBox *>("quality")->isHidden());
        QVERIFY(!dialog.params().lossless);
        QVERIFY(!dialog.skipNextTime());
        QCOMPARE(dialog.windowTitle(), QStringLiteral("GIF Options"));
    }
};

QTEST_MAIN(tst_AnimationOptionsDialog)
#include "tst_animationoptionsdialog.moc"
