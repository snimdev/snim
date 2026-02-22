#include "editor/image/ImageEditor.h"
#include "editor/annotations/DrawingGraphicsView.h"
#include "editor/EditorChrome.h"
#include "editor/annotations/LayerManager.h"
#include "editor/annotations/LayerProperties.h"
#include "editor/annotations/Layer.h"
#include "editor/annotations/ToolRegistry.h"
#include "editor/annotations/AnnotationBuilder.h"
#include "editor/annotations/AnnotationSet.h"
#include "editor/annotations/IAnnotationSink.h"
#include "editor/annotations/commands/EditorCommands.h"
#include "core/IconUtil.h"
#include "core/Perf.h"
#include "editor/annotations/tools/StepTool.h"
#include "editor/annotations/tools/TextTool.h"
#include "editor/annotations/StepNumbering.h"
#include "editor/image/BackdropItem.h"
#include "editor/image/BackdropPresets.h"
#include "editor/annotations/interactions/PointerToolInteraction.h"
#include <QGraphicsPixmapItem>
#include <QGraphicsLineItem>
#include <QInputDialog>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QApplication>
#include <QClipboard>
#include <QStyle>
#include <QKeySequence>
#include <QTimer>
#include <QStandardPaths>
#include <QDateTime>
#include <QElapsedTimer>
#include "upload/UploaderFactory.h"
#include "upload/UploadConfig.h"
#include "upload/UploadMenu.h"
#include <QLineEdit>
#include <QSettings>
#include <QDebug>
#include <QPalette>
#include <QFile>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>
#include <QToolButton>
#include <QShowEvent>
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QMenu>
#include <QIcon>
#include <QActionGroup>
#include <QUndoStack>
#include <QKeyEvent>
#include <cmath>

namespace Editor::Image {

// Drawn items become undoable layers.
class ImageEditor::LayerSink final : public IAnnotationSink
{
public:
    explicit LayerSink(ImageEditor *editor) : m_editor(editor) {}

    void commit(QGraphicsItem *item, const QString &toolId) override
    {
        m_editor->commitDrawnItem(item, toolId);
        // Refresh the panel's "Next number" hint; stamp-time derivation stays authoritative.
        if (toolId == QLatin1String("step"))
            if (Tools::ITool *tmpl = m_editor->m_builder->templateFor(toolId))
                if (const ToolSpec *spec = ToolRegistry::find(toolId))
                    m_editor->m_layerProperties->setTool(tmpl, spec->displayName);
    }

private:
    ImageEditor *m_editor;
};

ImageEditor::ImageEditor(const QPixmap &screenshot, QWidget *parent)
    : QMainWindow(parent)
    , m_view(nullptr)
    , m_scene(nullptr)
    , m_pixmapItem(nullptr)
    , m_toolbar(nullptr)
    , m_saveAsAction(nullptr)
    , m_copyAction(nullptr)
    , m_fitAction(nullptr)
    , m_actualSizeAction(nullptr)
    , m_panelsAction(nullptr)
    , m_backgroundAction(nullptr)
    , m_splitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_layerManager(nullptr)
    , m_layerProperties(nullptr)
    , m_originalScreenshot(screenshot)
    , m_backgroundLayer(nullptr)
    , m_backdropItem(nullptr)
    , m_backdropLayer(nullptr)
{
    qDebug() << "ImageEditor constructor called with screenshot size:" << screenshot.size();
    qDebug() << "Setting window title...";
    setWindowTitle("Image Editor");

    // Undo/redo stack, created before the toolbar so its Undo/Redo actions exist.
    m_undoStack = new QUndoStack(this);

    qDebug() << "About to call setupUI()...";
    setupUI();
    qDebug() << "setupUI() completed successfully";

    qDebug() << "About to call setupToolbar()...";
    setupToolbar();
    qDebug() << "setupToolbar() completed successfully";

    qDebug() << "About to call setupStrategies()...";
    setupStrategies();
    qDebug() << "setupStrategies() completed successfully";

    // Create background layer and add to layer manager
    qDebug() << "About to create background layer...";
    m_backgroundLayer = createBackgroundLayer();
    qDebug() << "Background layer created, about to add to layer manager...";

    if (m_layerManager) {
        m_layerManager->addLayer(m_backgroundLayer);
        qDebug() << "Background layer added to layer manager";
    } else {
        qDebug() << "ERROR: m_layerManager is null!";
    }

    // Connect layer manager signals
    qDebug() << "Connecting layer manager signals...";
    connect(m_layerManager, &LayerManager::layerVisibilityChanged,
            this, &ImageEditor::onLayerVisibilityChanged);
    connect(m_layerManager, &LayerManager::deleteLayerRequested,
            this, &ImageEditor::onDeleteLayerRequested);
    connect(m_layerManager, &LayerManager::layerSelected,
            this, &ImageEditor::onLayerSelected);
    // Any layer added after this point (annotations, backdrop) is an unsaved change.
    // The initial Background layer was already added above, so it isn't counted.
    connect(m_layerManager, &LayerManager::layerAdded,
            this, [this](Layer *) { m_dirty = true; });
    // Eye-button toggles become undoable commands.
    connect(m_layerManager, &LayerManager::visibilityToggleRequested, this,
            [this](Layer *layer, bool visible) {
                m_undoStack->push(new Commands::VisibilityChangeCommand(
                    layer, visible, QStringLiteral("Toggle %1").arg(layer->name())));
            });
    // Group / ungroup, undoably.
    connect(m_layerManager, &LayerManager::groupRequested, this,
            [this](const QList<Layer*> &members) {
                if (members.size() < 2)
                    return;
                auto *group = new Layer(QStringLiteral("Group %1").arg(++m_counters["group"]),
                                        Layer::Group, this);
                m_undoStack->push(new Commands::GroupLayersCommand(
                    m_layerManager, group, members, QStringLiteral("Group layers")));
            });
    connect(m_layerManager, &LayerManager::ungroupRequested, this,
            [this](Layer *group) {
                if (!group || !group->isGroup())
                    return;
                m_undoStack->push(new Commands::UngroupLayersCommand(
                    m_layerManager, group, QStringLiteral("Ungroup")));
            });
    // Keep the layer list (eye icons / names) in sync after any undo/redo. Deferred
    // so we never rebuild list-item widgets from inside an eye-button's own signal.
    connect(m_undoStack, &QUndoStack::indexChanged, this, [this](int) {
        QTimer::singleShot(0, this, [this] {
            if (m_layerManager) m_layerManager->updateLayerList();
        });
    });

    // Shared editor chrome (same sheet as the video editor).
    Editor::applyEditorStyleSheet(this);

    // Open at a consistent 70% of the current screen (the one under the cursor, where
    // the capture happened), independent of the capture's size, so a small grab doesn't
    // open a cramped window you have to resize. The capture is shown at 100% in a
    // scrollable view, centered on the same screen in showEvent().
    QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    const QSize avail = scr ? scr->availableSize() : QSize(1280, 800);
    QSize win(avail.width() * 7 / 10, avail.height() * 7 / 10);
    win = win.expandedTo(QSize(720, 520)).boundedTo(avail);
    resize(win);

    // Auto-apply a default backdrop preset, if the user set one (for consistent
    // project screenshots). Done last so all setup is ready.
    if (!BackdropPresets::defaultName().isEmpty()) {
        setBackdropEnabled(true);
        if (m_backdropItem) {
            m_backdropItem->applyConfig(BackdropPresets::defaultConfig());
            m_backdropItem->setActivePreset(BackdropPresets::defaultName());
        }
        if (m_backgroundAction)
            m_backgroundAction->setChecked(true);
    }
    // A freshly-opened capture (even with the auto default backdrop) counts as
    // clean, so closing it without edits doesn't prompt to save.
    m_dirty = false;

    qDebug() << "ImageEditor constructor completed successfully";
}

ImageEditor::~ImageEditor()
{
    // Clear the undo stack first, while the scene and layers are still alive, so
    // each command's destructor frees any off-scene items it owns cleanly (avoids
    // racing the QObject child-destruction order). See EditorCommands ownership notes.
    if (m_undoStack)
        m_undoStack->clear();
}

void ImageEditor::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_firstShown)
        return;
    m_firstShown = true;

    // Center on the current screen (under the cursor; the one the window was sized
    // for in the constructor), then fit the capture in the view.
    QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr)
        scr = screen() ? screen() : QGuiApplication::primaryScreen();
    if (scr) {
        QRect fg = frameGeometry();
        fg.moveCenter(scr->availableGeometry().center());
        move(fg.topLeft());
    }
    // Show at 100% (no scaling), centered; large captures are scrollable. Deferred
    // so centering uses the view's final laid-out size.
    if (m_view)
        QTimer::singleShot(0, this, [this] { m_view->zoomActual(); });
}

void ImageEditor::closeEvent(QCloseEvent *event)
{
    // Unsaved if anything was exported-since dirtied, or the undo stack diverged
    // from its last-saved (clean) state.
    const bool hasUnsaved = m_dirty || (m_undoStack && !m_undoStack->isClean());
    if (!hasUnsaved) {
        event->accept();
        return;
    }

    const auto choice = QMessageBox::warning(
        this, "Unsaved changes",
        "This screenshot has unsaved changes.\nSave before closing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (choice == QMessageBox::Save) {
        saveAs();                 // clears m_dirty on a successful save
        if (m_dirty)
            event->ignore();      // save cancelled or failed -> stay open
        else
            event->accept();
    } else if (choice == QMessageBox::Discard) {
        event->accept();
    } else {                      // Cancel
        event->ignore();
    }
}

void ImageEditor::setupUI()
{
    // Create main splitter for layout
    m_splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_splitter);

    // Create graphics view and scene using custom DrawingGraphicsView
    m_view = new DrawingGraphicsView();
    m_scene = new QGraphicsScene(this);
    m_view->setScene(m_scene);
    m_view->setAlignment(Qt::AlignCenter);   // keep small content centered in the view

    // Clean, flat canvas: a screenshot is opaque and fills its bounds, so the usual
    // "transparency" checkerboard is just noise. A neutral backdrop also makes the
    // (later) beautify background read clearly.
    const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    m_scene->setBackgroundBrush(QColor(dark ? QStringLiteral("#202124")
                                            : QStringLiteral("#ececec")));

    // Add the screenshot to the scene. The capture carries a devicePixelRatio
    // (2x/3x on Retina), so the pixmap item is laid out at its device-independent
    // size; use that for the scene rect and bounds. (Using m_originalScreenshot.rect(),
    // which is in device pixels, makes the scene 2x too big so the image lands in its
    // top-left quadrant instead of centered. The backdrop-off path already uses this.)
    m_pixmapItem = m_scene->addPixmap(m_originalScreenshot);
    const QRectF imageRect = m_pixmapItem->boundingRect();
    m_scene->setSceneRect(imageRect);

    // Set image bounds for the view (scene/logical coordinates)
    m_view->setImageBounds(imageRect.toRect());

    // Connect item clicked signal (strategies will be connected later)
    connect(m_view, &DrawingGraphicsView::itemClicked, this, &ImageEditor::onItemClicked);

    // ⌘/Ctrl+wheel over text → undoable font-size change (reuses PropertyChangeCommand,
    // whose mergeWith collapses a whole wheel spin into one undo step).
    connect(m_view, &DrawingGraphicsView::adjustTextSizeRequested, this,
            [this](Tools::ITool *tool, int steps) {
                if (!tool || steps == 0)
                    return;
                int oldSize = 0;
                for (const Tools::ToolProperty &p : tool->getProperties())
                    if (p.id == "fontSize") { oldSize = p.value.toInt(); break; }
                if (oldSize <= 0)
                    return;
                const int newSize = qBound(6, oldSize + steps, 200);
                if (newSize == oldSize)
                    return;
                m_undoStack->push(new Commands::PropertyChangeCommand(
                    tool, "fontSize", oldSize, newSize, QStringLiteral("Resize text")));
            });

    // Create right side widget with splitter for layer manager and properties
    m_rightSplitter = new QSplitter(Qt::Vertical);

    // Remove margins to align with the view
    m_rightSplitter->setContentsMargins(0, 0, 0, 0);

    // Create layer manager and properties panel
    m_layerManager = new LayerManager();
    m_layerProperties = new LayerProperties();
    // The backdrop properties panel hosts a "Save as preset…" button; route it to
    // the shared save flow (also used by the popover "+").
    connect(m_layerProperties, &LayerProperties::savePresetRequested,
            this, &ImageEditor::saveCurrentBackdropAsPreset);

    // Panel property edits become undoable commands. The backdrop uses a dedicated
    // BackdropChangeCommand because its edits have geometry/shadow side-effects that a
    // generic property command can't replay; restoring the saved snapshot re-applies
    // them via onBackdropChanged.
    connect(m_layerProperties, &LayerProperties::propertyChangeRequested, this,
            [this](Tools::ITool *tool, const QString &propId, const QVariant &value) {
                if (!tool)
                    return;
                if (auto *bd = dynamic_cast<BackdropItem*>(tool)) {
                    m_undoStack->push(new Commands::BackdropChangeCommand(
                        bd, propId, value, bd->createMemento(),
                        QStringLiteral("Backdrop %1").arg(propId)));
                    return;
                }
                QVariant oldValue;
                for (const Tools::ToolProperty &p : tool->getProperties())
                    if (p.id == propId) { oldValue = p.value; break; }
                m_undoStack->push(new Commands::PropertyChangeCommand(
                    tool, propId, oldValue, value, QStringLiteral("Change %1").arg(propId)));
            });

    // Add widgets to right splitter
    m_rightSplitter->addWidget(m_layerManager);
    m_rightSplitter->addWidget(m_layerProperties);

    // Fluid divider: Layers gets a modest initial height, Properties takes the rest
    // and absorbs drags; a wider handle makes the divider grabbable. Neither pane
    // can be collapsed to nothing.
    m_rightSplitter->setHandleWidth(6);
    m_rightSplitter->setChildrenCollapsible(false);
    m_rightSplitter->setStretchFactor(0, 0);   // Layers: keep its size on resize
    m_rightSplitter->setStretchFactor(1, 1);   // Properties: grow/shrink with the panel
    m_rightSplitter->setSizes({160, 520});

    // Add main widgets to main splitter
    m_splitter->addWidget(m_view);
    m_splitter->addWidget(m_rightSplitter);

    // Set splitter proportions (75% for view, 25% for right panel)
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    // Clean default layout: hide the Layers/Properties panel; the "Panels" toolbar
    // toggle reveals it. (Auto-revealed when a tool needs its options; see below.)
    m_rightSplitter->setVisible(false);

    // Non-blocking feedback for save/copy; auto-clearing.
    statusBar()->setSizeGripEnabled(false);
}

QIcon ImageEditor::createThemedIcon(const QString &iconPath)
{
    // Toolbar icon: pick a tone for the current theme, then hand off to the shared
    // HiDPI-correct loader so it's crisp on Retina (rendered at the toolbar's icon
    // size rather than a 1x bitmap the OS upscales).
    const QColor windowColor = QApplication::palette().color(QPalette::Window);
    const bool isDarkMode = windowColor.lightness() < 128;
    const QColor iconColor(isDarkMode ? "#d0d0d0" : "#333333");
    const int size = m_toolbar ? m_toolbar->iconSize().width() : 20;
    return Core::themedSvgIcon(iconPath, iconColor, size);
}

void ImageEditor::setupToolbar()
{
    m_toolbar = addToolBar("Tools");
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconSize(QSize(20, 20));
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);

    // --- File actions ---
    m_saveAsAction = new QAction(this);
    m_saveAsAction->setToolTip("Save As (Ctrl+S)");
    m_saveAsAction->setIcon(createThemedIcon(":/icons/icons/save.svg"));
    m_saveAsAction->setShortcut(QKeySequence::Save);
    connect(m_saveAsAction, &QAction::triggered, this, &ImageEditor::saveAs);
    m_toolbar->addAction(m_saveAsAction);

    m_copyAction = new QAction(this);
    m_copyAction->setToolTip("Copy to Clipboard (Ctrl+C)");
    m_copyAction->setIcon(createThemedIcon(":/icons/icons/copy.svg"));
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &ImageEditor::copyToClipboard);
    m_toolbar->addAction(m_copyAction);

    // Upload as a split button: click = default destination; ▾ = pick a saved server.
    m_uploadAction = new QAction(this);
    m_uploadAction->setToolTip("Upload to the default server and copy the link");
    m_uploadAction->setIcon(createThemedIcon(":/icons/icons/upload.svg"));
    connect(m_uploadAction, &QAction::triggered, this, [this] { doUpload(QString()); });
    auto *uploadButton = new QToolButton(m_toolbar);
    uploadButton->setDefaultAction(m_uploadAction);
    uploadButton->setPopupMode(QToolButton::MenuButtonPopup);
    auto *uploadMenu = new QMenu(uploadButton);
    uploadButton->setMenu(uploadMenu);
    connect(uploadMenu, &QMenu::aboutToShow, this, [this, uploadMenu] {
        Upload::rebuildUploadMenu(uploadMenu, [this](const QString &id) { doUpload(id); });
    });
    m_toolbar->addWidget(uploadButton);

    m_toolbar->addSeparator();

    // --- Undo / Redo (Command pattern). These QActions are vended by the stack:
    //     they auto enable/disable and update their text ("Undo Add Arrow"). ---
    QAction *undoAction = m_undoStack->createUndoAction(this, "Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setIcon(createThemedIcon(":/icons/icons/undo.svg"));
    m_toolbar->addAction(undoAction);

    QAction *redoAction = m_undoStack->createRedoAction(this, "Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setIcon(createThemedIcon(":/icons/icons/redo.svg"));
    m_toolbar->addAction(redoAction);

    m_toolbar->addSeparator();

    // --- Tools (registry-driven) ---
    // One exclusive action group enforces "exactly one tool selected".
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    for (const ToolSpec &spec : ToolRegistry::tools()) {
        auto *act = new QAction(this);
        act->setToolTip(spec.tooltip.isEmpty() ? spec.displayName : spec.tooltip);
        act->setIcon(createThemedIcon(spec.iconPath));
        act->setCheckable(true);
        m_toolGroup->addAction(act);
        m_toolbar->addAction(act);
        const QString id = spec.id;
        connect(act, &QAction::triggered, this, [this, id] { activateTool(id); });
        m_actions.insert(spec.id, act);

        // Pointer is the only non-drawing tool and comes first; set off the drawing
        // group with a separator after it.
        if (!spec.isDrawingTool)
            m_toolbar->addSeparator();
    }
    if (QAction *p = m_actions.value("pointer"))
        p->setChecked(true);

    // --- Background / backdrop (left group). Clicking opens a settings popover. ---
    m_toolbar->addSeparator();
    m_backgroundAction = new QAction(this);
    m_backgroundAction->setCheckable(true);
    m_backgroundAction->setToolTip("Background (padding, color/gradient/wallpaper, rounded corners, shadow)");
    m_backgroundAction->setIcon(createThemedIcon(":/icons/icons/background.svg"));
    connect(m_backgroundAction, &QAction::triggered, this, &ImageEditor::onBackgroundButtonClicked);
    m_toolbar->addAction(m_backgroundAction);

    // --- Right-aligned: panels toggle (no Fit/100%; the view auto-centers). ---
    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_panelsAction = new QAction(this);
    m_panelsAction->setCheckable(true);
    m_panelsAction->setToolTip("Show layers & properties");
    m_panelsAction->setIcon(createThemedIcon(":/icons/icons/panels.svg"));
    connect(m_panelsAction, &QAction::toggled, this,
            [this](bool on) { if (m_rightSplitter) m_rightSplitter->setVisible(on); });
    m_toolbar->addAction(m_panelsAction);
    // (Drawing tools auto-reveal this panel via activateTool's autoRevealPanel flag.)
}

void ImageEditor::setupStrategies()
{
    using namespace Interactions;

    // Created after the view, so the view (holding a non-owned interaction) dies first.
    m_layerSink = std::make_unique<LayerSink>(this);
    m_builder = new AnnotationBuilder(m_scene, m_layerSink.get(), this);
    m_builder->setImageBounds(m_pixmapItem->boundingRect().toRect());   // device-independent
    m_builder->setSourcePixmap(m_originalScreenshot);
    m_builder->setStepNumberProvider([this] { return nextStepNumber(m_layerManager->layers()); });

    if (auto *p = dynamic_cast<PointerToolInteraction*>(m_builder->interaction("pointer")))
        connect(p, &PointerToolInteraction::itemClicked, this, &ImageEditor::onItemClicked);

    // So the click that ends editing doesn't place another box.
    connect(m_builder, &AnnotationBuilder::textPlaced, this, [this] { activateTool("pointer"); });

    activateTool("pointer");   // default tool
}

void ImageEditor::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Screenshot",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/screenshot.png",
        "Image Files (*.png *.jpg *.bmp)"
    );

    if (!fileName.isEmpty()) {
        QPixmap result = renderScene();
        if (result.save(fileName)) {
            m_dirty = false;
            if (m_undoStack) m_undoStack->setClean();   // mark this the saved point
            statusBar()->showMessage(tr("Screenshot saved to %1").arg(fileName), 4000);
        } else {
            QMessageBox::warning(this, "Error", "Failed to save screenshot.");
        }
    }
}

void ImageEditor::copyToClipboard()
{
    QPixmap result = renderScene();
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setPixmap(result);
    m_dirty = false;   // the result has been exported; closing won't lose work
    if (m_undoStack) m_undoStack->setClean();
    statusBar()->showMessage(tr("Copied to clipboard"), 3000);
}

void ImageEditor::doUpload(const QString &profileId)
{
    // Validate the CHOSEN destination (empty id = default), not just the default.
    if (!Upload::UploadConfig::forProfile(profileId).isComplete()) {
        QMessageBox::information(this, tr("Upload not configured"),
                                tr("Set up an upload destination in Settings → Upload first."));
        return;
    }
    // Render to a temp PNG and hand it to the app's uploader (which outlives this
    // window and deletes the temp when the upload finishes).
    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/Snim_upload_")
                        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz"))
                        + QStringLiteral(".png");
    if (!renderScene().save(tmp)) {
        QMessageBox::warning(this, tr("Upload failed"), tr("Could not prepare the image."));
        return;
    }
    m_dirty = false;   // exported; closing won't lose work
    if (m_undoStack) m_undoStack->setClean();
    emit uploadRequested(tmp, QStringLiteral("screenshot.png"), /*deleteWhenDone=*/true, profileId);
}

QPixmap ImageEditor::renderScene()
{
    // The scene works in device-independent coordinates, so export at the capture's
    // native resolution by scaling the output up by the screenshot's devicePixelRatio
    // (otherwise a Retina capture would save at half resolution).
    QElapsedTimer perfTimer;
    perfTimer.start();
    const QRectF sceneRect = m_scene->sceneRect();
    const qreal dpr = m_originalScreenshot.devicePixelRatio();
    QPixmap pixmap((sceneRect.size() * dpr).toSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // source = the whole scene (logical); target = the full device-pixel pixmap.
    m_scene->render(&painter, QRectF(QPointF(0, 0), pixmap.size()), sceneRect);
    Core::Perf::reportElapsed("renderScene", perfTimer.elapsed(), Core::Perf::kBudgetRenderMs,
                              QStringLiteral("%1x%2 @ dpr %3").arg(pixmap.width()).arg(pixmap.height()).arg(dpr));
    return pixmap;
}

void ImageEditor::activateTool(const QString &toolId)
{
    const ToolSpec *spec = ToolRegistry::find(toolId);
    if (!spec)
        return;

    Interactions::IDrawingInteraction *strategy = m_builder->interaction(toolId);
    Tools::ITool *tmpl = m_builder->templateFor(toolId);

    // Sync the interaction's live-preview style from the template (freehand /
    // highlight / blur) so what's drawn matches what will be committed.
    if (spec->syncStrategy && strategy && tmpl)
        spec->syncStrategy(strategy, tmpl);

    if (m_view)
        m_view->setDrawingStrategy(strategy);

    m_layerManager->selectLayer(nullptr);
    if (spec->isDrawingTool && tmpl)
        m_layerProperties->setTool(tmpl, spec->displayName);   // show this tool's options
    else
        m_layerProperties->setLayer(nullptr);                  // pointer: nothing to edit

    // The exclusive QActionGroup unchecks the other tools for us.
    if (QAction *act = m_actions.value(toolId))
        act->setChecked(true);

    if (spec->autoRevealPanel && m_panelsAction)
        m_panelsAction->setChecked(true);
}

Layer *ImageEditor::makeLayer(QGraphicsItem *item, const QString &toolId)
{
    const ToolSpec *spec = ToolRegistry::find(toolId);
    if (!spec || !item) {
        delete item;
        return nullptr;
    }

    // Layer name: text uses its content; everything else uses "Prefix N".
    auto *textItem = dynamic_cast<Tools::TextTool*>(item);
    const QString name = textItem
        ? QString("Text: %1").arg(textItem->toPlainText())
        : QString("%1 %2").arg(spec->namePrefix).arg(++m_counters[toolId]);

    auto *layer = new Layer(name, spec->layerType, this);
    layer->setItem(item);

    // Text layers keep their name in sync with their (editable) content.
    if (textItem) {
        connect(textItem, &Tools::TextTool::textChanged, this, [this, layer, textItem]() {
            QString t = textItem->toPlainText();
            if (t.length() > 20)
                t = t.left(20) + "...";
            layer->setName(QString("Text: %1").arg(t));
            m_layerManager->updateLayerList();
        });
    }
    return layer;
}

void ImageEditor::commitDrawnItem(QGraphicsItem *item, const QString &toolId)
{
    Layer *layer = makeLayer(item, toolId);
    if (!layer)
        return;
    const ToolSpec *spec = ToolRegistry::find(toolId);

    // "Remember last settings": copy this item's style back into the tool template
    // so the next stroke/shape starts from the same look.
    m_builder->rememberStyle(item, toolId);

    // Push as an undoable command; its redo() adds the item to the scene + manager.
    const QString label = spec->namePrefix.isEmpty() ? QStringLiteral("Add Layer")
                                                      : QStringLiteral("Add %1").arg(spec->namePrefix);
    m_undoStack->push(new Commands::AddLayerCommand(m_scene, m_layerManager, layer, label));

    if (spec->switchToPointerAfter) {
        // Switch to the pointer and select the new item so it can be moved/resized
        // immediately; otherwise clicking it with the drawing tool creates another.
        activateTool("pointer");
        m_layerManager->selectLayer(layer);
    }
}

void ImageEditor::importAnnotations(const AnnotationSet &set)
{
    // Imported layers are the starting state, so they bypass the undo stack.
    for (const AnnotationSet::Instance &instance : set.instantiate(m_originalScreenshot)) {
        Layer *layer = makeLayer(instance.item, instance.toolId);
        if (!layer)
            continue;
        m_builder->rememberStyle(instance.item, instance.toolId);
        m_scene->addItem(instance.item);
        m_layerManager->addLayer(layer);
    }
    // The panel's "Next number" hint; stamping derives the number from the layers anyway.
    if (auto *step = dynamic_cast<Tools::StepTool*>(m_builder->templateFor("step")))
        step->setNumber(nextStepNumber(m_layerManager->layers()));
}

void ImageEditor::keyPressEvent(QKeyEvent *event)
{
    // Handling shortcuts here (rather than QAction::setShortcut) means a text item in
    // inline-edit mode consumes the keys first, so these never fire mid-typing.
    if (event->key() == Qt::Key_Escape) {
        if (m_builder->isEditingText()) {
            m_scene->focusItem()->clearFocus();   // commit/discard the text box
        } else {
            m_layerManager->selectLayer(nullptr);
            activateTool("pointer");
        }
        event->accept();
        return;
    }

    if (m_builder->isEditingText()) {      // let the inline editor keep every other key
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        m_layerManager->deleteCurrentLayer();   // delete + select neighbour (RemoveLayerCommand)
        event->accept();
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_D) {
        duplicateSelectedLayer();
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Left: case Qt::Key_Right: case Qt::Key_Up: case Qt::Key_Down: {
        const qreal step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
        const qreal dx = event->key() == Qt::Key_Left ? -step
                       : event->key() == Qt::Key_Right ? step : 0.0;
        const qreal dy = event->key() == Qt::Key_Up ? -step
                       : event->key() == Qt::Key_Down ? step : 0.0;
        nudgeSelectedLayer(dx, dy);
        event->accept();
        return;
    }
    default:
        break;
    }

    QMainWindow::keyPressEvent(event);
}

void ImageEditor::duplicateSelectedLayer()
{
    Layer *sel = m_layerManager->selectedLayer();
    if (!sel || sel->isGroup()
        || sel->type() == Layer::Background || sel->type() == Layer::Backdrop)
        return;
    auto *tool = dynamic_cast<Tools::ITool*>(sel->item());
    if (!tool)
        return;
    QGraphicsItem *copy = tool->clone();   // full duplicate (geometry + style)
    if (!copy)
        return;
    if (sel->item())
        copy->setPos(sel->item()->pos() + QPointF(12, 12));   // offset so it's visible

    // Resolve the tool id from the layer type so commitDrawnItem names/commits it.
    QString toolId;
    for (const ToolSpec &spec : ToolRegistry::tools())
        if (spec.isDrawingTool && spec.layerType == sel->type()) { toolId = spec.id; break; }
    if (toolId.isEmpty()) {
        delete copy;
        return;
    }
    commitDrawnItem(copy, toolId);   // undoable AddLayerCommand, named + selected
}

void ImageEditor::nudgeSelectedLayer(qreal dx, qreal dy)
{
    Layer *sel = m_layerManager->selectedLayer();
    if (!sel || sel->isGroup()
        || sel->type() == Layer::Background || sel->type() == Layer::Backdrop)
        return;
    if (QGraphicsItem *it = sel->item())
        m_undoStack->push(new Commands::MoveLayerCommand(it, QPointF(dx, dy),
                                                         QStringLiteral("Nudge")));
}

Layer* ImageEditor::createBackgroundLayer()
{
    auto *layer = new Layer("Background", Layer::Background, this);
    layer->setItem(m_pixmapItem);
    return layer;
}

namespace {
// Render the screenshot with rounded corners, preserving its devicePixelRatio.
// radiusDevicePx is in device pixels (matching the pixmap's own pixels).
QPixmap roundedScreenshot(const QPixmap &src, int radiusDevicePx)
{
    if (radiusDevicePx <= 0)
        return src;
    QPixmap out(src.size());
    out.fill(Qt::transparent);
    {
        QPainter p(&out);                       // out has dpr 1 here -> paint in device px
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, src.width(), src.height()),
                            radiusDevicePx, radiusDevicePx);
        p.setClipPath(path);
        QPixmap s = src;
        s.setDevicePixelRatio(1);               // draw 1:1 in device px
        p.drawPixmap(0, 0, s);
    }
    out.setDevicePixelRatio(src.devicePixelRatio()); // occupy the same scene rect as the original
    return out;
}
} // namespace

void ImageEditor::setBackdropEnabled(bool on)
{
    if (on && !m_backdropItem) {
        m_backdropItem = new BackdropItem();
        m_scene->addItem(m_backdropItem);
        m_backdropItem->setOnChanged([this](const QString &id) { onBackdropChanged(id); });

        m_backdropLayer = new Layer("Backdrop", Layer::Backdrop, this);
        m_backdropLayer->setItem(m_backdropItem);
        m_layerManager->addLayer(m_backdropLayer);

        applyRoundedScreenshot();
        applyShadow();
        applyBackdropGeometry(/*refit=*/true);
    } else if (!on && m_backdropItem) {
        if (m_backdropLayer) {
            m_layerManager->removeLayer(m_backdropLayer);
            m_layerProperties->removeLayer(m_backdropLayer);
            if (m_selectedLayer == m_backdropLayer)
                m_selectedLayer = nullptr;
            m_backdropLayer->deleteLater();
            m_backdropLayer = nullptr;
        }
        m_scene->removeItem(m_backdropItem);
        delete m_backdropItem;
        m_backdropItem = nullptr;

        // Restore the bare screenshot (no rounding, no shadow) and tight sceneRect.
        m_pixmapItem->setGraphicsEffect(nullptr);
        m_pixmapItem->setPixmap(m_originalScreenshot);
        m_scene->setSceneRect(m_pixmapItem->sceneBoundingRect());
        if (m_view) m_view->fitContent();

        // Reflect the off state in the toolbar button and close the popover.
        if (m_backgroundAction) m_backgroundAction->setChecked(false);
        if (m_backdropPopover) m_backdropPopover->hide();
    }
}

void ImageEditor::onBackgroundButtonClicked()
{
    // Toggle the settings popover; enable the backdrop on first open. Removal is
    // done from inside the popover (so the button never disables it directly).
    if (m_backdropPopover && m_backdropPopover->isVisible()) {
        m_backdropPopover->hide();
    } else {
        if (!m_backdropItem)
            setBackdropEnabled(true);
        showBackdropPopover();
    }
    if (m_backgroundAction)
        m_backgroundAction->setChecked(m_backdropItem != nullptr); // checked == backdrop active
}

bool ImageEditor::eventFilter(QObject *obj, QEvent *event)
{
    // Close the backdrop popover on a click outside it, but tolerate clicks in a
    // child combo dropdown / the color dialog, and on the Background button itself.
    if (event->type() == QEvent::MouseButtonPress && m_backdropPopover && m_backdropPopover->isVisible()) {
        if (!QApplication::activePopupWidget() && !QApplication::activeModalWidget()) {
            const QPoint gp = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
            bool onButton = false;
            if (QWidget *btn = m_toolbar->widgetForAction(m_backgroundAction))
                onButton = QRect(btn->mapToGlobal(QPoint(0, 0)), btn->size()).contains(gp);
            if (!onButton && !m_backdropPopover->frameGeometry().contains(gp))
                m_backdropPopover->hide();
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// Composite a small "★" badge in the top-right corner of a preset preview to
// mark the default. The pixmap is DPR-1 (configPreview builds it at logical size).
static QPixmap withDefaultBadge(QPixmap pm)
{
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal r = 14.0;
    const QRectF badge(pm.width() - r - 3.0, 3.0, r, r);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 230));
    p.drawEllipse(badge);
    QFont f = p.font();
    f.setPointSizeF(8.5);
    p.setFont(f);
    p.setPen(QColor(0xf5, 0xa6, 0x23)); // amber star
    p.drawText(badge, Qt::AlignCenter, QStringLiteral("★"));
    return pm;
}

void ImageEditor::showBackdropPopover()
{
    if (!m_backdropItem || !m_backdropLayer)
        return;

    if (!m_backdropPopover) {
        // Quick-actions popover: preset tiles + save / Details / Remove. Full
        // controls live in the side Properties panel (via "Details…"). Frameless
        // Qt::Tool (not Qt::Popup) so child menus/dialogs don't dismiss it.
        m_backdropPopover = new QWidget(this, Qt::Tool | Qt::FramelessWindowHint);
        m_backdropPopover->setObjectName("backdropPopover");
        m_backdropPopover->setStyleSheet(
            "#backdropPopover { background: palette(window); border: 1px solid palette(mid);"
            " border-radius: 10px; }"
            "#popoverClose { border: none; color: palette(mid); font-size: 13px; }"
            "#popoverClose:hover { color: palette(text); }"
            "#popoverCaption { color: palette(mid); font-size: 10px; font-weight: bold;"
            " letter-spacing: 1px; }"
            "QPushButton#detailsBtn { padding: 6px; border-radius: 6px;"
            " border: 1px solid palette(mid); background: palette(button); }"
            "QPushButton#detailsBtn:hover { border-color: palette(highlight); }"
            "QPushButton#removeBtn { padding: 6px; border-radius: 6px; border: none;"
            " color: #d9534f; background: transparent; }"
            "QPushButton#removeBtn:hover { background: rgba(217,83,79,0.12); }");
        auto *outer = new QVBoxLayout(m_backdropPopover);
        outer->setContentsMargins(14, 12, 14, 14);
        outer->setSpacing(10);
        qApp->installEventFilter(this); // close on outside click

        auto *header = new QHBoxLayout();
        auto *title = new QLabel("Background", m_backdropPopover);
        QFont tf = title->font();
        tf.setBold(true);
        tf.setPointSizeF(tf.pointSizeF() + 1);
        title->setFont(tf);
        auto *closeBtn = new QToolButton(m_backdropPopover);
        closeBtn->setObjectName("popoverClose");
        closeBtn->setText("✕");
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setAutoRaise(true);
        connect(closeBtn, &QToolButton::clicked, m_backdropPopover, &QWidget::hide);
        header->addWidget(title);
        header->addStretch();
        header->addWidget(closeBtn);
        outer->addLayout(header);

        auto *caption = new QLabel("PRESETS", m_backdropPopover);
        caption->setObjectName("popoverCaption");
        outer->addWidget(caption);

        m_presetGrid = new QWidget(m_backdropPopover);
        auto *grid = new QGridLayout(m_presetGrid);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(8);
        outer->addWidget(m_presetGrid);

        auto *divider = new QFrame(m_backdropPopover);
        divider->setFrameShape(QFrame::HLine);
        divider->setFrameShadow(QFrame::Plain);
        divider->setStyleSheet("color: palette(mid);");
        divider->setFixedHeight(1);
        outer->addWidget(divider);

        auto *detailsBtn = new QPushButton("Details…", m_backdropPopover);
        detailsBtn->setObjectName("detailsBtn");
        detailsBtn->setCursor(Qt::PointingHandCursor);
        detailsBtn->setToolTip("Edit all backdrop options in the side panel");
        connect(detailsBtn, &QPushButton::clicked, this, [this] {
            if (m_backdropLayer) {
                m_layerManager->selectLayer(m_backdropLayer);
                if (m_panelsAction) m_panelsAction->setChecked(true);
                m_layerProperties->setLayer(m_backdropLayer);
            }
            if (m_backdropPopover) m_backdropPopover->hide();
        });
        outer->addWidget(detailsBtn);

        auto *removeBtn = new QPushButton("Remove background", m_backdropPopover);
        removeBtn->setObjectName("removeBtn");
        removeBtn->setCursor(Qt::PointingHandCursor);
        connect(removeBtn, &QPushButton::clicked, this, [this] {
            setBackdropEnabled(false);
            if (m_backdropPopover) m_backdropPopover->hide();
        });
        outer->addWidget(removeBtn);

        m_backdropPopover->setFixedWidth(268);
    }

    rebuildPresetGrid();
    m_backdropPopover->adjustSize();

    // Anchor under the Background toolbar button.
    if (QWidget *btn = m_toolbar->widgetForAction(m_backgroundAction)) {
        const QPoint p = btn->mapToGlobal(QPoint(0, btn->height() + 4));
        m_backdropPopover->move(p);
    }
    m_backdropPopover->show();
    m_backdropPopover->raise();
}

void ImageEditor::rebuildPresetGrid()
{
    if (!m_presetGrid)
        return;
    auto *grid = dynamic_cast<QGridLayout *>(m_presetGrid->layout());
    if (!grid)
        return;

    // Clear existing tiles.
    while (QLayoutItem *item = grid->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    const QSize tile(64, 44);                              // preview (icon) size
    const QSize btnSize(tile.width() + 12, tile.height() + 12);  // uniform tile button
    const int columns = 3;
    const QString def = BackdropPresets::defaultName();
    const QString tileStyle =
        "QToolButton { border: 2px solid transparent; border-radius: 9px; }"
        "QToolButton:hover { border-color: palette(mid); }"
        "QToolButton:checked { border-color: palette(highlight); }";

    int idx = 0;
    for (const BackdropPreset &preset : BackdropPresets::all()) {
        const bool isDefault = (preset.name == def);
        auto *btn = new QToolButton(m_presetGrid);
        QPixmap preview = BackdropItem::configPreview(preset.config, tile);
        if (isDefault)
            preview = withDefaultBadge(preview);
        btn->setIcon(QIcon(preview));
        btn->setIconSize(tile);
        btn->setFixedSize(btnSize);
        btn->setCheckable(true);
        btn->setChecked(preset.name == m_backdropItem->activePreset());
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(preset.name + (isDefault ? "  ·  default" : ""));
        btn->setStyleSheet(tileStyle);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);

        const QString name = preset.name;
        const QVariantMap config = preset.config;
        const bool builtin = preset.builtin;
        connect(btn, &QToolButton::clicked, this, [this, config, name] {
            if (!m_backdropItem) return;
            m_backdropItem->applyConfig(config);
            m_backdropItem->setActivePreset(name);   // set after applyConfig so it isn't cleared
            rebuildPresetGrid();
        });
        connect(btn, &QToolButton::customContextMenuRequested, this, [this, btn, name, builtin](const QPoint &pos) {
            QMenu menu;
            QAction *setDefault = menu.addAction("Set as default");
            QAction *deleteAct = builtin ? nullptr : menu.addAction("Delete preset");
            QAction *chosen = menu.exec(btn->mapToGlobal(pos));
            if (chosen && chosen == setDefault) {
                BackdropPresets::setDefault(name);
                rebuildPresetGrid();
            } else if (deleteAct && chosen == deleteAct) {
                BackdropPresets::remove(name);
                if (m_backdropItem->activePreset() == name) m_backdropItem->setActivePreset(QString());
                rebuildPresetGrid();
            }
        });
        grid->addWidget(btn, idx / columns, idx % columns);
        ++idx;
    }

    // "+" tile saves the current backdrop config as a new preset.
    auto *addBtn = new QToolButton(m_presetGrid);
    addBtn->setText("+");
    addBtn->setFixedSize(btnSize);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setToolTip("Save current backdrop as a preset");
    QFont af = addBtn->font();
    af.setPointSize(18);
    addBtn->setFont(af);
    addBtn->setStyleSheet(
        "QToolButton { border: 2px dashed palette(mid); border-radius: 9px; color: palette(mid); }"
        "QToolButton:hover { border-color: palette(highlight); color: palette(text); }");
    connect(addBtn, &QToolButton::clicked, this, [this] { saveCurrentBackdropAsPreset(); });
    grid->addWidget(addBtn, idx / columns, idx % columns);
}

void ImageEditor::saveCurrentBackdropAsPreset()
{
    if (!m_backdropItem)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Save backdrop preset",
                                               "Preset name:", QLineEdit::Normal, QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) {
        BackdropPresets::save(name.trimmed(), m_backdropItem->toConfig());
        m_backdropItem->setActivePreset(name.trimmed());
        rebuildPresetGrid();   // no-op if the popover grid was never built
        // Refresh the side panel so the new thumbnail appears in its preset grid.
        if (m_backdropLayer && m_selectedLayer == m_backdropLayer)
            m_layerProperties->rebuildLayer(m_backdropLayer);
    }
}

void ImageEditor::applyBackdropGeometry(bool refit)
{
    if (!m_backdropItem)
        return;
    const QRectF shot = m_pixmapItem->sceneBoundingRect();
    // Scale logical padding to scene units (scene unit == device px of the pixmap).
    const qreal scale = shot.width() > 0 ? m_originalScreenshot.width() / shot.width() : 1.0;
    const qreal pad = m_backdropItem->padding() * scale;
    const QRectF expanded = shot.adjusted(-pad, -pad, pad, pad);
    m_backdropItem->setCanvasRect(expanded);
    m_scene->setSceneRect(expanded);
    if (refit && m_view)
        m_view->fitContent();
}

void ImageEditor::applyRoundedScreenshot()
{
    if (!m_backdropItem)
        return;
    const QRectF shot = m_pixmapItem->sceneBoundingRect();
    const qreal scale = shot.width() > 0 ? m_originalScreenshot.width() / shot.width() : 1.0;
    const int radiusDevice = int(m_backdropItem->cornerRadius() * scale);
    m_pixmapItem->setPixmap(roundedScreenshot(m_originalScreenshot, radiusDevice));
}

void ImageEditor::applyShadow()
{
    if (!m_backdropItem)
        return;
    auto *shadow = dynamic_cast<QGraphicsDropShadowEffect *>(m_pixmapItem->graphicsEffect());
    if (!shadow) {
        shadow = new QGraphicsDropShadowEffect(this);
        m_pixmapItem->setGraphicsEffect(shadow);
    }
    const qreal blur = m_backdropItem->shadowBlur();
    shadow->setBlurRadius(blur * 1.6);
    shadow->setColor(QColor(0, 0, 0, int(255 * m_backdropItem->shadowOpacity())));
    shadow->setOffset(0, blur * 0.35);
}

void ImageEditor::onBackdropChanged(const QString &propertyId)
{
    m_dirty = true;   // any backdrop tweak is an unsaved change

    // A whole config was applied (preset / default), so re-apply everything.
    if (propertyId == "config") {
        applyRoundedScreenshot();
        applyShadow();
        applyBackdropGeometry(/*refit=*/true);
        // Deferred so the active-preset name (set by the caller right after
        // applyConfig) is current when the panel / popover rebuild reads it.
        QTimer::singleShot(0, this, [this] {
            if (m_backdropLayer && m_selectedLayer == m_backdropLayer)
                m_layerProperties->rebuildLayer(m_backdropLayer);
            if (m_backdropPopover && m_backdropPopover->isVisible())
                rebuildPresetGrid();
        });
        return;
    }

    // A manual tweak diverged from the active preset (BackdropItem already cleared
    // it); refresh the popover ring if it's open.
    if (m_backdropPopover && m_backdropPopover->isVisible())
        rebuildPresetGrid();

    // These all change which/what controls should show (fill type, or the
    // preset-vs-manual gradient state + Start/End colors), so rebuild the panel.
    if (propertyId == "fill" || propertyId == "gradient"
        || propertyId == "gradStart" || propertyId == "gradEnd") {
        if (m_backdropItem)
            m_backdropItem->update();
        // Deferred so we don't delete the combo box / swatch from inside its own signal.
        QTimer::singleShot(0, this, [this] {
            if (m_backdropLayer && m_selectedLayer == m_backdropLayer)
                m_layerProperties->rebuildLayer(m_backdropLayer);
        });
    } else if (propertyId == "padding") {
        applyBackdropGeometry(/*refit=*/true);
    } else if (propertyId == "radius") {
        applyRoundedScreenshot();
    } else if (propertyId == "shadow") {
        applyShadow();
    } else if (m_backdropItem) {
        m_backdropItem->update();   // color / gradient / wallpaper preset: just repaint
    }
}

// ...existing implementation methods...

void ImageEditor::onLayerVisibilityChanged(Layer *layer, bool visible)
{
    // Layer visibility is handled automatically by the Layer class
    m_dirty = true;
}

void ImageEditor::onDeleteLayerRequested(Layer *layer)
{
    if (!layer || layer->type() == Layer::Background || layer->type() == Layer::Backdrop) {
        return;   // backdrop has its own teardown (setBackdropEnabled(false))
    }

    // Undoable removal: the command pulls the item from the scene/manager and,
    // while it sits off-scene, owns it (freeing it only if the command is dropped).
    const QString label = QStringLiteral("Delete %1").arg(layer->name());
    m_undoStack->push(new Commands::RemoveLayerCommand(
        m_scene, m_layerManager, m_layerProperties, layer, label));
    m_dirty = true;
}

void ImageEditor::onLayerSelected(Layer *layer)
{
    // Update the Properties panel content, but do NOT force the panel open:
    // selecting an element only shows its properties if the panel is already visible.
    m_selectedLayer = layer;
    m_layerProperties->setLayer(layer);

    // Also select the item on the drawing board
    if (layer && layer->item()) {
        // Clear current selection
        m_scene->clearSelection();

        // Select the layer's item (but don't select background)
        if (layer->type() != Layer::Background) {
            layer->item()->setSelected(true);
        }
    }
}

void ImageEditor::onItemClicked(QGraphicsItem *item)
{
    selectLayerByItem(item);
}

void ImageEditor::selectLayerByItem(QGraphicsItem *item)
{
    for (Layer *layer : m_layerManager->layers()) {
        if (layer->item() == item) {
            m_layerManager->selectLayer(layer);
            break;
        }
    }
}

bool ImageEditor::isWithinImageBounds(const QPoint &point) const
{
    return QRect(QPoint(0, 0), m_originalScreenshot.deviceIndependentSize().toSize()).contains(point);
}

QPoint ImageEditor::clampToImageBounds(const QPoint &point) const
{
    QRect bounds(QPoint(0, 0), m_originalScreenshot.deviceIndependentSize().toSize());
    int clampedX = qMax(bounds.left(), qMin(bounds.right(), point.x()));
    int clampedY = qMax(bounds.top(), qMin(bounds.bottom(), point.y()));
    return QPoint(clampedX, clampedY);
}

void ImageEditor::mousePressEvent(QMouseEvent *event)
{
    QMainWindow::mousePressEvent(event);
}

void ImageEditor::mouseMoveEvent(QMouseEvent *event)
{
    QMainWindow::mouseMoveEvent(event);
}

void ImageEditor::mouseReleaseEvent(QMouseEvent *event)
{
    QMainWindow::mouseReleaseEvent(event);
}

} // namespace Editor::Image
