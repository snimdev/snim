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

    // Load stylesheet
    QFile styleFile(":/styles/styles/editor.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    resize(1400, 800);
    qDebug() << "ImageEditor constructor completed successfully";
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

    // Set checkerboard background (standard image editor pattern)
    QPixmap checkerboard(16, 16);
    QPainter cbPainter(&checkerboard);
    cbPainter.fillRect(0, 0, 16, 16, QColor(240, 240, 240));
    cbPainter.fillRect(0, 0, 8, 8, QColor(220, 220, 220));
    cbPainter.fillRect(8, 8, 8, 8, QColor(220, 220, 220));
    cbPainter.end();
    m_scene->setBackgroundBrush(QBrush(checkerboard));

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

    // Add widgets to right splitter
    m_rightSplitter->addWidget(m_layerManager);
    m_rightSplitter->addWidget(m_layerProperties);

    // Add main widgets to main splitter
    m_splitter->addWidget(m_view);
    m_splitter->addWidget(m_rightSplitter);

    // Set splitter proportions (75% for view, 25% for right panel)
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
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

// ...existing implementation methods...

void ImageEditor::onLayerVisibilityChanged(Layer *layer, bool visible)
{
    // Layer visibility is handled automatically by the Layer class
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
}

void ImageEditor::onLayerSelected(Layer *layer)
{
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
