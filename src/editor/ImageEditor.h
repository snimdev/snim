#ifndef IMAGEEDITOR_IMAGEEDITOR_H
#define IMAGEEDITOR_IMAGEEDITOR_H

#include <QMainWindow>
#include <QPixmap>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QToolBar>
#include <QAction>
#include <QSplitter>
#include <QGraphicsLineItem>
#include <QMouseEvent>

namespace ImageEditor {

class DrawingGraphicsView;
class LayerManager;
class LayerProperties;
class Layer;

class ImageEditor : public QMainWindow
{
    Q_OBJECT

public:
    explicit ImageEditor(const QPixmap &screenshot, QWidget *parent = nullptr);
    virtual ~ImageEditor() = default;

public slots:
    void saveAs();
    void copyToClipboard();
    void selectPointerTool();
    void selectArrowTool();
    void selectTextTool();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onLayerVisibilityChanged(Layer *layer, bool visible);
    void onDeleteLayerRequested(Layer *layer);
    void onLayerSelected(Layer *layer);
    void onItemClicked(QGraphicsItem *item);

private:
    void setupUI();
    void setupToolbar();
    QPixmap renderScene();
    void addTextLayer(const QPoint &position, const QString &text);
    void addArrowLayer(const QPoint &start, const QPoint &end);
    Layer* createBackgroundLayer();
    void selectLayerByItem(QGraphicsItem *item);
    bool isWithinImageBounds(const QPoint &point) const;
    QPoint clampToImageBounds(const QPoint &point) const;

    enum ToolType {
        None,
        Arrow,
        Text
    };

    // UI Components
    DrawingGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    QToolBar *m_toolbar;
    QAction *m_saveAsAction;
    QAction *m_copyAction;
    QAction *m_pointerAction;
    QAction *m_arrowAction;
    QAction *m_textAction;
    QSplitter *m_splitter;
    QSplitter *m_rightSplitter;
    LayerManager *m_layerManager;
    LayerProperties *m_layerProperties;

    // State
    QPixmap m_originalScreenshot;
    ToolType m_currentTool;
    bool m_drawing;
    QGraphicsItem *m_currentArrow;
    Layer *m_backgroundLayer;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_IMAGEEDITOR_H
