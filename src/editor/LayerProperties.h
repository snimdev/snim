#ifndef IMAGEEDITOR_LAYERPROPERTIES_H
#define IMAGEEDITOR_LAYERPROPERTIES_H

#include <QWidget>
#include <QMap>
#include <QPalette>

class QStackedWidget;

namespace ImageEditor {

class Layer;

class LayerProperties : public QWidget
{
    Q_OBJECT

public:
    explicit LayerProperties(QWidget *parent = nullptr);
    void setLayer(Layer *layer);
    void removeLayer(Layer *layer);

private:
    void buildPropertiesUI(Layer *layer);
    void showPropertiesStyle();
    void hidePropertiesStyle();

    QStackedWidget *m_stackedWidget;
    QMap<Layer*, QWidget*> m_layerWidgetMap;
    QWidget* m_emptyWidget;
    QPalette m_originalPalette;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_LAYERPROPERTIES_H
