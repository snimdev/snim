#ifndef IMAGEEDITOR_LAYERPROPERTIES_H
#define IMAGEEDITOR_LAYERPROPERTIES_H

#include <QWidget>
#include <QMap>
#include <QPalette>

class QStackedWidget;

namespace ImageEditor {

class Layer;

namespace Tools {
    class ITool;
}

class LayerProperties : public QWidget
{
    Q_OBJECT

public:
    explicit LayerProperties(QWidget *parent = nullptr);
    void setLayer(Layer *layer);
    void setTool(Tools::ITool *tool, const QString &toolName);
    void removeLayer(Layer *layer);

private:
    void buildPropertiesUI(Layer *layer);
    void buildPropertiesUIForTool(Tools::ITool *tool, const QString &title);
    void showPropertiesStyle();
    void hidePropertiesStyle();

    QStackedWidget *m_stackedWidget;
    QMap<Layer*, QWidget*> m_layerWidgetMap;
    QWidget* m_emptyWidget;
    QWidget* m_toolWidget;  // Widget for standalone tool (not from layer)
    QPalette m_originalPalette;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_LAYERPROPERTIES_H
