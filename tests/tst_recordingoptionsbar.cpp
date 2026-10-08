#include <QtTest>

#include <QAction>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QToolButton>

#include "core/Settings.h"
#include "record/RecordingOptionsBar.h"

using namespace Record;

// The options bar is built from real widgets and themed :/icons SVGs, so this runs on
// the offscreen platform with the qrc compiled in. The clicks go through the real
// QStyleSheetStyle hit testing, which is what the split-button regression was about:
// with QToolButton::MenuButtonPopup the menu subcontrol claimed the right third of the
// centered icon, so clicking the glyph opened the device menu instead of toggling.
class tst_RecordingOptionsBar : public QObject
{
    Q_OBJECT

    static QToolButton *button(QWidget *bar, const char *name)
    {
        return bar->findChild<QToolButton *>(QLatin1String(name));
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("SnimOptionsBarTest");
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        Core::Settings::setCameraEnabled(false);
        Core::Settings::setMicEnabled(false);
        Core::Settings::setCameraDeviceId(QByteArray());
    }

    void iconClickTogglesCameraEverywhere()
    {
        RecordingOptionsBar bar;
        bar.show();
        QToolButton *camera = button(&bar, "cameraButton");
        QVERIFY(camera);
        QVERIFY(!camera->isChecked());
        QSignalSpy spy(&bar, &RecordingOptionsBar::cameraToggled);

        // Every column of the toggle must toggle: the old split button swallowed the
        // rightmost 12px, which overlapped the drawn icon.
        for (int x = 0; x < camera->width(); ++x) {
            QTest::mouseClick(camera, Qt::LeftButton, Qt::NoModifier,
                              QPoint(x, camera->height() / 2));
            QVERIFY2(camera->isChecked(), qPrintable(QStringLiteral("press at x=%1").arg(x)));
            QCOMPARE(Core::Settings::cameraEnabled(), true);

            QTest::mouseClick(camera, Qt::LeftButton, Qt::NoModifier,
                              QPoint(x, camera->height() / 2));
            QVERIFY(!camera->isChecked());
            QCOMPARE(Core::Settings::cameraEnabled(), false);
        }
        QCOMPARE(spy.count(), 2 * camera->width());
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    void arrowButtonOpensTheDeviceMenu()
    {
        RecordingOptionsBar bar;
        bar.show();
        QToolButton *arrow = button(&bar, "cameraArrow");
        QVERIFY(arrow);
        QVERIFY(!arrow->isCheckable());   // the arrow must never flip the camera
        QSignalSpy spy(&bar, &RecordingOptionsBar::cameraToggled);
        QTest::mouseClick(arrow, Qt::LeftButton, Qt::NoModifier,
                          QPoint(arrow->width() / 2, arrow->height() / 2));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(button(&bar, "cameraButton")->isChecked(), false);
        if (QMenu *menu = bar.findChild<QMenu *>())
            menu->close();
    }

    void cameraMenuLeadsWithCameraOff()
    {
        Core::Settings::setCameraEnabled(true);
        RecordingOptionsBar bar;
        QToolButton *camera = button(&bar, "cameraButton");
        QVERIFY(camera->isChecked());
        QMenu *menu = camera->findChild<QMenu *>();
        QVERIFY(menu);

        emit menu->aboutToShow();   // the bar rebuilds the menu on open
        QVERIFY(!menu->actions().isEmpty());
        QAction *off = menu->actions().first();
        QCOMPARE(off->text(), QStringLiteral("Camera off"));
        QVERIFY(off->isCheckable());
        QVERIFY(!off->isChecked());   // the camera is on

        QSignalSpy spy(&bar, &RecordingOptionsBar::cameraToggled);
        off->trigger();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
        QVERIFY(!camera->isChecked());
        QCOMPARE(Core::Settings::cameraEnabled(), false);

        emit menu->aboutToShow();
        QVERIFY(menu->actions().first()->isChecked());   // now it reads as the active row
    }

    void micMenuLeadsWithMicrophoneOff()
    {
        Core::Settings::setMicEnabled(true);
        RecordingOptionsBar bar;
        QToolButton *mic = button(&bar, "micButton");
        QVERIFY(mic->isChecked());
        QMenu *menu = mic->findChild<QMenu *>();
        QVERIFY(menu);

        emit menu->aboutToShow();
        QAction *off = menu->actions().first();
        QCOMPARE(off->text(), QStringLiteral("Microphone off"));
        off->trigger();
        QVERIFY(!mic->isChecked());
        QCOMPARE(Core::Settings::micEnabled(), false);
    }

    void cameraMenuHandlesNoDevices()
    {
        RecordingOptionsBar bar;
        QMenu *menu = button(&bar, "cameraButton")->findChild<QMenu *>();
        emit menu->aboutToShow();

        const QList<QAction *> actions = menu->actions();
        QCOMPARE(actions.first()->text(), QStringLiteral("Camera off"));
        if (QMediaDevices::videoInputs().isEmpty()) {
            QCOMPARE(actions.last()->text(), QStringLiteral("No camera found"));
            QVERIFY(!actions.last()->isEnabled());
        } else {
            // A device is present on this machine: the fallback row must be absent.
            for (QAction *a : actions)
                QVERIFY(a->text() != QStringLiteral("No camera found"));
        }
    }
};

QTEST_MAIN(tst_RecordingOptionsBar)
#include "tst_recordingoptionsbar.moc"
