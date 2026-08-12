#include <QtTest>
#include <QGraphicsRectItem>
#include <QSignalSpy>

#include "editor/annotations/Layer.h"

using namespace Editor;

// A Layer is a leaf (wraps an item) or a Group (has children); visibility cascades
// from a group to its children.
class tst_Layer : public QObject
{
    Q_OBJECT

private slots:
    void leaf_visibility_togglesWrappedItem()
    {
        Layer leaf("L", Layer::Rectangle);
        auto *item = new QGraphicsRectItem();
        leaf.setItem(item);
        QVERIFY(leaf.isVisible());
        QVERIFY(item->isVisible());

        leaf.setVisible(false);
        QVERIFY(!leaf.isVisible());
        QVERIFY(!item->isVisible());

        delete item;   // Layer doesn't own its item
    }

    void group_isGroup_andChildManagement()
    {
        Layer group("G", Layer::Group);
        QVERIFY(group.isGroup());
        QCOMPARE(group.item(), nullptr);

        Layer a("a", Layer::Arrow);
        Layer b("b", Layer::Arrow);
        group.addChild(&a);
        group.addChild(&b);
        QCOMPARE(group.children().size(), 2);

        group.addChild(&a);   // no duplicates
        QCOMPARE(group.children().size(), 2);

        group.removeChild(&a);
        QCOMPARE(group.children().size(), 1);
        QCOMPARE(group.children().first(), &b);

        QVERIFY(!a.isGroup());   // a leaf is not a group
    }

    void isEditable_onlyForDrawnLeaves()
    {
        for (Layer::LayerType type : {Layer::Arrow, Layer::Text, Layer::Rectangle, Layer::Ellipse,
                                      Layer::Freehand, Layer::Highlight, Layer::Blur, Layer::Step})
            QVERIFY(Layer("drawn", type).isEditable());
        for (Layer::LayerType type : {Layer::Background, Layer::Backdrop, Layer::Group})
            QVERIFY(!Layer("fixed", type).isEditable());
    }

    void group_visibility_cascadesToChildren()
    {
        Layer group("G", Layer::Group);
        Layer a("a", Layer::Arrow);
        Layer b("b", Layer::Arrow);
        auto *ia = new QGraphicsRectItem();
        auto *ib = new QGraphicsRectItem();
        a.setItem(ia);
        b.setItem(ib);
        group.addChild(&a);
        group.addChild(&b);

        group.setVisible(false);
        QVERIFY(!a.isVisible());
        QVERIFY(!b.isVisible());
        QVERIFY(!ia->isVisible());
        QVERIFY(!ib->isVisible());

        group.setVisible(true);
        QVERIFY(a.isVisible());
        QVERIFY(b.isVisible());
        QVERIFY(ia->isVisible());

        delete ia;
        delete ib;
    }

    void visibilityChanged_signalEmittedOnceOnChange()
    {
        Layer leaf("L", Layer::Rectangle);
        QSignalSpy spy(&leaf, &Layer::visibilityChanged);

        leaf.setVisible(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);

        leaf.setVisible(false);   // no actual change -> no signal
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(tst_Layer)
#include "tst_layer.moc"
