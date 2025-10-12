#ifndef IMAGEEDITOR_H
#define IMAGEEDITOR_H

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
    void addTextLayer(const QPoint &position, const QString &text = "");
    void addArrowLayer(const QPoint &start, const QPoint &end);
    Layer* createBackgroundLayer();
    bool isWithinImageBounds(const QPoint &point) const;
    QPoint clampToImageBounds(const QPoint &point) const;
    void selectLayerByItem(QGraphicsItem *item);

    enum Tool {
        None,
        Arrow,
        Text
    };

    QPixmap m_originalScreenshot;
    DrawingGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    QToolBar *m_toolbar;

    // Layer management
    LayerManager *m_layerManager;
    LayerProperties *m_layerProperties;
    QSplitter *m_splitter;
    QSplitter *m_rightSplitter;
    Layer *m_backgroundLayer;

    Tool m_currentTool;
    QPoint m_startPoint;
    QPoint m_endPoint;
    bool m_drawing;
    QGraphicsLineItem *m_currentArrow;

    QAction *m_saveAsAction;
    QAction *m_copyAction;
    QAction *m_pointerAction;
    QAction *m_arrowAction;
    QAction *m_textAction;
};

#endif // IMAGEEDITOR_H
