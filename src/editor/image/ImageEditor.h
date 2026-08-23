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
#include <QDateTime>
#include <QHash>
#include <QString>
#include <memory>

class QActionGroup;
class QUndoStack;

namespace Editor {

class AnnotationBuilder;
class AnnotationSet;
class DrawingGraphicsView;
class LayerManager;
class LayerProperties;
class Layer;

} // namespace Editor

namespace Editor::Image {

class BackdropItem;

class ImageEditor : public QMainWindow
{
    Q_OBJECT

public:
    explicit ImageEditor(const QPixmap &screenshot, QWidget *parent = nullptr);
    ~ImageEditor() override;

    // Adds the overlay's annotations as editable layers, outside the undo history.
    void importAnnotations(const AnnotationSet &set);

public slots:
    void saveAs();
    void copyToClipboard();
    void doUpload(const QString &profileId);   // render to a temp PNG -> app's uploader (empty id = default)

signals:
    // The app owns the upload (it outlives this window); deleteWhenDone=true means the
    // temp PNG is the app's to delete once the upload finishes. profileId empty = default.
    void uploadRequested(const QString &localPath, const QString &suggestedName,
                         bool deleteWhenDone, const QString &profileId);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;   // editor keyboard shortcuts
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onLayerVisibilityChanged(Layer *layer, bool visible);
    void onDeleteLayerRequested(Layer *layer);
    void onLayerSelected(Layer *layer);

private:
    class LayerSink;

    void setupUI();
    void setupToolbar();
    void setupStrategies();
    QPixmap renderScene();
    // Tool registry driven: select a tool by id, and turn a freshly-drawn item
    // into a layer (the single commit point all drawing tools funnel through).
    void activateTool(const QString &toolId);
    void commitDrawnItem(QGraphicsItem *item, const QString &toolId);
    // Named layer for item, or null (item deleted) for an unknown tool.
    Layer *makeLayer(QGraphicsItem *item, const QString &toolId);
    // Keyboard-shortcut helpers (all document mutations go through QUndoCommands).
    void duplicateSelectedLayer();                 // ⌘/Ctrl+D: clone() + AddLayerCommand
    void nudgeSelectedLayer(qreal dx, qreal dy);   // arrows: MoveLayerCommand
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

    // UI Components
    DrawingGraphicsView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;

    // - Toolbar
    QToolBar *m_toolbar = nullptr;

    // - Toolbar :: Actions
    QAction *m_saveAsAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_uploadAction = nullptr;

    // - Toolbar :: View controls
    QAction *m_panelsAction = nullptr;       // toggle the Layers/Properties side panel
    QAction *m_backgroundAction = nullptr;   // toggle the CleanShot-style beautify backdrop

    // - Toolbar :: Tools. Registry-driven: one exclusive action group plus the
    //   id-keyed actions; interactions and templates live in the builder.
    QActionGroup *m_toolGroup = nullptr;
    QHash<QString, QAction*> m_actions;
    std::unique_ptr<LayerSink> m_layerSink;   // builder's commit target
    AnnotationBuilder *m_builder = nullptr;
    QHash<QString, int> m_counters;   // per-tool layer-name counter

    // Undo/redo. Add/delete/property/visibility edits are pushed as
    // QUndoCommands; the stack owns them and drives the toolbar Undo/Redo actions.
    QUndoStack *m_undoStack = nullptr;

    // Sidebar
    QSplitter *m_splitter = nullptr;
    QSplitter *m_rightSplitter = nullptr;

    // Layer Management
    LayerManager *m_layerManager = nullptr;
    LayerProperties *m_layerProperties = nullptr;

    // State
    QPixmap m_originalScreenshot;
    QDateTime m_capturedAt = QDateTime::currentDateTime();   // opens right after capture
    bool m_firstShown = false;   // fit/center the view only on the first show
    bool m_dirty = false;        // unsaved changes (layers added/edited, backdrop, ...)
    BackdropItem *m_backdropItem = nullptr;   // beautify backdrop (null when off)
    Layer *m_backdropLayer = nullptr;
    QWidget *m_backdropPopover = nullptr;   // floating quick-actions popover
    QWidget *m_presetGrid = nullptr;        // preset-tiles container inside the popover
    Layer *m_selectedLayer = nullptr;       // current side-panel selection
};

} // namespace Editor::Image

#endif // IMAGEEDITOR_IMAGEEDITOR_H
