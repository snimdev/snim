#include <QtTest>

#include <QApplication>
#include <QGuiApplication>
#include <QPixmap>
#include <QPointer>
#include <QScreen>
#include <QSharedPointer>
#include <QSignalSpy>
#include <memory>

#include "capture/AreaSelector.h"
#include "capture/OverlayAnnotations.h"
#include "editor/annotations/ToolRegistry.h"
#include "editor/annotations/tools/HighlightTool.h"
#include "editor/annotations/tools/RectangleTool.h"
#include "editor/annotations/tools/StepTool.h"

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
    QPixmap m_shot;
    QColor m_stroke;

    // The area capture's session: built from the overlay's frame, virtual geometry = screen.
    QSharedPointer<OverlayAnnotations> attachSession()
    {
        auto session = QSharedPointer<OverlayAnnotations>::create(m_shot, m_screen);
        m_sel->setAnnotations(session);
        return session;
    }

    // Presses the platform's first binding for a standard chord.
    void chord(QKeySequence::StandardKey key)
    {
        const QKeyCombination combo = QKeySequence::keyBindings(key).first()[0];
        QTest::keyClick(m_sel.data(), combo.key(), combo.keyboardModifiers());
    }

    void stroke(const QPoint &from, const QPoint &to)
    {
        QTest::mousePress(m_sel.data(), Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(m_sel.data(), to);
        QTest::mouseRelease(m_sel.data(), Qt::LeftButton, Qt::NoModifier, to);
    }

    // What a real double-click delivers: the second press arrives before the double-click.
    void doubleClick(const QPoint &pos)
    {
        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, pos);
        QTest::mousePress(m_sel.data(), Qt::LeftButton, Qt::NoModifier, pos);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, pos, m_sel->mapToGlobal(pos), Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(m_sel.data(), &dbl);
        QTest::mouseRelease(m_sel.data(), Qt::LeftButton, Qt::NoModifier, pos);
    }

    static bool near(const QColor &a, const QColor &b)
    {
        return qAbs(a.red() - b.red()) < 40 && qAbs(a.green() - b.green()) < 40
               && qAbs(a.blue() - b.blue()) < 40;
    }

    // An interior drag with no tool armed moves the selection by the drag delta.
    void verifyInteriorDragMoves(const QRect &sel)
    {
        QSignalSpy state(m_sel.data(), &AreaSelector::liveStateChanged);
        stroke(QPoint(100, 100), QPoint(130, 120));
        QVERIFY(!state.isEmpty());
        QCOMPARE(state.last().at(1).toInt(), kAdjusting);
        QCOMPARE(state.last().at(0).toRect(), sel.translated(30, 20));
    }

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
    void initTestCase()
    {
        QCoreApplication::setOrganizationName("SnimTest");
        QCoreApplication::setApplicationName("tst_areaselector");
        QStandardPaths::setTestModeEnabled(true);
        std::unique_ptr<Editor::Tools::ITool> tmpl(
            Editor::ToolRegistry::find("rectangle")->makeTemplate());
        m_stroke = dynamic_cast<Editor::Tools::RectangleTool*>(tmpl.get())->pen().color();
        QVERIFY(!near(m_stroke, QColor(Qt::darkGray)));
    }

    void init()
    {
        m_screen = QGuiApplication::primaryScreen()->geometry();

        auto *sel = new AreaSelector;
        m_sel = sel;

        m_shot = QPixmap(m_screen.size());
        m_shot.fill(Qt::darkGray);
        sel->setScreenshot(m_shot);
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

    // With a session attached, an armed tool turns interior presses into strokes.
    void drawnRectangleIsInTheCopy()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        QCOMPARE(session->activeTool(), QStringLiteral("rectangle"));

        stroke(QPoint(80, 80), QPoint(160, 140));
        QVERIFY(session->hasItems());

        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(copy.count(), 1);

        // Crop-local: the rectangle spans (40,40)-(120,100).
        const QRect area = copy.at(0).at(0).toRect();
        const QImage out = session->flattenedCrop(area).toImage();
        QVERIFY(near(out.pixelColor(40, 70), m_stroke));
        QVERIFY(near(out.pixelColor(120, 70), m_stroke));
        QCOMPARE(out.pixelColor(80, 70), QColor(Qt::darkGray));
    }

    void clicksDrawInsteadOfCommittingWithAToolArmed()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QCOMPARE(area.count(), 0);
        QTest::mouseDClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(120, 120));
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel->isVisible());
    }

    void doubleClickDrawsOnlyOnce()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        session->setActiveTool("step");   // one item per press, so an extra press shows
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        doubleClick(QPoint(120, 120));
        QCOMPARE(area.count(), 0);
        QCOMPARE(session->snapshot(selectionFor(QPoint(40, 40), QPoint(240, 180))).size(), 2);
        QVERIFY(m_sel->isVisible());
    }

    void stepClicksStampOneThenTwo()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_N);
        QCOMPARE(session->activeTool(), QStringLiteral("step"));
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(80, 80));
        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(140, 80));
        QCOMPARE(area.count(), 0);

        const Editor::AnnotationSet set =
            session->snapshot(selectionFor(QPoint(40, 40), QPoint(240, 180)));
        QCOMPARE(set.size(), 2);
        QCOMPARE(dynamic_cast<Editor::Tools::StepTool*>(set.entries().at(0).prototype.get())->number(), 1);
        QCOMPARE(dynamic_cast<Editor::Tools::StepTool*>(set.entries().at(1).prototype.get())->number(), 2);
        QCOMPARE(set.entries().at(1).pos, QPointF(100, 40));
    }

    void highlightStrokeIsInTheCopy()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_H);
        QCOMPARE(session->activeTool(), QStringLiteral("highlight"));
        stroke(QPoint(80, 100), QPoint(180, 100));

        QSignalSpy copy(m_sel.data(), &AreaSelector::copyRequested);
        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(copy.count(), 1);

        std::unique_ptr<Editor::Tools::ITool> tmpl(
            Editor::ToolRegistry::find("highlight")->makeTemplate());
        const QColor c = dynamic_cast<Editor::Tools::HighlightTool*>(tmpl.get())->color();
        const QColor bg(Qt::darkGray);
        const qreal a = Editor::Tools::HighlightTool::HIGHLIGHT_OPACITY;
        const auto mix = [a](int under, int over) { return qRound(under + a * (over - under)); };
        const QColor blended(mix(bg.red(), c.red()), mix(bg.green(), c.green()), mix(bg.blue(), c.blue()));

        // Crop-local: the stroke runs along y 60 from x 40 to 140.
        const QImage out = session->flattenedCrop(copy.at(0).at(0).toRect()).toImage();
        const QColor mid = out.pixelColor(90, 60);
        QVERIFY(qAbs(mid.red() - blended.red()) <= 3);
        QVERIFY(qAbs(mid.green() - blended.green()) <= 3);
        QVERIFY(qAbs(mid.blue() - blended.blue()) <= 3);
        QCOMPARE(out.pixelColor(90, 120), bg);
    }

    void pressOutsideKeepsTheSelectionWithAToolArmed()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        QSignalSpy state(m_sel.data(), &AreaSelector::liveStateChanged);

        stroke(QPoint(400, 400), QPoint(500, 480));
        for (const auto &args : state)
            QCOMPARE(args.at(0).toRect(), selectionFor(QPoint(40, 40), QPoint(240, 180)));
        QVERIFY(!session->hasItems());
    }

    void undoAndRedoChords()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        stroke(QPoint(80, 80), QPoint(160, 140));
        QVERIFY(session->hasItems());

        chord(QKeySequence::Undo);
        QVERIFY(!session->hasItems());
        chord(QKeySequence::Redo);
        QVERIFY(session->hasItems());
        QVERIFY(m_sel->isVisible());
    }

    void sameLetterDisarmsTheTool()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        QVERIFY(session->activeTool().isEmpty());

        verifyInteriorDragMoves(selectionFor(QPoint(40, 40), QPoint(240, 180)));
        QVERIFY(!session->hasItems());
    }

    void toolLettersAreInertWithoutASession()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        verifyInteriorDragMoves(selectionFor(QPoint(40, 40), QPoint(240, 180)));
    }

    void escapeDisarmsBeforeClearing()
    {
        const auto session = attachSession();
        drag(QPoint(40, 40), QPoint(240, 180));
        QTest::keyClick(m_sel.data(), Qt::Key_R);
        stroke(QPoint(80, 80), QPoint(160, 140));
        QSignalSpy state(m_sel.data(), &AreaSelector::liveStateChanged);

        QTest::keyClick(m_sel.data(), Qt::Key_Escape);
        QVERIFY(session->activeTool().isEmpty());
        QVERIFY(session->hasItems());
        for (const auto &args : state)
            QCOMPARE(args.at(1).toInt(), kAdjusting);
        QVERIFY(m_sel->isVisible());

        QTest::keyClick(m_sel.data(), Qt::Key_Escape);
        QVERIFY(!state.isEmpty());
        QCOMPARE(state.last().at(1).toInt(), kIdle);
        QVERIFY(!session->hasItems());
        QVERIFY(m_sel->isVisible());
    }
};

QTEST_MAIN(tst_AreaSelector)

#include "tst_areaselector.moc"
