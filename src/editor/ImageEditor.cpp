#include "ImageEditor.h"
#include "DrawingGraphicsView.h"
#include "LayerManager.h"
#include "LayerProperties.h"
#include "Layer.h"
#include "tools/TextTool.h"
#include "tools/ArrowTool.h"
#include "tools/RectangleTool.h"
#include "tools/EllipseTool.h"
#include "tools/FreehandTool.h"
#include "tools/HighlightTool.h"
#include "tools/BlurTool.h"
#include "tools/BackdropItem.h"
#include "BackdropPresets.h"
#include "interactions/PointerToolInteraction.h"
#include "interactions/ArrowDrawingInteraction.h"
#include "interactions/TextDrawingInteraction.h"
#include "interactions/RectangleDrawingInteraction.h"
#include "interactions/EllipseDrawingInteraction.h"
#include "interactions/FreehandDrawingInteraction.h"
#include "interactions/HighlightDrawingInteraction.h"
#include "interactions/BlurDrawingInteraction.h"
#include <QGraphicsPixmapItem>
#include <QGraphicsLineItem>
#include <QInputDialog>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QStyle>
#include <QKeySequence>
#include <QTimer>
#include <QStandardPaths>
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
#include <cmath>

namespace ImageEditor {

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
    , m_pointerAction(nullptr)
    , m_arrowAction(nullptr)
    , m_textAction(nullptr)
    , m_rectangleAction(nullptr)
    , m_ellipseAction(nullptr)
    , m_freehandAction(nullptr)
    , m_splitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_layerManager(nullptr)
    , m_layerProperties(nullptr)
    , m_originalScreenshot(screenshot)
    , m_currentTool(None)
    , m_drawing(false)
    , m_currentArrow(nullptr)
    , m_backgroundLayer(nullptr)
    , m_backdropItem(nullptr)
    , m_backdropLayer(nullptr)
    , m_lastFreehandLayer(nullptr)
    , m_lastHighlightLayer(nullptr)
    , m_lastBlurLayer(nullptr)
    , m_pointerStrategy(nullptr)
    , m_arrowStrategy(nullptr)
    , m_textStrategy(nullptr)
    , m_rectangleStrategy(nullptr)
    , m_ellipseStrategy(nullptr)
    , m_freehandStrategy(nullptr)
    , m_highlightStrategy(nullptr)
    , m_blurStrategy(nullptr)
    , m_textTemplate(nullptr)
    , m_arrowTemplate(nullptr)
    , m_rectangleTemplate(nullptr)
    , m_ellipseTemplate(nullptr)
    , m_freehandTemplate(nullptr)
    , m_highlightTemplate(nullptr)
    , m_blurTemplate(nullptr)
{
    qDebug() << "ImageEditor constructor called with screenshot size:" << screenshot.size();
    qDebug() << "Setting window title...";
    setWindowTitle("Image Editor");

    qDebug() << "About to call setupUI()...";
    setupUI();
    qDebug() << "setupUI() completed successfully";

    qDebug() << "About to call setupToolbar()...";
    setupToolbar();
    qDebug() << "setupToolbar() completed successfully";

    qDebug() << "About to call setupStrategies()...";
    setupStrategies();
    qDebug() << "setupStrategies() completed successfully";

    qDebug() << "About to call setupToolTemplates()...";
    setupToolTemplates();
    qDebug() << "setupToolTemplates() completed successfully";

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

    // Load stylesheet
    QFile styleFile(":/styles/styles/editor.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    // Open at a consistent 70% of the current screen (the one under the cursor,
    // where the capture happened) — independent of the capture's size, so a small
    // grab doesn't open a cramped window you have to resize. The capture is shown
    // at 100% in a scrollable view; centered on the same screen in showEvent().
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
    // A freshly-opened capture — even with the auto default backdrop — counts as
    // clean, so closing it without edits doesn't prompt to save.
    m_dirty = false;

    qDebug() << "ImageEditor constructor completed successfully";
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
    if (!m_dirty) {
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

    // Clean, flat canvas — a screenshot is opaque and fills its bounds, so the
    // usual "transparency" checkerboard is just noise. A neutral backdrop also
    // makes the (later) beautify background read clearly.
    const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
    m_scene->setBackgroundBrush(QColor(dark ? QStringLiteral("#202124")
                                            : QStringLiteral("#ececec")));

    // Add the screenshot to the scene
    m_pixmapItem = m_scene->addPixmap(m_originalScreenshot);
    m_scene->setSceneRect(m_originalScreenshot.rect());

    // Set image bounds for the view
    m_view->setImageBounds(m_originalScreenshot.rect());

    // Connect item clicked signal (strategies will be connected later)
    connect(m_view, &DrawingGraphicsView::itemClicked, this, &ImageEditor::onItemClicked);

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
    // toggle reveals it. (Auto-revealed when a tool needs its options — see below.)
    m_rightSplitter->setVisible(false);
}

QIcon ImageEditor::createThemedIcon(const QString &iconPath)
{
    // Detect if we're in dark mode by checking the palette
    QPalette palette = QApplication::palette();
    QColor windowColor = palette.color(QPalette::Window);
    bool isDarkMode = windowColor.lightness() < 128;

    // Load the SVG file and replace currentColor with appropriate color
    QFile file(iconPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open icon file:" << iconPath;
        return QIcon();
    }

    QString svgContent = QString::fromUtf8(file.readAll());
    file.close();

    // Replace "currentColor" with appropriate color based on theme
    QString iconColor = isDarkMode ? "#d0d0d0" : "#333333";  // Light gray for dark mode, dark gray for light mode
    svgContent.replace("currentColor", iconColor);

    // Save modified SVG to temporary buffer and create QPixmap
    QByteArray svgData = svgContent.toUtf8();

    // Use QPixmap to load the SVG data directly
    QPixmap pixmap;
    pixmap.loadFromData(svgData, "SVG");

    return QIcon(pixmap);
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

    m_toolbar->addSeparator();

    // --- Selection ---
    m_pointerAction = new QAction(this);
    m_pointerAction->setToolTip("Pointer (V)");
    m_pointerAction->setIcon(createThemedIcon(":/icons/icons/pointer.svg"));
    m_pointerAction->setCheckable(true);
    m_pointerAction->setChecked(true);
    connect(m_pointerAction, &QAction::triggered, this, &ImageEditor::selectPointerTool);
    m_toolbar->addAction(m_pointerAction);

    m_toolbar->addSeparator();

    // --- Drawing tools ---
    m_arrowAction = new QAction(this);
    m_arrowAction->setToolTip("Arrow");
    m_arrowAction->setIcon(createThemedIcon(":/icons/icons/arrow.svg"));
    m_arrowAction->setCheckable(true);
    connect(m_arrowAction, &QAction::triggered, this, &ImageEditor::selectArrowTool);
    m_toolbar->addAction(m_arrowAction);

    m_textAction = new QAction(this);
    m_textAction->setToolTip("Text");
    m_textAction->setIcon(createThemedIcon(":/icons/icons/text.svg"));
    m_textAction->setCheckable(true);
    connect(m_textAction, &QAction::triggered, this, &ImageEditor::selectTextTool);
    m_toolbar->addAction(m_textAction);

    m_rectangleAction = new QAction(this);
    m_rectangleAction->setToolTip("Rectangle");
    m_rectangleAction->setIcon(createThemedIcon(":/icons/icons/rectangle.svg"));
    m_rectangleAction->setCheckable(true);
    connect(m_rectangleAction, &QAction::triggered, this, &ImageEditor::selectRectangleTool);
    m_toolbar->addAction(m_rectangleAction);

    m_ellipseAction = new QAction(this);
    m_ellipseAction->setToolTip("Ellipse");
    m_ellipseAction->setIcon(createThemedIcon(":/icons/icons/ellipse.svg"));
    m_ellipseAction->setCheckable(true);
    connect(m_ellipseAction, &QAction::triggered, this, &ImageEditor::selectEllipseTool);
    m_toolbar->addAction(m_ellipseAction);

    m_freehandAction = new QAction(this);
    m_freehandAction->setToolTip("Freehand");
    m_freehandAction->setIcon(createThemedIcon(":/icons/icons/freehand.svg"));
    m_freehandAction->setCheckable(true);
    connect(m_freehandAction, &QAction::triggered, this, &ImageEditor::selectFreehandTool);
    m_toolbar->addAction(m_freehandAction);

    m_highlightAction = new QAction(this);
    m_highlightAction->setToolTip("Highlight");
    m_highlightAction->setIcon(createThemedIcon(":/icons/icons/highlight.svg"));
    m_highlightAction->setCheckable(true);
    connect(m_highlightAction, &QAction::triggered, this, &ImageEditor::selectHighlightTool);
    m_toolbar->addAction(m_highlightAction);

    m_blurAction = new QAction(this);
    m_blurAction->setToolTip("Blur");
    m_blurAction->setIcon(createThemedIcon(":/icons/icons/blur.svg"));
    m_blurAction->setCheckable(true);
    connect(m_blurAction, &QAction::triggered, this, &ImageEditor::selectBlurTool);
    m_toolbar->addAction(m_blurAction);

    // --- Background / backdrop (left group). Clicking opens a settings popover. ---
    m_toolbar->addSeparator();
    m_backgroundAction = new QAction(this);
    m_backgroundAction->setCheckable(true);
    m_backgroundAction->setToolTip("Background (padding, color/gradient/wallpaper, rounded corners, shadow)");
    m_backgroundAction->setIcon(createThemedIcon(":/icons/icons/background.svg"));
    connect(m_backgroundAction, &QAction::triggered, this, &ImageEditor::onBackgroundButtonClicked);
    m_toolbar->addAction(m_backgroundAction);

    // --- Right-aligned: panels toggle (no Fit/100% — the view auto-centers). ---
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

    // Auto-reveal the side panel when a drawing tool is picked so its options are
    // visible (hidden by default; the user can close it with the Panels toggle).
    for (QAction *a : {m_arrowAction, m_textAction, m_rectangleAction, m_ellipseAction,
                       m_freehandAction, m_highlightAction, m_blurAction}) {
        connect(a, &QAction::triggered, this,
                [this] { if (m_panelsAction) m_panelsAction->setChecked(true); });
    }
}

void ImageEditor::setupStrategies()
{
    // Create all drawing interactions
    m_pointerStrategy = new Interactions::PointerToolInteraction(this);
    m_arrowStrategy = new Interactions::ArrowDrawingInteraction(this);
    m_textStrategy = new Interactions::TextDrawingInteraction(this);
    m_rectangleStrategy = new Interactions::RectangleDrawingInteraction(this);
    m_ellipseStrategy = new Interactions::EllipseDrawingInteraction(this);
    m_freehandStrategy = new Interactions::FreehandDrawingInteraction(this);
    m_highlightStrategy = new Interactions::HighlightDrawingInteraction(this);
    m_blurStrategy = new Interactions::BlurDrawingInteraction(this);

    // Set image bounds for all strategies that need it
    m_arrowStrategy->setImageBounds(m_originalScreenshot.rect());
    m_rectangleStrategy->setImageBounds(m_originalScreenshot.rect());
    m_ellipseStrategy->setImageBounds(m_originalScreenshot.rect());
    m_freehandStrategy->setImageBounds(m_originalScreenshot.rect());
    m_highlightStrategy->setImageBounds(m_originalScreenshot.rect());
    m_blurStrategy->setImageBounds(m_originalScreenshot.rect());
    m_blurStrategy->setSourcePixmap(m_originalScreenshot);

    // Connect interaction signals to ImageEditor slots
    connect(m_pointerStrategy, &Interactions::PointerToolInteraction::itemClicked,
            this, &ImageEditor::onItemClicked);

    connect(m_arrowStrategy, &Interactions::ArrowDrawingInteraction::arrowDrawn,
            this, &ImageEditor::addArrowLayer);

    connect(m_textStrategy, &Interactions::TextDrawingInteraction::textRequested,
            this, [this](const QPoint &position) {
                bool ok;
                QString text = QInputDialog::getText(this, "Add Text", "Enter text:", QLineEdit::Normal, "", &ok);
                if (ok && !text.isEmpty()) {
                    addTextLayer(position, text);
                    // Automatically switch back to pointer tool after adding text
                    selectPointerTool();
                }
            });

    connect(m_rectangleStrategy, &Interactions::RectangleDrawingInteraction::rectangleDrawn,
            this, &ImageEditor::addRectangleLayer);

    connect(m_ellipseStrategy, &Interactions::EllipseDrawingInteraction::ellipseDrawn,
            this, &ImageEditor::addEllipseLayer);

    connect(m_freehandStrategy, &Interactions::FreehandDrawingInteraction::freehandDrawn,
            this, &ImageEditor::addFreehandLayer);

    connect(m_highlightStrategy, &Interactions::HighlightDrawingInteraction::highlightDrawn,
            this, &ImageEditor::addHighlightLayer);

    connect(m_blurStrategy, &Interactions::BlurDrawingInteraction::blurDrawn,
            this, &ImageEditor::addBlurLayer);

    // Set pointer tool as default
    m_view->setDrawingStrategy(m_pointerStrategy);
}

void ImageEditor::setupToolTemplates()
{
    // Create template tool instances for property preview
    // These are NOT added to the scene, only used for showing properties

    // Text template
    m_textTemplate = new Tools::TextTool("Sample Text", nullptr);
    auto *textTool = dynamic_cast<Tools::TextTool*>(m_textTemplate);
    if (textTool) {
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        textTool->setDefaultTextColor(foregroundColor);
    }

    // Arrow template
    m_arrowTemplate = new Tools::ArrowTool(QPointF(0, 0), QPointF(100, 100), nullptr);
    auto *arrowTool = dynamic_cast<Tools::ArrowTool*>(m_arrowTemplate);
    if (arrowTool) {
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        arrowTool->setPen(QPen(foregroundColor, 3));
    }

    // Rectangle template
    m_rectangleTemplate = new Tools::RectangleTool(QRect(0, 0, 100, 100), nullptr);
    auto *rectangleTool = dynamic_cast<Tools::RectangleTool*>(m_rectangleTemplate);
    if (rectangleTool) {
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        rectangleTool->setPen(QPen(foregroundColor, 2));
    }

    // Ellipse template
    m_ellipseTemplate = new Tools::EllipseTool(QRect(0, 0, 100, 100), nullptr);
    auto *ellipseTool = dynamic_cast<Tools::EllipseTool*>(m_ellipseTemplate);
    if (ellipseTool) {
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        ellipseTool->setPen(QPen(foregroundColor, 2));
    }

    // Freehand template
    m_freehandTemplate = new Tools::FreehandTool(nullptr);
    auto *freehandTool = dynamic_cast<Tools::FreehandTool*>(m_freehandTemplate);
    if (freehandTool) {
        freehandTool->setPen(getCurrentFreehandPen());
    }

    // Highlight template
    m_highlightTemplate = new Tools::HighlightTool(nullptr);
    auto *highlightTool = dynamic_cast<Tools::HighlightTool*>(m_highlightTemplate);
    if (highlightTool) {
        highlightTool->setColor(getCurrentHighlightColor());
        highlightTool->setWidth(getCurrentHighlightWidth());
    }

    // Blur template
    m_blurTemplate = new Tools::BlurTool(nullptr);
    auto *blurTool = dynamic_cast<Tools::BlurTool*>(m_blurTemplate);
    if (blurTool) {
        blurTool->setBlurRadius(getCurrentBlurRadius());
        blurTool->setBrushWidth(getCurrentBlurBrushWidth());
    }
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
            QMessageBox::information(this, "Success", "Screenshot saved successfully!");
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
    QMessageBox::information(this, "Success", "Screenshot copied to clipboard!");
}

QPixmap ImageEditor::renderScene()
{
    QPixmap pixmap(m_scene->sceneRect().size().toSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    m_scene->render(&painter);
    return pixmap;
}

void ImageEditor::selectPointerTool()
{
    m_view->setDrawingStrategy(m_pointerStrategy);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_freehandAction->setChecked(false);
    m_highlightAction->setChecked(false);
    m_blurAction->setChecked(false);
    m_pointerAction->setChecked(true);

    // Deselect any layer and hide properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setLayer(nullptr);
}

void ImageEditor::selectArrowTool()
{
    m_view->setDrawingStrategy(m_arrowStrategy);
    m_pointerAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_freehandAction->setChecked(false);
    m_highlightAction->setChecked(false);
    m_blurAction->setChecked(false);
    m_arrowAction->setChecked(true);

    // Deselect any layer and show arrow tool properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setTool(m_arrowTemplate, "Arrow Tool");
}

void ImageEditor::selectTextTool()
{
    m_view->setDrawingStrategy(m_textStrategy);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_freehandAction->setChecked(false);
    m_highlightAction->setChecked(false);
    m_blurAction->setChecked(false);
    m_textAction->setChecked(true);

    // Deselect any layer and show text tool properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setTool(m_textTemplate, "Text Tool");
}

void ImageEditor::selectRectangleTool()
{
    m_view->setDrawingStrategy(m_rectangleStrategy);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_freehandAction->setChecked(false);
    m_highlightAction->setChecked(false);
    m_blurAction->setChecked(false);
    m_rectangleAction->setChecked(true);

    // Deselect any layer and show rectangle tool properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setTool(m_rectangleTemplate, "Rectangle Tool");
}

void ImageEditor::selectEllipseTool()
{
    m_view->setDrawingStrategy(m_ellipseStrategy);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_freehandAction->setChecked(false);
    m_highlightAction->setChecked(false);
    m_blurAction->setChecked(false);
    m_ellipseAction->setChecked(true);

    // Deselect any layer and show ellipse tool properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setTool(m_ellipseTemplate, "Ellipse Tool");
}

void ImageEditor::selectFreehandTool()
{
    // Update freehand pen settings from last layer before setting strategy and template
    QPen currentPen = getCurrentFreehandPen();
    m_freehandStrategy->setPen(currentPen);
    auto *freehandTool = dynamic_cast<Tools::FreehandTool*>(m_freehandTemplate);
    if (freehandTool) {
        freehandTool->setPen(currentPen);
    }

    m_view->setDrawingStrategy(m_freehandStrategy);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_freehandAction->setChecked(true);
    m_highlightAction->setChecked(false);
    m_blurAction->setChecked(false);

    // Deselect any layer and show freehand tool properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setTool(m_freehandTemplate, "Freehand Tool");
}

void ImageEditor::selectHighlightTool()
{
    // Update highlight color and width settings from last layer before setting strategy and template
    QColor currentColor = getCurrentHighlightColor();
    qreal currentWidth = getCurrentHighlightWidth();
    m_highlightStrategy->setColor(currentColor);
    m_highlightStrategy->setWidth(currentWidth);

    auto *highlightTool = dynamic_cast<Tools::HighlightTool*>(m_highlightTemplate);
    if (highlightTool) {
        highlightTool->setColor(currentColor);
        highlightTool->setWidth(currentWidth);
    }

    m_view->setDrawingStrategy(m_highlightStrategy);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_freehandAction->setChecked(false);
    m_highlightAction->setChecked(true);
    m_blurAction->setChecked(false);

    // Deselect any layer and show highlight tool properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setTool(m_highlightTemplate, "Highlight Tool");
}

void ImageEditor::selectBlurTool()
{
    // Update blur settings from last layer before setting strategy and template
    qreal currentBlurRadius = getCurrentBlurRadius();
    qreal currentBrushWidth = getCurrentBlurBrushWidth();
    m_blurStrategy->setBlurRadius(currentBlurRadius);
    m_blurStrategy->setBrushWidth(currentBrushWidth);

    auto *blurTool = dynamic_cast<Tools::BlurTool*>(m_blurTemplate);
    if (blurTool) {
        blurTool->setBlurRadius(currentBlurRadius);
        blurTool->setBrushWidth(currentBrushWidth);
    }

    m_view->setDrawingStrategy(m_blurStrategy);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_freehandAction->setChecked(false);
    m_highlightAction->setChecked(false);
    m_blurAction->setChecked(true);

    // Deselect any layer and show blur tool properties
    m_layerManager->selectLayer(nullptr);
    m_layerProperties->setTool(m_blurTemplate, "Blur Tool");
}

void ImageEditor::addTextLayer(const QPoint &position, const QString &text)
{
    auto *textItem = new Tools::TextTool(text);
    textItem->setPos(position);

    // Copy properties from template tool
    auto *textTemplate = dynamic_cast<Tools::TextTool*>(m_textTemplate);
    if (textTemplate) {
        // Get all properties from template
        QList<Tools::ToolProperty> props = textTemplate->getProperties();
        for (const auto& prop : props) {
            textItem->setProperty(prop.id, prop.value);
        }
    } else {
        // Fallback to settings if template doesn't exist
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        textItem->setDefaultTextColor(foregroundColor);
    }

    // Create layer for text
    auto *layer = new Layer(QString("Text: %1").arg(text), Layer::Text, this);
    layer->setItem(textItem);

    connect(textItem, &Tools::TextTool::textChanged, [this, layer, textItem]() {
        QString newText = textItem->toPlainText();
        if (newText.length() > 20) {
            newText = newText.left(20) + "...";
        }
        layer->setName(QString("Text: %1").arg(newText));
        m_layerManager->updateLayerList();
    });

    m_scene->addItem(textItem);
    m_layerManager->addLayer(layer);
    m_layerManager->selectLayer(layer);
}

void ImageEditor::addArrowLayer(const QPoint &start, const QPoint &end)
{
    // Calculate arrow properties
    double dx = end.x() - start.x();
    double dy = end.y() - start.y();
    double length = std::sqrt(dx*dx + dy*dy);

    if (length < 10) return; // Too short to be meaningful

    // Create the new ArrowTool with interactive handles
    auto *arrowItem = new Tools::ArrowTool(start, end);

    // Copy properties from template tool
    auto *arrowTemplate = dynamic_cast<Tools::ArrowTool*>(m_arrowTemplate);
    if (arrowTemplate) {
        // Get all properties from template
        QList<Tools::ToolProperty> props = arrowTemplate->getProperties();
        for (const auto& prop : props) {
            arrowItem->setProperty(prop.id, prop.value);
        }
    } else {
        // Fallback to settings if template doesn't exist
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        QPen pen(foregroundColor, 3);
        arrowItem->setPen(pen);
        arrowItem->setArrowHeadType(Tools::ArrowTool::Outlined);
    }

    // Add the arrow item to the scene
    m_scene->addItem(arrowItem);

    // Create layer for arrow
    static int arrowCounter = 1;
    auto *layer = new Layer(QString("Arrow %1").arg(arrowCounter++), Layer::Arrow, this);
    layer->setItem(arrowItem);

    m_layerManager->addLayer(layer);
    m_layerManager->selectLayer(layer);

    // Don't auto-select the arrow - user should use pointer tool to select it
    m_scene->clearSelection();
}

void ImageEditor::addRectangleLayer(const QRect &rect)
{
    // Create the RectangleTool
    auto *rectangleItem = new Tools::RectangleTool(rect);

    // Copy properties from template tool
    auto *rectangleTemplate = dynamic_cast<Tools::RectangleTool*>(m_rectangleTemplate);
    if (rectangleTemplate) {
        // Get all properties from template
        QList<Tools::ToolProperty> props = rectangleTemplate->getProperties();
        for (const auto& prop : props) {
            rectangleItem->setProperty(prop.id, prop.value);
        }
    } else {
        // Fallback to settings if template doesn't exist
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        QColor backgroundColor = settings.value("Editor/BackgroundColor", QColor(Qt::transparent)).value<QColor>();
        QPen pen(foregroundColor, 2);
        rectangleItem->setPen(pen);
        rectangleItem->setBrush(QBrush(backgroundColor));
    }

    // Add the rectangle item to the scene
    m_scene->addItem(rectangleItem);

    // Create layer for rectangle
    static int rectangleCounter = 1;
    auto *layer = new Layer(QString("Rectangle %1").arg(rectangleCounter++), Layer::Rectangle, this);
    layer->setItem(rectangleItem);

    m_layerManager->addLayer(layer);
    m_layerManager->selectLayer(layer);

    // Don't auto-select the rectangle - user should use pointer tool to select it
    m_scene->clearSelection();
}

void ImageEditor::addEllipseLayer(const QRect &rect)
{
    // Create the EllipseTool
    auto *ellipseItem = new Tools::EllipseTool(rect);

    // Copy properties from template tool
    auto *ellipseTemplate = dynamic_cast<Tools::EllipseTool*>(m_ellipseTemplate);
    if (ellipseTemplate) {
        // Get all properties from template
        QList<Tools::ToolProperty> props = ellipseTemplate->getProperties();
        for (const auto& prop : props) {
            ellipseItem->setProperty(prop.id, prop.value);
        }
    } else {
        // Fallback to settings if template doesn't exist
        QSettings settings;
        QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
        QColor backgroundColor = settings.value("Editor/BackgroundColor", QColor(Qt::transparent)).value<QColor>();
        QPen pen(foregroundColor, 2);
        ellipseItem->setPen(pen);
        ellipseItem->setBrush(QBrush(backgroundColor));
    }

    // Add the ellipse item to the scene
    m_scene->addItem(ellipseItem);

    // Create layer for ellipse
    static int ellipseCounter = 1;
    auto *layer = new Layer(QString("Ellipse %1").arg(ellipseCounter++), Layer::Ellipse, this);
    layer->setItem(ellipseItem);

    m_layerManager->addLayer(layer);
    m_layerManager->selectLayer(layer);

    // Don't auto-select the ellipse - user should use pointer tool to select it
    m_scene->clearSelection();
}

void ImageEditor::addFreehandLayer(const QList<QPointF> &points)
{
    if (points.isEmpty()) {
        return;
    }

    // Create the FreehandTool and add all points
    auto *freehandItem = new Tools::FreehandTool();

    // Copy properties from template tool
    auto *freehandTemplate = dynamic_cast<Tools::FreehandTool*>(m_freehandTemplate);
    if (freehandTemplate) {
        // Get all properties from template
        QList<Tools::ToolProperty> props = freehandTemplate->getProperties();
        for (const auto& prop : props) {
            freehandItem->setProperty(prop.id, prop.value);
        }
    } else {
        // Fallback to previous layer or settings if template doesn't exist
        QPen pen;
        if (m_lastFreehandLayer && m_lastFreehandLayer->item()) {
            auto *lastFreehand = dynamic_cast<Tools::FreehandTool*>(m_lastFreehandLayer->item());
            if (lastFreehand) {
                pen = lastFreehand->pen();
            }
        } else {
            QSettings settings;
            QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
            pen = QPen(foregroundColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        }
        freehandItem->setPen(pen);
    }

    // Add all points to recreate the path
    for (const QPointF &point : points) {
        freehandItem->addPoint(point);
    }
    freehandItem->finishPath();

    // Add the freehand item to the scene
    m_scene->addItem(freehandItem);

    // Create layer for freehand
    static int freehandCounter = 1;
    auto *layer = new Layer(QString("Freehand %1").arg(freehandCounter++), Layer::Freehand, this);
    layer->setItem(freehandItem);

    // Remember this layer for next freehand drawing
    m_lastFreehandLayer = layer;

    m_layerManager->addLayer(layer);
    m_layerManager->selectLayer(layer);

    // Don't auto-select the freehand - user should use pointer tool to select it
    m_scene->clearSelection();
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
        m_backdropItem = new Tools::BackdropItem();
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
    // Close the backdrop popover on a click outside it — but tolerate clicks in a
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
    auto *grid = qobject_cast<QGridLayout *>(m_presetGrid->layout());
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
        QPixmap preview = Tools::BackdropItem::configPreview(preset.config, tile);
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
    auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(m_pixmapItem->graphicsEffect());
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

    // A whole config was applied (preset / default) — re-apply everything.
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
        m_backdropItem->update();   // color / gradient / wallpaper preset — just repaint
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
    if (!layer || layer->type() == Layer::Background) {
        return;
    }

    // If this is the last freehand layer, clear the reference
    if (layer == m_lastFreehandLayer) {
        m_lastFreehandLayer = nullptr;
    }

    // Remove the graphics item from the scene
    if (layer->item()) {
        m_scene->removeItem(layer->item());
        delete layer->item();
    }

    // Remove from layer manager and properties panel
    m_layerManager->removeLayer(layer);
    m_layerProperties->removeLayer(layer);

    // Delete the layer
    layer->deleteLater();
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
    return m_originalScreenshot.rect().contains(point);
}

QPoint ImageEditor::clampToImageBounds(const QPoint &point) const
{
    QRect bounds = m_originalScreenshot.rect();
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

QPen ImageEditor::getCurrentFreehandPen() const
{
    // If we have a previous freehand layer, use its pen settings
    if (m_lastFreehandLayer && m_lastFreehandLayer->item()) {
        auto *lastFreehand = dynamic_cast<Tools::FreehandTool*>(m_lastFreehandLayer->item());
        if (lastFreehand) {
            return lastFreehand->pen();
        }
    }

    // No previous freehand layer, return default pen
    QSettings settings;
    QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
    return QPen(foregroundColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

QColor ImageEditor::getCurrentHighlightColor() const
{
    // If we have a previous highlight layer, use its color
    if (m_lastHighlightLayer && m_lastHighlightLayer->item()) {
        auto *lastHighlight = dynamic_cast<Tools::HighlightTool*>(m_lastHighlightLayer->item());
        if (lastHighlight) {
            return lastHighlight->color();
        }
    }

    // No previous highlight layer, return default color
    return QColor("#b3ff61");
}

qreal ImageEditor::getCurrentHighlightWidth() const
{
    // If we have a previous highlight layer, use its width
    if (m_lastHighlightLayer && m_lastHighlightLayer->item()) {
        auto *lastHighlight = dynamic_cast<Tools::HighlightTool*>(m_lastHighlightLayer->item());
        if (lastHighlight) {
            return lastHighlight->width();
        }
    }

    // No previous highlight layer, return default medium width
    return Tools::HighlightTool::HIGHLIGHT_WIDTH_MEDIUM;
}

void ImageEditor::addHighlightLayer(const QList<QPointF> &points, const QColor &color, qreal width)
{
    if (points.isEmpty()) {
        return;
    }

    // Create the HighlightTool and add all points
    auto *highlightItem = new Tools::HighlightTool();
    highlightItem->setColor(color);
    highlightItem->setWidth(width);

    // Add all points to recreate the path
    for (const QPointF &point : points) {
        highlightItem->addPoint(point);
    }
    highlightItem->finishPath();

    // Add the highlight item to the scene
    m_scene->addItem(highlightItem);

    // Create layer for highlight
    static int highlightCounter = 1;
    auto *layer = new Layer(QString("Highlight %1").arg(highlightCounter++), Layer::Highlight, this);
    layer->setItem(highlightItem);

    // Remember this layer for next highlight drawing
    m_lastHighlightLayer = layer;

    m_layerManager->addLayer(layer);
    m_layerManager->selectLayer(layer);

    // Don't auto-select the highlight - user should use pointer tool to select it
    m_scene->clearSelection();
}

qreal ImageEditor::getCurrentBlurRadius() const
{
    // If we have a previous blur layer, use its blur radius
    if (m_lastBlurLayer && m_lastBlurLayer->item()) {
        auto *lastBlur = dynamic_cast<Tools::BlurTool*>(m_lastBlurLayer->item());
        if (lastBlur) {
            return lastBlur->blurRadius();
        }
    }

    // No previous blur layer, return default blur radius
    return 10.0;
}

qreal ImageEditor::getCurrentBlurBrushWidth() const
{
    // If we have a previous blur layer, use its brush width
    if (m_lastBlurLayer && m_lastBlurLayer->item()) {
        auto *lastBlur = dynamic_cast<Tools::BlurTool*>(m_lastBlurLayer->item());
        if (lastBlur) {
            return lastBlur->brushWidth();
        }
    }

    // No previous blur layer, return default brush width
    return 30.0;
}

void ImageEditor::addBlurLayer(const QList<QPointF> &points)
{
    if (points.isEmpty()) {
        return;
    }

    // Create the BlurTool and add all points
    auto *blurItem = new Tools::BlurTool();
    blurItem->setSourcePixmap(m_originalScreenshot);

    // Copy properties from template tool
    auto *blurTemplate = dynamic_cast<Tools::BlurTool*>(m_blurTemplate);
    if (blurTemplate) {
        // Get all properties from template
        QList<Tools::ToolProperty> props = blurTemplate->getProperties();
        for (const auto& prop : props) {
            blurItem->setProperty(prop.id, prop.value);
        }
    } else {
        // Fallback to previous layer or defaults if template doesn't exist
        if (m_lastBlurLayer && m_lastBlurLayer->item()) {
            auto *lastBlur = dynamic_cast<Tools::BlurTool*>(m_lastBlurLayer->item());
            if (lastBlur) {
                blurItem->setBlurRadius(lastBlur->blurRadius());
                blurItem->setBrushWidth(lastBlur->brushWidth());
            }
        } else {
            blurItem->setBlurRadius(10.0);
            blurItem->setBrushWidth(30.0);
        }
    }

    // Add all points to recreate the path
    for (const QPointF &point : points) {
        blurItem->addPoint(point);
    }
    blurItem->finishPath();

    // Add the blur item to the scene
    m_scene->addItem(blurItem);

    // Create layer for blur
    static int blurCounter = 1;
    auto *layer = new Layer(QString("Blur %1").arg(blurCounter++), Layer::Blur, this);
    layer->setItem(blurItem);

    // Remember this layer for next blur drawing
    m_lastBlurLayer = layer;

    m_layerManager->addLayer(layer);
    m_layerManager->selectLayer(layer);

    // Don't auto-select the blur - user should use pointer tool to select it
    m_scene->clearSelection();
}

} // namespace ImageEditor
