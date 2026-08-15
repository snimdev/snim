#include <QtTest>
#include <QApplication>
#include <QPointer>
#include <QScreen>
#include <QTemporaryDir>

#include "screen/AreaSelector.h"
#include "screen/SelectorGroup.h"

using namespace Screen;

// The per-screen overlays of one selection on a two-screen offscreen desktop: a 100% screen
// and a 150% one to its right. Each overlay gets its screen, a selection on one shows on the
// other, and whichever ends it closes them all before the group signals.
class tst_SelectorGroup : public QObject
{
    Q_OBJECT

    QPixmap m_frame;
    QRect m_desktop;

    QPointer<SelectorGroup> open(const SelectorGroup::Options &options = {})
    {
        auto *group = new SelectorGroup(m_frame, m_desktop, options, this);
        for (AreaSelector *selector : group->selectors()) {
            if (!QTest::qWaitForWindowExposed(selector))
                return nullptr;
        }
        return group;
    }

    static QList<AreaSelector *> visibleSelectors()
    {
        QList<AreaSelector *> selectors;
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *selector = qobject_cast<AreaSelector *>(widget); selector && selector->isVisible())
                selectors.append(selector);
        }
        return selectors;
    }

    // How bright the overlay paints a point of the virtual desktop.
    static int grayAt(AreaSelector *selector, const QPoint &virt)
    {
        const QImage painted = selector->grab().toImage();
        const QPoint local = (virt - selector->geometry().topLeft()) * painted.devicePixelRatio();
        return qGray(painted.pixel(local));
    }

    // A drag on one overlay in virtual coordinates; the pointer may leave it, as a grab allows.
    static void drag(AreaSelector *selector, const QPoint &from, const QPoint &to)
    {
        const QPoint origin = selector->geometry().topLeft();
        QTest::mousePress(selector, Qt::LeftButton, Qt::NoModifier, from - origin);
        QTest::mouseMove(selector, to - origin);
        QTest::mouseRelease(selector, Qt::LeftButton, Qt::NoModifier, to - origin);
    }

private slots:
    void initTestCase()
    {
        QCOMPARE(QGuiApplication::screens().size(), 2);
        for (const QScreen *screen : QGuiApplication::screens())
            m_desktop = m_desktop.united(screen->geometry());
        QCOMPARE(m_desktop, QRect(0, 0, 1600, 600));
        m_frame = QPixmap(m_desktop.size() * 1.5);
        m_frame.setDevicePixelRatio(1.5);
        m_frame.fill(Qt::white);
    }

    void cleanup()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(visibleSelectors().isEmpty());
    }

    void opensOneOverlayPerScreen()
    {
        const QPointer<SelectorGroup> group = open();
        QVERIFY(group);
        QCOMPARE(group->selectors().size(), 2);
        for (int i = 0; i < 2; ++i) {
            AreaSelector *selector = group->selectors().at(i);
            QCOMPARE(selector->geometry(), QGuiApplication::screens().at(i)->geometry());
            QVERIFY(!selector->isFullScreen());   // only Wayland asks for fullscreen
        }
        group->selectors().first()->cancelSelection();
    }

    void aSpanningSelectionShowsOnEveryOverlay()
    {
        const QPointer<SelectorGroup> group = open();
        QVERIFY(group);
        AreaSelector *left = group->selectors().at(0);
        AreaSelector *right = group->selectors().at(1);
        const int dimmed = grayAt(right, QPoint(900, 300));
        QSignalSpy live(group.data(), &SelectorGroup::liveStateChanged);

        drag(left, QPoint(600, 200), QPoint(1000, 400));
        QVERIFY(!live.isEmpty());
        QCOMPARE(live.last().at(0).value<AreaSelector *>(), left);
        QCOMPARE(live.last().at(1).toRect(), QRect(QPoint(600, 200), QPoint(1000, 400)));

        // The right overlay never saw the pointer, yet it lights its part of the selection.
        QVERIFY(grayAt(right, QPoint(900, 300)) > dimmed + 60);
        QCOMPARE(grayAt(right, QPoint(1400, 500)), dimmed);
        left->cancelSelection();
    }

    void anEndingClosesEveryOverlayFirst()
    {
        const QPointer<SelectorGroup> group = open();
        QVERIFY(group);
        int signalled = 0;
        connect(group.data(), &SelectorGroup::areaSelected, this, [&signalled](const QRect &area) {
            ++signalled;
            QCOMPARE(area, QRect(QPoint(100, 100), QPoint(300, 200)));
            QVERIFY(visibleSelectors().isEmpty());
        });
        AreaSelector *left = group->selectors().at(0);
        drag(left, QPoint(100, 100), QPoint(300, 200));
        QTest::keyClick(left, Qt::Key_Return);
        QCOMPARE(signalled, 1);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!group);   // it deletes itself once done
    }

    void theCopyChordEndsTheSelection()
    {
        SelectorGroup::Options options;
        options.actions = true;
        const QPointer<SelectorGroup> group = open(options);
        QVERIFY(group);
        QSignalSpy copy(group.data(), &SelectorGroup::copyRequested);
        QSignalSpy area(group.data(), &SelectorGroup::areaSelected);
        AreaSelector *right = group->selectors().at(1);
        drag(right, QPoint(900, 100), QPoint(1100, 300));
        const QKeyCombination combo = QKeySequence::keyBindings(QKeySequence::Copy).first()[0];
        QTest::keyClick(right, combo.key(), combo.keyboardModifiers());
        QCOMPARE(copy.count(), 1);
        QCOMPARE(area.count(), 0);
        QVERIFY(visibleSelectors().isEmpty());
    }

    void aWindowPickSendsThePickAlone()
    {
        SelectorGroup::Options options;
        options.mode = AreaSelector::Mode::WindowPick;
        options.windows = {{QRect(850, 50, 400, 300), 42}};
        const QPointer<SelectorGroup> group = open(options);
        QVERIFY(group);
        QSignalSpy picked(group.data(), &SelectorGroup::windowPicked);
        QSignalSpy area(group.data(), &SelectorGroup::areaSelected);
        AreaSelector *right = group->selectors().at(1);
        QTest::mouseMove(right, QPoint(200, 100));
        QTest::mouseClick(right, Qt::LeftButton, Qt::NoModifier, QPoint(200, 100));
        QCOMPARE(picked.count(), 1);
        QCOMPARE(picked.first().at(0).toRect(), QRect(850, 50, 400, 300));
        QCOMPARE(picked.first().at(1).value<quint64>(), quint64(42));
        QCOMPARE(area.count(), 0);
        QVERIFY(visibleSelectors().isEmpty());
    }

    void escapeOnAWindowPickIsAnEmptyArea()
    {
        SelectorGroup::Options options;
        options.mode = AreaSelector::Mode::WindowPick;
        const QPointer<SelectorGroup> group = open(options);
        QVERIFY(group);
        QSignalSpy picked(group.data(), &SelectorGroup::windowPicked);
        QSignalSpy area(group.data(), &SelectorGroup::areaSelected);
        QTest::keyClick(group->selectors().at(0), Qt::Key_Escape);
        QCOMPARE(area.count(), 1);
        QVERIFY(area.first().first().toRect().isEmpty());
        QCOMPARE(picked.count(), 0);
    }

    void aLayerSurfaceGroupShowsAsAPlainOverlay()
    {
        // Off Wayland the layer surface is a no-op: the recording selector still covers each screen.
        SelectorGroup::Options options;
        options.layerSurface = true;
        const QPointer<SelectorGroup> group = open(options);
        QVERIFY(group);
        QCOMPARE(group->selectors().size(), 2);
        QVERIFY(!group->selectors().first()->isFullScreen());
        group->selectors().first()->cancelSelection();
    }

    void destroyingTheGroupClosesItsOverlays()
    {
        QPointer<SelectorGroup> group = open();
        QVERIFY(group);
        delete group.data();
        QVERIFY(visibleSelectors().isEmpty());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir dir;
    QFile config(dir.filePath(QStringLiteral("screens.json")));
    if (!config.open(QIODevice::WriteOnly))
        return 1;
    config.write(R"({
        "windowFrameMargins": false,
        "screens": [
            { "name": "left", "x": 0, "y": 0, "width": 800, "height": 600,
              "logicalDpi": 96, "logicalBaseDpi": 96 },
            { "name": "right", "x": 800, "y": 0, "width": 1200, "height": 900,
              "logicalDpi": 144, "logicalBaseDpi": 96 }
        ]
    })");
    config.close();

    // The platform argument splits on ':', so a Windows drive letter cannot be in it.
    const QString cwd = QDir::currentPath();
    QDir::setCurrent(dir.path());
    qputenv("QT_QPA_PLATFORM", "offscreen:configfile=screens.json");
    QApplication app(argc, argv);
    QDir::setCurrent(cwd);

    tst_SelectorGroup test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_selectorgroup.moc"
