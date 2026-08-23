#ifndef IMAGEEDITOR_LAYERPROPERTIES_H
#define IMAGEEDITOR_LAYERPROPERTIES_H

#include <QWidget>
#include <QMap>
#include "tools/ITool.h"

class QLayout;
class QPushButton;
class QStackedWidget;

namespace Editor {

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
    // A control changed a tool property. Editor turns this into an undoable
    // command (kept undo-agnostic here so the widget stays reusable).
    void propertyChangeRequested(Tools::ITool *tool, const QString &propertyId, const QVariant &value);

private:
    // A scrolling page of tool's controls, made current. A layer's page has no title.
    QWidget *addPage(Tools::ITool *tool, Layer *layer, const QString &title = {});
    QLayout* createPropertyControl(Tools::ITool *tool, const Tools::ToolProperty &prop, QWidget *parent);
    QPushButton *createSwatch(Tools::ITool *tool, const QString &propertyId, const QString &tip,
                              const QColor &initial, QWidget *parent);

    QStackedWidget *m_stackedWidget;
    QMap<Layer*, QWidget*> m_layerWidgetMap;
    QWidget* m_emptyWidget;
    QWidget* m_toolWidget;  // Widget for standalone tool (not from layer)
};

} // namespace Editor

#endif // IMAGEEDITOR_LAYERPROPERTIES_H
