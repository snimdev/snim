#include "screenshotapp.h"
#include <QScreen>
#include <QPainter>
#include <QTimer>
#include <QKeyEvent>
#include <cmath>

// DrawingGraphicsView implementation
DrawingGraphicsView::DrawingGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
    , m_currentTool(None)
    , m_drawing(false)
    , m_currentArrow(nullptr)
{
    setDragMode(QGraphicsView::NoDrag);
    setRenderHint(QPainter::Antialiasing);
}

void DrawingGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint scenePos = mapToScene(event->pos()).toPoint();

        // Check if the click is within image boundaries
        if (!isWithinImageBounds(scenePos)) {
            QGraphicsView::mousePressEvent(event);
            return;
        }

        if (m_currentTool == Pointer) {
            // Handle pointer tool - select items
            QGraphicsItem *item = scene()->itemAt(mapToScene(event->pos()), QTransform());
            if (item) {
                emit itemClicked(item);
            }
            QGraphicsView::mousePressEvent(event);
            return;
        } else if (m_currentTool == Arrow) {
            m_startPoint = scenePos;
            m_drawing = true;
            event->accept();
            return;
        } else if (m_currentTool == Text) {
            emit textRequested(scenePos);
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void DrawingGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drawing && m_currentTool == Arrow) {
        QPoint scenePos = mapToScene(event->pos()).toPoint();

        // Clamp the end point to image boundaries for arrows
        m_endPoint = clampToImageBounds(scenePos);

        // Remove previous temporary arrow
        if (m_currentArrow) {
            scene()->removeItem(m_currentArrow);
            delete m_currentArrow;
        }

        // Draw new arrow line with clamped coordinates
        QPen pen(Qt::red, 3);
        m_currentArrow = scene()->addLine(m_startPoint.x(), m_startPoint.y(),
                                         m_endPoint.x(), m_endPoint.y(), pen);
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void DrawingGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawing && m_currentTool == Arrow) {
        m_drawing = false;

        // Remove the temporary arrow from scene
        if (m_currentArrow) {
            scene()->removeItem(m_currentArrow);
            delete m_currentArrow;
            m_currentArrow = nullptr;

            // Emit signal to create a proper arrow layer
            emit arrowDrawn(m_startPoint, m_endPoint);
        }
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

bool DrawingGraphicsView::isWithinImageBounds(const QPoint &point) const
{
    return m_imageBounds.contains(point);
}

QPoint DrawingGraphicsView::clampToImageBounds(const QPoint &point) const
{
    // Clamp the point to stay within image boundaries
    int clampedX = qMax(m_imageBounds.left(), qMin(m_imageBounds.right(), point.x()));
    int clampedY = qMax(m_imageBounds.top(), qMin(m_imageBounds.bottom(), point.y()));

    return QPoint(clampedX, clampedY);
}

ScreenshotApp::ScreenshotApp(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
{
    setQuitOnLastWindowClosed(false);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, "Screenshot App",
                             "System tray is not available on this system.");
        return;
    }

    setupSystemTray();
}

ScreenshotApp::~ScreenshotApp()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

void ScreenshotApp::setupSystemTray()
{
    // Create actions
    m_captureAreaAction = new QAction("Capture Area", this);
    m_captureAreaAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    connect(m_captureAreaAction, &QAction::triggered, this, &ScreenshotApp::captureArea);

    m_captureWindowAction = new QAction("Capture Window", this);
    m_captureWindowAction->setShortcut(QKeySequence("Ctrl+Shift+W"));
    connect(m_captureWindowAction, &QAction::triggered, this, &ScreenshotApp::captureWindow);

    m_aboutAction = new QAction("About", this);
    connect(m_aboutAction, &QAction::triggered, this, &ScreenshotApp::showAbout);

    m_quitAction = new QAction("Quit", this);
    connect(m_quitAction, &QAction::triggered, this, &ScreenshotApp::quit);

    // Create menu
    m_trayMenu = new QMenu();
    m_trayMenu->addAction(m_captureAreaAction);
    m_trayMenu->addAction(m_captureWindowAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_aboutAction);
    m_trayMenu->addAction(m_quitAction);

    // Create system tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);

    // Set icon (you can replace this with a custom icon)
    QPixmap iconPixmap(16, 16);
    iconPixmap.fill(Qt::blue);
    QPainter painter(&iconPixmap);
    painter.setPen(Qt::white);
    painter.drawText(iconPixmap.rect(), Qt::AlignCenter, "📷");
    m_trayIcon->setIcon(QIcon(iconPixmap));

    m_trayIcon->setToolTip("Screenshot App");
    m_trayIcon->show();
}

void ScreenshotApp::captureArea()
{
    // Hide the tray icon temporarily
    m_trayIcon->hide();

    // Small delay to ensure menu is hidden
    QTimer::singleShot(100, [this]() {
        QPixmap screenshot = captureScreenArea();
        m_trayIcon->show();

        if (!screenshot.isNull()) {
            showScreenshotDialog(screenshot);
        }
    });
}

void ScreenshotApp::captureWindow()
{
    // Hide the tray icon temporarily
    m_trayIcon->hide();

    // Small delay to ensure menu is hidden
    QTimer::singleShot(100, [this]() {
        QPixmap screenshot = captureScreen();
        m_trayIcon->show();

        if (!screenshot.isNull()) {
            showScreenshotDialog(screenshot);
        }
    });
}

void ScreenshotApp::showAbout()
{
    QMessageBox::about(nullptr, "About Screenshot App",
                      "Screenshot App v1.0\n\nCapture screenshots and manage them easily.\n\n"
                      "Shortcuts:\n"
                      "Ctrl+Shift+A - Capture Area\n"
                      "Ctrl+Shift+W - Capture Window");
}

void ScreenshotApp::quit()
{
    QApplication::quit();
}

QPixmap ScreenshotApp::captureScreen()
{
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        return screen->grabWindow(0);
    }
    return QPixmap();
}

QPixmap ScreenshotApp::captureScreenArea()
{
    // First capture the entire screen
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) {
        return QPixmap();
    }

    QPixmap fullScreenshot = screen->grabWindow(0);
    if (fullScreenshot.isNull()) {
        QMessageBox::warning(nullptr, "Permission Required",
                           "Screen recording permission is required. Please grant permission in System Preferences > Security & Privacy > Privacy > Screen Recording");
        return QPixmap();
    }

    // Create area selector with the screenshot as background
    auto *selector = new AreaSelector();
    selector->setScreenshot(fullScreenshot);
    selector->showFullScreen();

    QPixmap result;
    QEventLoop loop;

    connect(selector, &AreaSelector::areaSelected, [&](const QRect &area) {
        selector->close();
        if (!area.isNull()) {
            // Crop the captured screenshot to the selected area
            result = fullScreenshot.copy(area);
        }
        loop.quit();
    });

    loop.exec();
    delete selector;

    return result;
}

void ScreenshotApp::showScreenshotDialog(const QPixmap &screenshot)
{
    auto *dialog = new ScreenshotDialog(screenshot);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

// AreaSelector implementation
AreaSelector::AreaSelector(QWidget *parent)
    : QWidget(parent)
    , m_selecting(false)
    , m_rubberBand(nullptr)
{
    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
}

void AreaSelector::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_selecting = true;
        m_rubberBand->setGeometry(QRect(m_startPoint, QSize()));
        m_rubberBand->show();
    }
}

void AreaSelector::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        m_endPoint = event->pos();
        m_rubberBand->setGeometry(QRect(m_startPoint, m_endPoint).normalized());
    }
}

void AreaSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        m_endPoint = event->pos();
        m_selectedArea = QRect(m_startPoint, m_endPoint).normalized();
        m_rubberBand->hide();

        emit areaSelected(m_selectedArea);
    }
}

void AreaSelector::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if (!m_screenshot.isNull()) {
        // Draw the screenshot as background
        painter.drawPixmap(rect(), m_screenshot, m_screenshot.rect());

        // Add a semi-transparent dark overlay
        painter.fillRect(rect(), QColor(0, 0, 0, 50));
    } else {
        // Fallback to dark background if no screenshot
        painter.fillRect(rect(), QColor(0, 0, 0, 100));
    }
}

void AreaSelector::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit areaSelected(QRect());
    }
}

// ScreenshotDialog implementation
ScreenshotDialog::ScreenshotDialog(const QPixmap &screenshot, QWidget *parent)
    : QDialog(parent)
    , m_screenshot(screenshot)
{
    setWindowTitle("Screenshot Captured");
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);

    // Preview label
    m_previewLabel = new QLabel();
    QPixmap preview = m_screenshot.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_previewLabel->setPixmap(preview);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("border: 1px solid gray;");
    mainLayout->addWidget(m_previewLabel);

    // Buttons
    auto *buttonLayout = new QHBoxLayout();

    m_copyButton = new QPushButton("Copy to Clipboard");
    m_copyButton->setDefault(true);
    connect(m_copyButton, &QPushButton::clicked, this, &ScreenshotDialog::copyToClipboard);

    m_editorButton = new QPushButton("Open in Editor");
    connect(m_editorButton, &QPushButton::clicked, this, &ScreenshotDialog::openInEditor);

    m_cancelButton = new QPushButton("Cancel");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    buttonLayout->addWidget(m_copyButton);
    buttonLayout->addWidget(m_editorButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(buttonLayout);

    resize(450, 400);
}

void ScreenshotDialog::copyToClipboard()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setPixmap(m_screenshot);
    accept();
}

void ScreenshotDialog::openInEditor()
{
    auto *editor = new ImageEditor(m_screenshot);
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->show();
    accept(); // Close the dialog
}

// ImageEditor implementation with layer support
ImageEditor::ImageEditor(const QPixmap &screenshot, QWidget *parent)
    : QMainWindow(parent)
    , m_originalScreenshot(screenshot)
    , m_currentTool(None)
    , m_drawing(false)
    , m_currentArrow(nullptr)
    , m_layerManager(nullptr)
    , m_layerProperties(nullptr)
    , m_splitter(nullptr)
    , m_rightSplitter(nullptr)
    , m_backgroundLayer(nullptr)
{
    setWindowTitle("Image Editor");
    setupUI();
    setupToolbar();

    // Create background layer and add to layer manager
    m_backgroundLayer = createBackgroundLayer();
    m_layerManager->addLayer(m_backgroundLayer);

    // Connect layer manager signals
    connect(m_layerManager, &LayerManager::layerVisibilityChanged,
            this, &ImageEditor::onLayerVisibilityChanged);
    connect(m_layerManager, &LayerManager::deleteLayerRequested,
            this, &ImageEditor::onDeleteLayerRequested);
    connect(m_layerManager, &LayerManager::layerSelected,
            this, &ImageEditor::onLayerSelected);

    resize(1200, 700);
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

    // Create layer manager and properties panel
    m_layerManager = new LayerManager();
    m_layerProperties = new LayerProperties();

    // Add widgets to right splitter
    m_rightSplitter->addWidget(m_layerManager);
    m_rightSplitter->addWidget(m_layerProperties);

    // Set equal proportions for layer manager and properties
    m_rightSplitter->setStretchFactor(0, 1);
    m_rightSplitter->setStretchFactor(1, 1);

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

    // Copy to clipboard action
    m_copyAction = new QAction(this);
    m_copyAction->setText("Copy");
    m_copyAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_copyAction->setShortcut(QKeySequence::Copy); // Ctrl+C
    connect(m_copyAction, &QAction::triggered, this, &ImageEditor::copyToClipboard);
    m_toolbar->addAction(m_copyAction);

    m_toolbar->addSeparator();

    // Pointer tool
    m_pointerAction = new QAction(this);
    m_pointerAction->setText("Pointer");
    m_pointerAction->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_pointerAction->setCheckable(true);
    m_pointerAction->setChecked(true); // Default tool
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

    // Set default tool to pointer
    selectPointerTool();
}

void ImageEditor::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Screenshot",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/screenshot.png",
        "PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)");

    if (!fileName.isEmpty()) {
        QPixmap result = renderScene();
        if (result.save(fileName)) {
            QMessageBox::information(this, "Saved", "Screenshot saved successfully!");
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
    QMessageBox::information(this, "Copied", "Screenshot copied to clipboard!");
}

void ImageEditor::selectPointerTool()
{
    m_currentTool = None;
    m_pointerAction->setChecked(true);
    m_arrowAction->setChecked(false);
    m_textAction->setChecked(false);
    m_view->setCursor(Qt::ArrowCursor);

    // Set the tool on the custom view
    m_view->setCurrentTool(DrawingGraphicsView::Pointer);
}

void ImageEditor::selectArrowTool()
{
    m_currentTool = Arrow;
    m_arrowAction->setChecked(true);
    m_pointerAction->setChecked(false);
    m_textAction->setChecked(false);
    m_view->setCursor(Qt::CrossCursor);

    // Set the tool on the custom view
    m_view->setCurrentTool(DrawingGraphicsView::Arrow);
}

void ImageEditor::selectTextTool()
{
    m_currentTool = Text;
    m_textAction->setChecked(true);
    m_pointerAction->setChecked(false);
    m_arrowAction->setChecked(false);
    m_view->setCursor(Qt::IBeamCursor);

    // Set the tool on the custom view
    m_view->setCurrentTool(DrawingGraphicsView::Text);
}

void ImageEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPoint scenePos = m_view->mapToScene(m_view->mapFromGlobal(event->globalPosition().toPoint())).toPoint();

        // Check if the click is within image boundaries
        if (!isWithinImageBounds(scenePos)) {
            return; // Ignore clicks outside image bounds
        }

        if (m_currentTool == Arrow) {
            m_startPoint = scenePos;
            m_drawing = true;
        } else if (m_currentTool == Text) {
            bool ok;
            QString text = QInputDialog::getText(this, "Add Text", "Enter text:", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                addTextLayer(scenePos, text);
            }
        }
    }
    QMainWindow::mousePressEvent(event);
}

void ImageEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drawing && m_currentTool == Arrow) {
        QPoint scenePos = m_view->mapToScene(m_view->mapFromGlobal(event->globalPosition().toPoint())).toPoint();

        // Clamp the end point to image boundaries for arrows
        m_endPoint = clampToImageBounds(scenePos);

        // Remove previous temporary arrow
        if (m_currentArrow) {
            m_scene->removeItem(m_currentArrow);
            delete m_currentArrow;
        }

        // Draw new arrow line with clamped coordinates
        QPen pen(Qt::red, 3);
        m_currentArrow = m_scene->addLine(m_startPoint.x(), m_startPoint.y(),
                                         m_endPoint.x(), m_endPoint.y(), pen);
    }
    QMainWindow::mouseMoveEvent(event);
}

void ImageEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawing && m_currentTool == Arrow) {
        m_drawing = false;

        // Create arrow layer instead of adding directly to scene
        if (m_currentArrow) {
            // Remove the temporary arrow from scene
            m_scene->removeItem(m_currentArrow);
            delete m_currentArrow;
            m_currentArrow = nullptr;

            // Create a proper arrow layer
            addArrowLayer(m_startPoint, m_endPoint);
        }
    }
    QMainWindow::mouseReleaseEvent(event);
}

void ImageEditor::addTextLayer(const QPoint &position, const QString &text)
{
    static int textLayerCount = 0;
    textLayerCount++;

    // Create editable text item
    auto *textItem = new EditableTextItem(text);
    textItem->setPos(position);

    // Add to scene
    m_scene->addItem(textItem);

    // Create layer
    QString layerName = QString("Text %1").arg(textLayerCount);
    auto *layer = new Layer(layerName, Layer::Text, this);
    layer->setItem(textItem);

    // Add to layer manager
    m_layerManager->addLayer(layer);

    // Connect text change signal to update layer name
    connect(textItem, &EditableTextItem::textChanged, [this, layer, textItem]() {
        QString newText = textItem->toPlainText();
        if (newText.length() > 20) {
            newText = newText.left(17) + "...";
        }
        layer->setName("Text: " + newText);
        m_layerManager->updateLayerList();
    });
}

void ImageEditor::addArrowLayer(const QPoint &start, const QPoint &end)
{
    static int arrowLayerCount = 0;
    arrowLayerCount++;

    // Create graphics item group for the arrow
    auto *arrowGroup = new QGraphicsItemGroup();

    // Create arrow line
    QPen pen(Qt::red, 3);
    auto *line = m_scene->addLine(start.x(), start.y(), end.x(), end.y(), pen);
    arrowGroup->addToGroup(line);

    // Add arrowhead
    QPoint arrowHead = end;
    QPoint arrowTail = start;

    // Calculate arrowhead points
    double angle = atan2((arrowHead.y() - arrowTail.y()), (arrowHead.x() - arrowTail.x()));
    double arrowLength = 15;
    double arrowAngle = M_PI / 6;

    QPoint arrowP1(arrowHead.x() - arrowLength * cos(angle - arrowAngle),
                  arrowHead.y() - arrowLength * sin(angle - arrowAngle));
    QPoint arrowP2(arrowHead.x() - arrowLength * cos(angle + arrowAngle),
                  arrowHead.y() - arrowLength * sin(angle + arrowAngle));

    auto *arrowLine1 = m_scene->addLine(arrowHead.x(), arrowHead.y(), arrowP1.x(), arrowP1.y(), pen);
    auto *arrowLine2 = m_scene->addLine(arrowHead.x(), arrowHead.y(), arrowP2.x(), arrowP2.y(), pen);

    arrowGroup->addToGroup(arrowLine1);
    arrowGroup->addToGroup(arrowLine2);

    // Make the arrow group movable and selectable
    arrowGroup->setFlag(QGraphicsItem::ItemIsMovable, true);
    arrowGroup->setFlag(QGraphicsItem::ItemIsSelectable, true);

    // Add group to scene
    m_scene->addItem(arrowGroup);

    // Create layer
    QString layerName = QString("Arrow %1").arg(arrowLayerCount);
    auto *layer = new Layer(layerName, Layer::Arrow, this);
    layer->setItem(arrowGroup);

    // Add to layer manager
    m_layerManager->addLayer(layer);

    // Automatically switch back to pointer tool after adding arrow
    selectPointerTool();
}

Layer* ImageEditor::createBackgroundLayer()
{
    auto *layer = new Layer("Background", Layer::Background, this);
    layer->setItem(m_pixmapItem);

    // Make sure background layer is not movable
    if (m_pixmapItem) {
        m_pixmapItem->setFlag(QGraphicsItem::ItemIsMovable, false);
        m_pixmapItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    }

    return layer;
}

bool ImageEditor::isWithinImageBounds(const QPoint &point) const
{
    // Get the image boundaries from the scene rectangle
    QRectF imageBounds = m_originalScreenshot.rect();
    return imageBounds.contains(point);
}

QPoint ImageEditor::clampToImageBounds(const QPoint &point) const
{
    // Get the image boundaries
    QRect imageBounds = m_originalScreenshot.rect();

    // Clamp the point to stay within image boundaries
    int clampedX = qMax(imageBounds.left(), qMin(imageBounds.right(), point.x()));
    int clampedY = qMax(imageBounds.top(), qMin(imageBounds.bottom(), point.y()));

    return QPoint(clampedX, clampedY);
}

void ImageEditor::onItemClicked(QGraphicsItem *item)
{
    // Find the layer that corresponds to this graphics item
    selectLayerByItem(item);
}

void ImageEditor::selectLayerByItem(QGraphicsItem *item)
{
    if (!item) return;

    // Find the layer that owns this item
    for (Layer *layer : m_layerManager->layers()) {
        if (layer->item() == item) {
            // Select this layer in the layer manager
            m_layerManager->selectLayer(layer);
            return;
        }

        // Check if the item is a child of a group (for arrows)
        if (auto *group = qgraphicsitem_cast<QGraphicsItemGroup*>(layer->item())) {
            if (group->childItems().contains(item)) {
                m_layerManager->selectLayer(layer);
                return;
            }
        }
    }
}

QPixmap ImageEditor::renderScene()
{
    // Create a pixmap to render the scene
    QPixmap pixmap(m_scene->sceneRect().size().toSize());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    m_scene->render(&painter);

    return pixmap;
}

void ImageEditor::onDeleteLayerRequested(Layer *layer)
{
    if (layer && layer->type() != Layer::Background) {
        // Remove the graphics item from scene
        if (layer->item()) {
            m_scene->removeItem(layer->item());
            delete layer->item();
        }

        // Remove layer from manager
        m_layerManager->removeLayer(layer);

        // Delete the layer object
        layer->deleteLater();
    }
}

void ImageEditor::onLayerVisibilityChanged(Layer *layer, bool visible)
{
    // Visibility is already handled in the Layer class
    // This slot can be used for additional actions if needed
}

void ImageEditor::onLayerSelected(Layer *layer)
{
    // Update the properties panel for the selected layer
    m_layerProperties->setLayer(layer);
}

// EditableTextItem implementation
EditableTextItem::EditableTextItem(const QString &text, QGraphicsItem *parent)
    : QGraphicsTextItem(text, parent)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setTextInteractionFlags(Qt::NoTextInteraction);
}

void EditableTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFocus();
    QGraphicsTextItem::mouseDoubleClickEvent(event);
}

void EditableTextItem::focusOutEvent(QFocusEvent *event)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    emit textChanged();
    QGraphicsTextItem::focusOutEvent(event);
}

void EditableTextItem::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier && isSelected()) {
        QFont currentFont = font();
        qreal currentSize = currentFont.pointSizeF();

        if (currentSize <= 0) {
            currentSize = currentFont.pixelSize();
            if (currentSize <= 0) {
                currentSize = 12;
            }
        }

        qreal sizeChange = event->delta() / 120.0;
        qreal newSize = currentSize + sizeChange;
        newSize = qMax(6.0, qMin(72.0, newSize));

        currentFont.setPointSizeF(newSize);
        setFont(currentFont);
        emit textChanged();
        event->accept();
    } else {
        QGraphicsTextItem::wheelEvent(event);
    }
}

// Layer implementation
Layer::Layer(const QString &name, LayerType type, QObject *parent)
    : QObject(parent)
    , m_name(name)
    , m_visible(true)
    , m_type(type)
    , m_item(nullptr)
{
}

void Layer::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        if (m_item) {
            m_item->setVisible(visible);
        }
        emit visibilityChanged(visible);
    }
}

void Layer::setItem(QGraphicsItem *item)
{
    m_item = item;
    if (m_item) {
        m_item->setVisible(m_visible);
    }
}

// LayerManager implementation
LayerManager::LayerManager(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(200);
    setWindowTitle("Layers");

    auto *layout = new QVBoxLayout(this);

    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setMaximumHeight(300);

    m_layerList = new QListWidget();
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    connect(m_layerList, &QListWidget::itemSelectionChanged,
            this, &LayerManager::onItemSelectionChanged);

    scrollArea->setWidget(m_layerList);
    layout->addWidget(scrollArea);

    m_deleteButton = new QPushButton("Delete Layer");
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked,
            this, &LayerManager::onDeleteButtonClicked);
    layout->addWidget(m_deleteButton);
}

void LayerManager::addLayer(Layer *layer)
{
    m_layers.append(layer);
    updateLayerList();

    connect(layer, &Layer::visibilityChanged, [this, layer](bool visible) {
        emit layerVisibilityChanged(layer, visible);
    });
}

void LayerManager::removeLayer(Layer *layer)
{
    m_layers.removeAll(layer);
    updateLayerList();
}

void LayerManager::removeSelectedLayer()
{
    Layer *selected = selectedLayer();
    if (selected && selected->type() != Layer::Background) {
        emit deleteLayerRequested(selected);
    }
}

Layer* LayerManager::selectedLayer() const
{
    int currentRow = m_layerList->currentRow();
    if (currentRow >= 0 && currentRow < m_layers.size()) {
        return m_layers[m_layers.size() - 1 - currentRow];
    }
    return nullptr;
}

void LayerManager::selectLayer(Layer *layer)
{
    if (!layer) return;

    int layerIndex = m_layers.indexOf(layer);
    if (layerIndex >= 0) {
        int listIndex = m_layers.size() - 1 - layerIndex;
        m_layerList->setCurrentRow(listIndex);
    }
}

void LayerManager::onItemSelectionChanged()
{
    Layer *selected = selectedLayer();
    m_deleteButton->setEnabled(selected && selected->type() != Layer::Background);
    if (selected) {
        emit layerSelected(selected);
    }
}

void LayerManager::onDeleteButtonClicked()
{
    int currentRow = m_layerList->currentRow();
    Layer *selected = selectedLayer();

    if (selected && selected->type() != Layer::Background) {
        emit deleteLayerRequested(selected);

        int newRow = currentRow;
        if (newRow >= m_layerList->count() - 1) {
            newRow = m_layerList->count() - 2;
        }

        if (newRow >= 0 && newRow < m_layerList->count()) {
            QTimer::singleShot(50, [this, newRow]() {
                if (newRow < m_layerList->count()) {
                    m_layerList->setCurrentRow(newRow);
                }
            });
        }
    }
}

void LayerManager::updateLayerList()
{
    m_layerList->clear();

    for (int i = m_layers.size() - 1; i >= 0; --i) {
        Layer *layer = m_layers[i];
        auto *item = new QListWidgetItem();

        auto *widget = new QWidget();
        auto *layout = new QHBoxLayout(widget);
        layout->setContentsMargins(2, 2, 2, 2);

        auto *checkbox = new QCheckBox();
        checkbox->setChecked(layer->isVisible());
        connect(checkbox, &QCheckBox::toggled, [layer](bool checked) {
            layer->setVisible(checked);
        });

        QString displayName = layer->name();

        if (displayName.startsWith("Text: ")) {
            QString textContent = displayName.mid(6);
            if (textContent.length() > 8) {
                displayName = "Text: " + textContent.left(8) + "...";
            }
        }
        else if (displayName.length() > 8 &&
                 !displayName.startsWith("Background") &&
                 !displayName.startsWith("Arrow ")) {
            displayName = displayName.left(8) + "...";
        }

        auto *label = new QLabel(displayName);

        layout->addWidget(checkbox);
        layout->addWidget(label);
        layout->addStretch();

        item->setSizeHint(widget->sizeHint());
        m_layerList->addItem(item);
        m_layerList->setItemWidget(item, widget);
    }
}

// LayerProperties implementation
LayerProperties::LayerProperties(QWidget *parent)
    : QWidget(parent)
    , m_currentLayer(nullptr)
    , m_backgroundGroup(nullptr)
    , m_backgroundColorButton(nullptr)
    , m_textGroup(nullptr)
    , m_textColorButton(nullptr)
    , m_arrowGroup(nullptr)
    , m_arrowColorButton(nullptr)
{
    setFixedWidth(200);
    setWindowTitle("Layer Properties");
    setupUI();
}

void LayerProperties::setupUI()
{
    auto *layout = new QVBoxLayout(this);

    m_backgroundGroup = new QGroupBox("Background Properties");
    auto *backgroundLayout = new QVBoxLayout(m_backgroundGroup);

    auto *bgColorLayout = new QHBoxLayout();
    bgColorLayout->addWidget(new QLabel("Background Color:"));
    m_backgroundColorButton = createColorButton(QColor(255, 255, 255));
    connect(m_backgroundColorButton, &QPushButton::clicked,
            this, &LayerProperties::onBackgroundColorButtonClicked);
    bgColorLayout->addWidget(m_backgroundColorButton);
    backgroundLayout->addLayout(bgColorLayout);

    layout->addWidget(m_backgroundGroup);

    m_textGroup = new QGroupBox("Text Properties");
    auto *textLayout = new QVBoxLayout(m_textGroup);

    auto *textColorLayout = new QHBoxLayout();
    textColorLayout->addWidget(new QLabel("Text Color:"));
    m_textColorButton = createColorButton(QColor(0, 0, 0));
    connect(m_textColorButton, &QPushButton::clicked,
            this, &LayerProperties::onTextColorButtonClicked);
    textColorLayout->addWidget(m_textColorButton);
    textLayout->addLayout(textColorLayout);

    layout->addWidget(m_textGroup);

    m_arrowGroup = new QGroupBox("Arrow Properties");
    auto *arrowLayout = new QVBoxLayout(m_arrowGroup);

    auto *arrowColorLayout = new QHBoxLayout();
    arrowColorLayout->addWidget(new QLabel("Arrow Color:"));
    m_arrowColorButton = createColorButton(QColor(255, 0, 0));
    connect(m_arrowColorButton, &QPushButton::clicked,
            this, &LayerProperties::onArrowColorButtonClicked);
    arrowColorLayout->addWidget(m_arrowColorButton);
    arrowLayout->addLayout(arrowColorLayout);

    layout->addWidget(m_arrowGroup);

    layout->addStretch();

    m_backgroundGroup->hide();
    m_textGroup->hide();
    m_arrowGroup->hide();
}

QPushButton* LayerProperties::createColorButton(const QColor &color)
{
    auto *button = new QPushButton();
    button->setFixedSize(30, 20);
    button->setStyleSheet(QString("background-color: %1; border: 1px solid black;").arg(color.name()));
    return button;
}

void LayerProperties::setLayer(Layer *layer)
{
    m_currentLayer = layer;
    updatePropertiesForLayer();
}

void LayerProperties::updatePropertiesForLayer()
{
    m_backgroundGroup->hide();
    m_textGroup->hide();
    m_arrowGroup->hide();

    if (!m_currentLayer) {
        return;
    }

    switch (m_currentLayer->type()) {
        case Layer::Background:
            m_backgroundGroup->show();
            break;
        case Layer::Text:
            m_textGroup->show();
            break;
        case Layer::Arrow:
            m_arrowGroup->show();
            break;
    }
}

void LayerProperties::onBackgroundColorButtonClicked()
{
    if (!m_currentLayer || m_currentLayer->type() != Layer::Background) {
        return;
    }

    QColor color = QColorDialog::getColor(Qt::white, this, "Select Background Color");
    if (color.isValid()) {
        m_backgroundColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid black;").arg(color.name()));

        if (m_currentLayer->item()) {
            auto *pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(m_currentLayer->item());
            if (pixmapItem) {
                QPixmap originalPixmap = pixmapItem->pixmap();
                QPixmap coloredPixmap(originalPixmap.size());
                coloredPixmap.fill(color);

                QPainter painter(&coloredPixmap);
                painter.drawPixmap(0, 0, originalPixmap);
                painter.end();

                pixmapItem->setPixmap(coloredPixmap);
            }
        }
    }
}

void LayerProperties::onTextColorButtonClicked()
{
    if (!m_currentLayer || m_currentLayer->type() != Layer::Text) {
        return;
    }

    QColor color = QColorDialog::getColor(Qt::black, this, "Select Text Color");
    if (color.isValid()) {
        m_textColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid black;").arg(color.name()));

        if (m_currentLayer->item()) {
            auto *textItem = qgraphicsitem_cast<QGraphicsTextItem*>(m_currentLayer->item());
            if (textItem) {
                textItem->setDefaultTextColor(color);
            }
        }
    }
}

void LayerProperties::onArrowColorButtonClicked()
{
    if (!m_currentLayer || m_currentLayer->type() != Layer::Arrow) {
        return;
    }

    QColor color = QColorDialog::getColor(Qt::red, this, "Select Arrow Color");
    if (color.isValid()) {
        m_arrowColorButton->setStyleSheet(
            QString("background-color: %1; border: 1px solid black;").arg(color.name()));

        if (m_currentLayer->item()) {
            auto *groupItem = qgraphicsitem_cast<QGraphicsItemGroup*>(m_currentLayer->item());
            if (groupItem) {
                QPen pen(color, 3);
                for (auto *child : groupItem->childItems()) {
                    auto *lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(child);
                    if (lineItem) {
                        lineItem->setPen(pen);
                    }
                }
            }
        }
    }
}

#include "screenshotapp.moc"
