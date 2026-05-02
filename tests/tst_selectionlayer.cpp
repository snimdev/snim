#include <QtTest>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPixmap>
#include <QPointer>
#include <QScreen>
#include <QSharedPointer>
#include <QSignalSpy>

#include "screen/AreaSelector.h"
#include "screen/SelectionLayer.h"

using namespace Screen;

namespace {

// A layer with no annotation code: it records what reaches it and fakes the states
// (typing, drawing, armed) that make it keep Esc for itself.
class FakeLayer : public SelectionLayer
{
public:
    struct MouseCall {
        MouseAction action;
        QPoint virt;
        SelectionContext::Hit hit;
    };

    bool typing = false;
    bool drawing = false;
    bool armed = false;
    QList<int> keys;
    QList<MouseCall> mouse;
    QStringList activated;
    int clears = 0;

    void setSelection(const QRect &) override {}
    void clear() override { ++clears; }
    void render(QPainter *, const QRectF &, const QRect &) override {}
    bool isArmed() const override { return armed; }
    QCursor cursor() const override { return QCursor(Qt::PointingHandCursor); }
    bool capturesKeyboard() const override { return typing; }
    void forwardInputMethod(QInputMethodEvent *) override {}
    QVariant inputMethodQuery(Qt::InputMethodQuery) const override { return {}; }
    std::optional<QString> hint() const override { return std::nullopt; }

    QVector<ToolbarSlot> toolbarSlots() const override
    {
        ToolbarSlot one;
        one.id = QStringLiteral("one");
        one.glyph = QStringLiteral("1");
        ToolbarSlot two;
        two.id = QStringLiteral("two");
        two.glyph = QStringLiteral("2");
        two.group = 1;
        return {one, two};
    }

    void activateSlot(const QString &id) override { activated.append(id); }

    bool handleKey(QKeyEvent *event, SelectionContext &context) override
    {
        keys.append(event->key());
        if (event->key() == Qt::Key_Escape) {
            if (typing) { typing = false; return true; }
            if (drawing) { drawing = false; return true; }
            if (armed && context.actionsAvailable()) { armed = false; return true; }
            return false;
        }
        return typing || (armed && event->key() == Qt::Key_Q);
    }

    bool handleMouse(MouseAction action, const QPoint &virt, Qt::MouseButton button,
                     SelectionContext &context) override
    {
        const SelectionContext::Hit hit = context.hitTest(virt);
        mouse.append({action, virt, hit});
        const bool live = armed && context.adjusting();
        switch (action) {
            case MouseAction::Press:
                if (button != Qt::LeftButton || !live || hit == SelectionContext::Hit::Handle)
                    return false;
                drawing = hit == SelectionContext::Hit::Inside;
                return true;
            case MouseAction::Move:
                return drawing;
            case MouseAction::Release:
                if (!drawing) return false;
                drawing = false;
                return true;
            case MouseAction::DoubleClick:
                return button == Qt::LeftButton && live;
        }
        return false;
    }
};

} // namespace

// The selector against a fake layer: input, toolbar and Esc reach any SelectionLayer.
class tst_SelectionLayer : public QObject
{
    Q_OBJECT

private:
    static constexpr int kIdle = 0;
    static constexpr int kAdjusting = 2;

    QPointer<AreaSelector> m_sel;
    QSharedPointer<FakeLayer> m_layer;
    QRect m_screen;

    QRect selectionFor(const QPoint &from, const QPoint &to) const
    {
        return QRect(from, to).normalized().translated(m_screen.topLeft());
    }

    void drag(const QPoint &from, const QPoint &to)
    {
        QTest::mousePress(m_sel.data(), Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(m_sel.data(), to);
        QTest::mouseRelease(m_sel.data(), Qt::LeftButton, Qt::NoModifier, to);
    }

    // Centers of the toolbar buttons, left to right, found by hovering for the hand cursor.
    QVector<QPoint> toolbarButtons(int below)
    {
        const auto spans = [this](int y) {
            QVector<QPoint> centers;
            int start = -1;
            for (int x = 0; x <= m_sel->width(); x += 2) {
                QTest::mouseMove(m_sel.data(), QPoint(x, y));
                const bool hand = m_sel->cursor().shape() == Qt::PointingHandCursor;
                if (hand && start < 0) {
                    start = x;
                } else if (!hand && start >= 0) {
                    centers.append(QPoint((start + x) / 2, y));
                    start = -1;
                }
            }
            return centers;
        };
        for (int y = below; y < m_sel->height(); y += 4)
            if (!spans(y).isEmpty())
                return spans(y + 12);
        return {};
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
        m_layer = QSharedPointer<FakeLayer>::create();
        sel->setLayer(m_layer);
        sel->show();
        QVERIFY(QTest::qWaitForWindowExposed(sel));
    }

    void cleanup()
    {
        delete m_sel.data();
        m_layer.reset();
    }

    void armedLayerTakesInputInsideTheSelection()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        m_layer->armed = true;
        m_layer->mouse.clear();
        QSignalSpy state(m_sel.data(), &AreaSelector::liveStateChanged);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::mousePress(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
        QTest::mouseMove(m_sel.data(), QPoint(140, 120));
        QTest::mouseRelease(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(140, 120));

        QCOMPARE(m_layer->mouse.size(), 3);
        QCOMPARE(m_layer->mouse.at(0).action, SelectionLayer::MouseAction::Press);
        QCOMPARE(m_layer->mouse.at(0).virt, QPoint(100, 100) + m_screen.topLeft());
        QCOMPARE(m_layer->mouse.at(0).hit, SelectionContext::Hit::Inside);
        QCOMPARE(m_layer->mouse.at(1).action, SelectionLayer::MouseAction::Move);
        QCOMPARE(m_layer->mouse.at(2).action, SelectionLayer::MouseAction::Release);
        QVERIFY(state.isEmpty());   // the selection did not move
        QCOMPARE(area.count(), 0);

        QTest::keyClick(m_sel.data(), Qt::Key_Q);
        QCOMPARE(m_layer->keys, QList<int>{Qt::Key_Q});

        // Handles still resize with the layer armed.
        QTest::mousePress(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(240, 180));
        QCOMPARE(m_layer->mouse.last().hit, SelectionContext::Hit::Handle);
        QTest::mouseMove(m_sel.data(), QPoint(260, 200));
        QTest::mouseRelease(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(260, 200));
        QVERIFY(!state.isEmpty());
        QCOMPARE(state.last().at(0).toRect(), selectionFor(QPoint(40, 40), QPoint(260, 200)));
        QCOMPARE(area.count(), 0);
    }

    void typingLayerTakesEveryKey()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        m_layer->typing = true;
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_Return);
        QTest::keyClick(m_sel.data(), Qt::Key_C, Qt::ControlModifier);

        // keyClick presses Ctrl itself first, which the layer takes too.
        QVERIFY(m_layer->keys.contains(Qt::Key_Return));
        QVERIFY(m_layer->keys.contains(Qt::Key_C));
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel->isVisible());
    }

    void unarmedLayerLetsAClickCommit()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));

        QCOMPARE(area.count(), 1);
        QCOMPARE(area.at(0).at(0).toRect(), selectionFor(QPoint(40, 40), QPoint(240, 180)));
    }

    void layerSlotsLeadTheToolbar()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        const QVector<QPoint> buttons = toolbarButtons(180);
        QCOMPARE(buttons.size(), 2 + 4);   // the layer's, then Edit, Copy, Save, Cancel

        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, buttons.at(0));
        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, buttons.at(1));
        QCOMPARE(m_layer->activated, QStringList({"one", "two"}));
        QVERIFY(m_sel->isVisible());

        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);
        QTest::mouseClick(m_sel.data(), Qt::LeftButton, Qt::NoModifier, buttons.at(2));
        QCOMPARE(area.count(), 1);
        QCOMPARE(area.at(0).at(0).toRect(), selectionFor(QPoint(40, 40), QPoint(240, 180)));
    }

    void escapeEndsLayerStatesBeforeTheSelection()
    {
        drag(QPoint(40, 40), QPoint(240, 180));
        m_layer->typing = true;
        m_layer->drawing = true;
        m_layer->armed = true;
        m_layer->clears = 0;   // starting the selection cleared it once
        QSignalSpy state(m_sel.data(), &AreaSelector::liveStateChanged);
        QSignalSpy area(m_sel.data(), &AreaSelector::areaSelected);

        QTest::keyClick(m_sel.data(), Qt::Key_Escape);
        QVERIFY(!m_layer->typing);
        QVERIFY(m_layer->drawing);
        QTest::keyClick(m_sel.data(), Qt::Key_Escape);
        QVERIFY(!m_layer->drawing);
        QVERIFY(m_layer->armed);
        QTest::keyClick(m_sel.data(), Qt::Key_Escape);
        QVERIFY(!m_layer->armed);
        QVERIFY(state.isEmpty());
        QCOMPARE(m_layer->clears, 0);
        QCOMPARE(area.count(), 0);

        QTest::keyClick(m_sel.data(), Qt::Key_Escape);
        QVERIFY(!state.isEmpty());
        QCOMPARE(state.last().at(1).toInt(), kIdle);
        QVERIFY(state.last().at(0).toRect().isEmpty());
        QCOMPARE(m_layer->clears, 1);
        QCOMPARE(area.count(), 0);
        QVERIFY(m_sel->isVisible());

        QTest::keyClick(m_sel.data(), Qt::Key_Escape);
        QCOMPARE(area.count(), 1);
        QVERIFY(area.at(0).at(0).toRect().isEmpty());
        QVERIFY(!m_sel->isVisible());
        QCOMPARE(m_layer->keys.count(Qt::Key_Escape), 5);
    }
};

QTEST_MAIN(tst_SelectionLayer)

#include "tst_selectionlayer.moc"
