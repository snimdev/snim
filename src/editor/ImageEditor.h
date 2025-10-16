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
#include <QPen>

namespace ImageEditor {

class DrawingGraphicsView;
class LayerManager;
class LayerProperties;
class Layer;

namespace Tools {
    class ITool;
}

namespace Interactions {
    class PointerToolInteraction;
    class ArrowDrawingInteraction;
    class TextDrawingInteraction;
    class RectangleDrawingInteraction;
    class EllipseDrawingInteraction;
    class FreehandDrawingInteraction;
    class HighlightDrawingInteraction;
    class BlurDrawingInteraction;
}

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
    void selectRectangleTool();
    void selectEllipseTool();
    void selectFreehandTool();
    void selectHighlightTool();
    void selectBlurTool();

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
    void setupStrategies();
    void setupToolTemplates();
    QPixmap renderScene();
    void addTextLayer(const QPoint &position, const QString &text);
    void addArrowLayer(const QPoint &start, const QPoint &end);
    void addRectangleLayer(const QRect &rect);
    void addEllipseLayer(const QRect &rect);
    void addFreehandLayer(const QList<QPointF> &points);
    void addHighlightLayer(const QList<QPointF> &points, const QColor &color, qreal width);
    void addBlurLayer(const QList<QPointF> &points);
    Layer* createBackgroundLayer();
    void selectLayerByItem(QGraphicsItem *item);
    bool isWithinImageBounds(const QPoint &point) const;
    QPoint clampToImageBounds(const QPoint &point) const;
    QPen getCurrentFreehandPen() const;
    QColor getCurrentHighlightColor() const;
    qreal getCurrentHighlightWidth() const;
    qreal getCurrentBlurRadius() const;
    qreal getCurrentBlurBrushWidth() const;
    QIcon createThemedIcon(const QString &iconPath);

    enum ToolType {
        None,
        Arrow,
        Text,
        Rectangle,
        Ellipse,
        Freehand,
        Highlight,
        Blur
    };

    // UI Components
    DrawingGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;

    // - Toolbar
    QToolBar *m_toolbar;

    // - Toolbar :: Actions
    QAction *m_saveAsAction;
    QAction *m_copyAction;

    // - Toolbar :: Tools
    QAction *m_pointerAction;
    QAction *m_arrowAction;
    QAction *m_textAction;
    QAction *m_rectangleAction;
    QAction *m_ellipseAction;
    QAction *m_freehandAction;
    QAction *m_highlightAction;
    QAction *m_blurAction;

    // Sidebar
    QSplitter *m_splitter;
    QSplitter *m_rightSplitter;

    // Layer Management
    LayerManager *m_layerManager;
    LayerProperties *m_layerProperties;

    // State
    QPixmap m_originalScreenshot;
    ToolType m_currentTool;
    bool m_drawing;
    QGraphicsItem *m_currentArrow;
    Layer *m_backgroundLayer;
    Layer *m_lastFreehandLayer; // Track last freehand layer to remember settings
    Layer *m_lastHighlightLayer; // Track last highlight layer to remember color
    Layer *m_lastBlurLayer; // Track last blur layer to remember settings

    // Drawing Interactions
    Interactions::PointerToolInteraction *m_pointerStrategy;
    Interactions::ArrowDrawingInteraction *m_arrowStrategy;
    Interactions::TextDrawingInteraction *m_textStrategy;
    Interactions::RectangleDrawingInteraction *m_rectangleStrategy;
    Interactions::EllipseDrawingInteraction *m_ellipseStrategy;
    Interactions::FreehandDrawingInteraction *m_freehandStrategy;
    Interactions::HighlightDrawingInteraction *m_highlightStrategy;
    Interactions::BlurDrawingInteraction *m_blurStrategy;

    // Template tool instances for property preview (not added to scene)
    Tools::ITool *m_textTemplate;
    Tools::ITool *m_arrowTemplate;
    Tools::ITool *m_rectangleTemplate;
    Tools::ITool *m_ellipseTemplate;
    Tools::ITool *m_freehandTemplate;
    Tools::ITool *m_highlightTemplate;
    Tools::ITool *m_blurTemplate;
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_IMAGEEDITOR_H
