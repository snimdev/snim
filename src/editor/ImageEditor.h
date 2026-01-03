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
#include <QHash>
#include <QString>

class QActionGroup;
class QUndoStack;

namespace ImageEditor {

class DrawingGraphicsView;
class LayerManager;
class LayerProperties;
class Layer;

namespace Tools {
    class ITool;
    class BackdropItem;
    class TextTool;
}

namespace Interactions {
    class IDrawingInteraction;
}

class ImageEditor : public QMainWindow
{
    Q_OBJECT

public:
    explicit ImageEditor(const QPixmap &screenshot, QWidget *parent = nullptr);
    ~ImageEditor() override;

public slots:
    void saveAs();
    void copyToClipboard();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;   // editor keyboard shortcuts
    bool eventFilter(QObject *obj, QEvent *event) override;

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
    // Tool registry driven: select a tool by id, and turn a freshly-drawn item
    // into a layer (the single commit point all drawing tools funnel through).
    void activateTool(const QString &toolId);
    void commitDrawnItem(QGraphicsItem *item, const QString &toolId);
    // Inline text: a freshly-placed text box is edited live; on focus-out it is
    // either committed (non-empty) or discarded (empty). See setupStrategies.
    void finalizePendingText(Tools::TextTool *item);
    // Keyboard-shortcut helpers (all document mutations go through QUndoCommands).
    void duplicateSelectedLayer();                 // ⌘/Ctrl+D: clone() + AddLayerCommand
    void nudgeSelectedLayer(qreal dx, qreal dy);   // arrows: MoveLayerCommand
    [[nodiscard]] bool isEditingText() const;      // a text item currently in inline-edit mode?
    Layer* createBackgroundLayer();
    void setBackdropEnabled(bool on);
    void onBackgroundButtonClicked();
    void showBackdropPopover();
    void rebuildPresetGrid();
    void saveCurrentBackdropAsPreset();
    void applyBackdropGeometry(bool refit);
    void applyRoundedScreenshot();
    void applyShadow();
    void onBackdropChanged(const QString &propertyId);
    void selectLayerByItem(QGraphicsItem *item);
    bool isWithinImageBounds(const QPoint &point) const;
    QPoint clampToImageBounds(const QPoint &point) const;
    QIcon createThemedIcon(const QString &iconPath);

    // UI Components
    DrawingGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;

    // - Toolbar
    QToolBar *m_toolbar;

    // - Toolbar :: Actions
    QAction *m_saveAsAction;
    QAction *m_copyAction;

    // - Toolbar :: View controls
    QAction *m_fitAction;
    QAction *m_actualSizeAction;
    QAction *m_panelsAction;       // toggle the Layers/Properties side panel
    QAction *m_backgroundAction;   // toggle the CleanShot-style beautify backdrop

    // - Toolbar :: Tools. Registry-driven: one exclusive action group plus the
    //   id-keyed maps below (actions, strategies, templates), all keyed by tool id.
    QActionGroup *m_toolGroup = nullptr;
    QHash<QString, QAction*> m_actions;
    QHash<QString, Interactions::IDrawingInteraction*> m_strategies;
    QHash<QString, Tools::ITool*> m_templates;
    QHash<QString, int> m_counters;   // per-tool layer-name counter

    // Undo/redo. Add/delete/property/visibility edits are pushed as
    // QUndoCommands; the stack owns them and drives the toolbar Undo/Redo actions.
    QUndoStack *m_undoStack = nullptr;

    // Sidebar
    QSplitter *m_splitter;
    QSplitter *m_rightSplitter;

    // Layer Management
    LayerManager *m_layerManager;
    LayerProperties *m_layerProperties;

    // State
    QPixmap m_originalScreenshot;
    bool m_firstShown = false;   // fit/center the view only on the first show
    bool m_dirty = false;        // unsaved changes (layers added/edited, backdrop, ...)
    Layer *m_backgroundLayer;
    Tools::BackdropItem *m_backdropItem;  // beautify backdrop (null when off)
    Layer *m_backdropLayer;
    QWidget *m_backdropPopover = nullptr;   // floating quick-actions popover
    QWidget *m_presetGrid = nullptr;        // preset-tiles container inside the popover
    Layer *m_selectedLayer = nullptr;       // current side-panel selection
    Tools::TextTool *m_pendingTextItem = nullptr;  // text box being created/edited inline (uncommitted)
};

} // namespace ImageEditor

#endif // IMAGEEDITOR_IMAGEEDITOR_H
