#include "ImageEditor.h"
#include "DrawingGraphicsView.h"
#include "LayerManager.h"
#include "LayerProperties.h"
#include "Layer.h"
#include "tools/TextTool.h"
#include "tools/ArrowTool.h"
#include "tools/RectangleTool.h"
#include "tools/EllipseTool.h"
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
    , m_splitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_layerManager(nullptr)
    , m_layerProperties(nullptr)
    , m_originalScreenshot(screenshot)
    , m_currentTool(None)
    , m_drawing(false)
    , m_currentArrow(nullptr)
    , m_backgroundLayer(nullptr)
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

    resize(1200, 700);
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

    // Add the screenshot to the scene
    m_pixmapItem = m_scene->addPixmap(m_originalScreenshot);
    m_scene->setSceneRect(m_originalScreenshot.rect());

    // Set image bounds for the view
    m_view->setImageBounds(m_originalScreenshot.rect());

    // Connect signals from the custom view
    connect(m_view, &DrawingGraphicsView::arrowDrawn, this, &ImageEditor::addArrowLayer);
    connect(m_view, &DrawingGraphicsView::rectangleDrawn, this, &ImageEditor::addRectangleLayer);
    connect(m_view, &DrawingGraphicsView::ellipseDrawn, this, &ImageEditor::addEllipseLayer);
    connect(m_view, &DrawingGraphicsView::textRequested, this, [this](const QPoint &position) {
        bool ok;
        QString text = QInputDialog::getText(this, "Add Text", "Enter text:", QLineEdit::Normal, "", &ok);
        if (ok && !text.isEmpty()) {
            addTextLayer(position, text);
            // Automatically switch back to pointer tool after adding text
            selectPointerTool();
        }
    });
    connect(m_view, &DrawingGraphicsView::itemClicked, this, &ImageEditor::onItemClicked);

    // Configure view
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setRenderHint(QPainter::Antialiasing);

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

void ImageEditor::setupToolbar()
{
    m_toolbar = addToolBar("Tools");
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Save As action
    m_saveAsAction = new QAction(this);
    m_saveAsAction->setText("Save As");
    m_saveAsAction->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveAsAction->setShortcut(QKeySequence::Save); // Ctrl+S
    connect(m_saveAsAction, &QAction::triggered, this, &ImageEditor::saveAs);
    m_toolbar->addAction(m_saveAsAction);

    // Copy to Clipboard action
    m_copyAction = new QAction(this);
    m_copyAction->setText("Copy");
    m_copyAction->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    m_copyAction->setShortcut(QKeySequence::Copy); // Ctrl+C
    connect(m_copyAction, &QAction::triggered, this, &ImageEditor::copyToClipboard);
    m_toolbar->addAction(m_copyAction);

    m_toolbar->addSeparator();

    // Pointer tool
    m_pointerAction = new QAction(this);
    m_pointerAction->setText("Pointer");
    m_pointerAction->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_pointerAction->setCheckable(true);
    m_pointerAction->setChecked(true);
    connect(m_pointerAction, &QAction::triggered, this, &ImageEditor::selectPointerTool);
    m_toolbar->addAction(m_pointerAction);

    // Arrow tool
    m_arrowAction = new QAction(this);
    m_arrowAction->setText("Arrow");
    m_arrowAction->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    m_arrowAction->setCheckable(true);
    connect(m_arrowAction, &QAction::triggered, this, &ImageEditor::selectArrowTool);
    m_toolbar->addAction(m_arrowAction);

    // Text tool
    m_textAction = new QAction(this);
    m_textAction->setText("Text");
    m_textAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_textAction->setCheckable(true);
    connect(m_textAction, &QAction::triggered, this, &ImageEditor::selectTextTool);
    m_toolbar->addAction(m_textAction);

    // Rectangle tool
    m_rectangleAction = new QAction(this);
    m_rectangleAction->setText("Rectangle");
    m_rectangleAction->setIcon(style()->standardIcon(QStyle::SP_DialogNoButton));
    m_rectangleAction->setCheckable(true);
    connect(m_rectangleAction, &QAction::triggered, this, &ImageEditor::selectRectangleTool);
    m_toolbar->addAction(m_rectangleAction);

    // Ellipse tool
    m_ellipseAction = new QAction(this);
    m_ellipseAction->setText("Ellipse");
    m_ellipseAction->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    m_ellipseAction->setCheckable(true);
    connect(m_ellipseAction, &QAction::triggered, this, &ImageEditor::selectEllipseTool);
    m_toolbar->addAction(m_ellipseAction);
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
    m_currentTool = None;
    m_view->setCurrentTool(DrawingGraphicsView::ToolType::Pointer);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_pointerAction->setChecked(true);
    m_layerProperties->setLayer(nullptr);
}

void ImageEditor::selectArrowTool()
{
    m_currentTool = Arrow;
    m_view->setCurrentTool(DrawingGraphicsView::ToolType::Arrow);
    m_pointerAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_arrowAction->setChecked(true);
    m_layerProperties->setLayer(nullptr);
}

void ImageEditor::selectTextTool()
{
    m_currentTool = Text;
    m_view->setCurrentTool(DrawingGraphicsView::ToolType::Text);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_textAction->setChecked(true);
    m_layerProperties->setLayer(nullptr);
}

void ImageEditor::selectRectangleTool()
{
    m_currentTool = Rectangle;
    m_view->setCurrentTool(DrawingGraphicsView::ToolType::Rectangle);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_ellipseAction->setChecked(false);
    m_rectangleAction->setChecked(true);
    m_layerProperties->setLayer(nullptr);
}

void ImageEditor::selectEllipseTool()
{
    m_currentTool = Ellipse;
    m_view->setCurrentTool(DrawingGraphicsView::ToolType::Ellipse);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_rectangleAction->setChecked(false);
    m_ellipseAction->setChecked(true);
    m_layerProperties->setLayer(nullptr);
}

void ImageEditor::addTextLayer(const QPoint &position, const QString &text)
{
    auto *textItem = new Tools::TextTool(text);
    textItem->setPos(position);

    // Get foreground color from settings (text uses foreground color)
    QSettings settings;
    QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
    textItem->setDefaultTextColor(foregroundColor);

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

    // Get foreground color from settings (arrows use foreground for stroke)
    QSettings settings;
    QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();

    // Set default appearance
    QPen pen(foregroundColor, 3);
    arrowItem->setPen(pen);
    arrowItem->setArrowHeadType(Tools::ArrowTool::Outlined);

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

    // Get foreground and background colors from settings
    QSettings settings;
    QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
    QColor backgroundColor = settings.value("Editor/BackgroundColor", QColor(Qt::transparent)).value<QColor>();

    // Set default appearance (foreground = stroke, background = fill)
    QPen pen(foregroundColor, 2);
    rectangleItem->setPen(pen);
    rectangleItem->setBrush(QBrush(backgroundColor));

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

    // Get foreground and background colors from settings
    QSettings settings;
    QColor foregroundColor = settings.value("Editor/ForegroundColor", QColor(Qt::red)).value<QColor>();
    QColor backgroundColor = settings.value("Editor/BackgroundColor", QColor(Qt::transparent)).value<QColor>();

    // Set default appearance (foreground = stroke, background = fill)
    QPen pen(foregroundColor, 2);
    ellipseItem->setPen(pen);
    ellipseItem->setBrush(QBrush(backgroundColor));

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

} // namespace ImageEditor
