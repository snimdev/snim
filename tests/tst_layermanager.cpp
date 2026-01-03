#include <QtTest>
#include <QSignalSpy>

#include "editor/LayerManager.h"
#include "editor/Layer.h"

using namespace ImageEditor;

// LayerManager is a QWidget (QTreeWidget); construct it offscreen and drive its
// public API. (Multi-select group-button enablement uses the private tree and is
// covered by the command-level Group test instead.)
class tst_LayerManager : public QObject
{
    Q_OBJECT

    LayerManager *mgr = nullptr;
    QObject *owner = nullptr;   // owns the Layers

    Layer* leaf(const QString &name)
    {
        return new Layer(name, Layer::Rectangle, owner);   // no item needed for these tests
    }

private slots:
    void init()    { mgr = new LayerManager(); owner = new QObject(); }
    void cleanup() { delete mgr; mgr = nullptr; delete owner; owner = nullptr; }

    void addLayer_appendsAndEmits()
    {
        QSignalSpy added(mgr, &LayerManager::layerAdded);
        Layer *a = leaf("A");
        mgr->addLayer(a);
        QCOMPARE(mgr->layers().size(), 1);
        QVERIFY(mgr->layers().contains(a));
        QCOMPARE(added.count(), 1);
    }

    void layers_keepInsertionOrder()
    {
        Layer *a = leaf("A");
        Layer *b = leaf("B");
        mgr->addLayer(a);
        mgr->addLayer(b);
        QCOMPARE(mgr->layers().at(0), a);
        QCOMPARE(mgr->layers().at(1), b);
    }

    void selectLayer_roundTrip_emitsSelected()
    {
        Layer *a = leaf("A");
        mgr->addLayer(a);
        QSignalSpy sel(mgr, &LayerManager::layerSelected);
        mgr->selectLayer(a);
        QCOMPARE(mgr->selectedLayer(), a);
        QVERIFY(sel.count() >= 1);
    }

    void removeLayer_topLevel()
    {
        Layer *a = leaf("A");
        mgr->addLayer(a);
        mgr->removeLayer(a);
        QVERIFY(mgr->layers().isEmpty());
    }

    void removeLayer_detachesChildFromGroup()
    {
        auto *group = new Layer("G", Layer::Group, owner);
        Layer *child = leaf("C");
        group->addChild(child);
        mgr->addLayer(group);
        QCOMPARE(group->children().size(), 1);

        mgr->removeLayer(child);                 // child is not a top-level node
        QCOMPARE(group->children().size(), 0);   // it was detached from its group
        QVERIFY(mgr->layers().contains(group));  // the group itself remains
    }
};

QTEST_MAIN(tst_LayerManager)
#include "tst_layermanager.moc"
