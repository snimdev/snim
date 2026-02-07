#include <QtTest>

#include <QGuiApplication>
#include <QPixmap>
#include <QPointer>
#include <QScreen>
#include <QSignalSpy>

#include "capture/AreaSelector.h"

using namespace Capture;

// The overlay's keyboard slice: Ctrl+C and Ctrl+S reach the same terminal actions as the
// Copy and Save toolbar buttons, and stay inert wherever the toolbar itself is unavailable.
class tst_AreaSelector : public QObject
{
    Q_OBJECT

private:
    // liveStateChanged reports the phase as an int: 0 = Idle, 1 = Dragging, 2 = Adjusting.
    static constexpr int kIdle = 0;
    static constexpr int kAdjusting = 2;

    QPointer<AreaSelector> m_sel;
    QRect m_screen;

    // Selections travel in virtual-desktop coords, so a local drag lands at the offset rect.
    QRect selectionFor(const QPoint &from, const QPoint &to) const
    {
        return QRect(from, to).normalized().translated(m_screen.topLeft());
    }

    void drag(const QPoint &from, const QPoint &to)
    {
        QSignalSpy state(m_sel.data(), &AreaSelector::liveStateChanged);
        QTest::mousePress(m_sel.data(), Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(m_sel.data(), to);
        QTest::mouseRelease(m_sel.data(), Qt::LeftButton, Qt::NoModifier, to);

        QVERIFY(!state.isEmpty());
        QCOMPARE(state.last().at(1).toInt(), kAdjusting);
        QCOMPARE(state.last().at(0).toRect(), selectionFor(from, to));
    }

private slots:
    void init()
    {
        m_screen = QGuiApplication::primaryScreen()->geometry();

        auto *sel = new AreaSelector;
        m_sel = sel;

        QPixmap shot(m_screen.size());
        shot.fill(Qt::darkGray);
        sel->setScreenshot(shot);
        sel->setVirtualGeometry(m_screen);
        sel->setScreenOffset(m_screen.topLeft());
        sel->setActionsEnabled(true);
        sel->setGeometry(m_screen);
        sel->show();
        QVERIFY(QTest::qWaitForWindowExposed(sel));
    }

    void cleanup()
    {
        delete m_sel.data();
    }

    void copyChordCopiesTheSelection()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QSignalSpy save(m_sel.data(), &AreaSelector::saveRequested);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::ControlModifier);

        QCOMPARE(copy.count(), 1);
        QCOMPARE(copy.at(0).at(0).toRect(), selectionFor(QPoint(40, 40), QPoint(240, 180)));
        QCOMPARE(save.count(), 0);
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel);
        QVERIFY(!m_sel->isVisible());
    }

    void saveChordSavesTheSelection()
    {
        drag(QPoint(60, 50), QPoint(300, 220));
        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QSignalSpy save(m_sel.data(), &AreaSelector::saveRequested);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_S, Qt::ControlModifier);

        QCOMPARE(save.count(), 1);
        QCOMPARE(save.at(0).at(0).toRect(), selectionFor(QPoint(60, 50), QPoint(300, 220)));
        QCOMPARE(copy.count(), 0);
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel);
        QVERIFY(!m_sel->isVisible());
    }

    void chordsStayInertWithoutActions()
    {
        m_sel->setActionsEnabled(false);   // the recording and OCR overlays
        drag(QPoint(40, 40), QPoint(240, 180));
        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QSignalSpy save(m_sel.data(), &AreaSelector::saveRequested);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::ControlModifier);
        QTest::keyClick(m_sel.data(), Qt::Key_S, Qt::ControlModifier);

        QCOMPARE(copy.count(), 0);
        QCOMPARE(save.count(), 0);
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel->isVisible());

        QTest::keyClick(m_sel.data(), Qt::Key_Return);

        QCOMPARE(area.count(), 1);
        QCOMPARE(area.at(0).at(0).toRect(), selectionFor(QPoint(40, 40), QPoint(240, 180)));
        QVERIFY(m_sel);
        QVERIFY(!m_sel->isVisible());
    }

    void chordsStayInertWhileIdle()
    {
        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QSignalSpy save(m_sel.data(), &AreaSelector::saveRequested);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::ControlModifier);
        QTest::keyClick(m_sel.data(), Qt::Key_S, Qt::ControlModifier);

        QCOMPARE(copy.count(), 0);
        QCOMPARE(save.count(), 0);
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel->isVisible());
    }

    void bareLetterAndExtraModifierDoNothing()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QSignalSpy save(m_sel.data(), &AreaSelector::saveRequested);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::NoModifier);
        QTest::keyClick(m_sel.data(), Qt::Key_S, Qt::NoModifier);
        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);

        QCOMPARE(copy.count(), 0);
        QCOMPARE(save.count(), 0);
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel->isVisible());
    }

    void enterStillCommitsTheSelection()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QSignalSpy save(m_sel.data(), &AreaSelector::saveRequested);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_Return);

        QCOMPARE(area.count(), 1);
        QCOMPARE(area.at(0).at(0).toRect(), selectionFor(QPoint(40, 40), QPoint(240, 180)));
        QCOMPARE(copy.count(), 0);
        QCOMPARE(save.count(), 0);
        QVERIFY(m_sel);
        QVERIFY(!m_sel->isVisible());
    }

    void escapeClearsThenCancels()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        QSignalSpy state(m_sel.data(), &AreaSelector::liveStateChanged);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_Escape);

        QVERIFY(!state.isEmpty());
        QCOMPARE(state.last().at(1).toInt(), kIdle);
        QVERIFY(state.last().at(0).toRect().isEmpty());
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel->isVisible());

        QTest::keyClick(m_sel.data(), Qt::Key_Escape);

        QCOMPARE(area.count(), 1);
        QVERIFY(area.at(0).at(0).toRect().isEmpty());
        QVERIFY(m_sel);
        QVERIFY(!m_sel->isVisible());
    }
};

QTEST_MAIN(tst_AreaSelector)

#include "tst_areaselector.moc"
