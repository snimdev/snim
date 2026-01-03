#ifndef IMAGEEDITOR_LAYERPROPERTIES_H
#define IMAGEEDITOR_LAYERPROPERTIES_H

#include <QWidget>
#include <QMap>
#include <QPalette>
#include "tools/ITool.h"

class QStackedWidget;
class QLayout;

namespace ImageEditor {

class Layer;

class LayerProperties : public QWidget
{
    Q_OBJECT

public:
    explicit LayerProperties(QWidget *parent = nullptr);
    void setLayer(Layer *layer);
    void rebuildLayer(Layer *layer);   // force a fresh build (for dynamic property sets)
    void setTool(Tools::ITool *tool, const QString &toolName);
    void removeLayer(Layer *layer);

signals:
    void savePresetRequested();   // emitted by the backdrop panel's "Save as preset…" button

private:
    void buildPropertiesUI(Layer *layer);
    void buildPropertiesUIForTool(Tools::ITool *tool, const QString &title);
    QLayout* createPropertyControl(Tools::ITool *tool, const Tools::ToolProperty &prop, QWidget *parent);
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
