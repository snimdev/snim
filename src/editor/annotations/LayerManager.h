#ifndef IMAGEEDITOR_LAYERMANAGER_H
#define IMAGEEDITOR_LAYERMANAGER_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QHash>

namespace Editor {

class Layer;

class LayerManager : public QWidget
{
    Q_OBJECT

public:
    explicit LayerManager(QWidget *parent = nullptr);

    void updateLayerList();
    void addLayer(Layer *layer);        // append a top-level node (leaf or group)
    void removeLayer(Layer *layer);     // remove a top-level node
    void deleteCurrentLayer();          // delete the current leaf, then select a neighbour
    Layer* selectedLayer() const;       // the current (single) layer, for the panel
    QList<Layer*> selectedLayers() const;   // all selected (for grouping)
    QList<Layer*> layers() const { return m_layers; }   // top-level, in order
    void selectLayer(Layer *layer);

signals:
    void layerSelected(Layer *layer);
    void layerVisibilityChanged(Layer *layer, bool visible);
    void deleteLayerRequested(Layer *layer);
    void layerAdded(Layer *layer);
    // Eye-button toggle request; Editor turns this into an undoable command
    // (the command flips the layer, not the button directly).
    void visibilityToggleRequested(Layer *layer, bool visible);
    // Group the given top-level leaves, or dissolve a group.
    void groupRequested(const QList<Layer*> &layers);
    void ungroupRequested(Layer *group);

private slots:
    void onItemSelectionChanged();
    void onDeleteButtonClicked();
    void onGroupButtonClicked();
    void onUngroupButtonClicked();

private:
    void addRow(QTreeWidgetItem *parentItem, Layer *layer);   // build one tree row
    [[nodiscard]] Layer* layerForItem(QTreeWidgetItem *item) const;

    QTreeWidget *m_tree;
    QPushButton *m_deleteButton;
    QPushButton *m_groupButton;
    QPushButton *m_ungroupButton;
    QList<Layer*> m_layers;   // top-level nodes, in z-order (index 0 == bottom)
    QHash<QTreeWidgetItem*, Layer*> m_itemToLayer;
};

} // namespace Editor

#endif // IMAGEEDITOR_LAYERMANAGER_H
