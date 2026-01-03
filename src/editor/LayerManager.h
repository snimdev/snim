#ifndef IMAGEEDITOR_LAYERMANAGER_H
#define IMAGEEDITOR_LAYERMANAGER_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>

namespace ImageEditor {

class Layer;

class LayerManager : public QWidget
{
    Q_OBJECT

public:
    explicit LayerManager(QWidget *parent = nullptr);

    void updateLayerList();
    void addLayer(Layer *layer);
    void removeLayer(Layer *layer);
    void removeSelectedLayer();
    Layer* selectedLayer() const;
    QList<Layer*> layers() const { return m_layers; }
    void selectLayer(Layer *layer);

signals:
    void layerSelected(Layer *layer);
    void layerVisibilityChanged(Layer *layer, bool visible);
    void deleteLayerRequested(Layer *layer);
    void layerAdded(Layer *layer);

private slots:
    void onItemSelectionChanged();
    void onDeleteButtonClicked();

private:
    QListWidget *m_layerList;
    QPushButton *m_deleteButton;
    QList<Layer*> m_layers;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_LAYERMANAGER_H
