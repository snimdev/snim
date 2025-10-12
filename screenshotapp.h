#ifndef SCREENSHOTAPP_H
#define SCREENSHOTAPP_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QScreen>
#include <QPixmap>
#include <QClipboard>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QRubberBand>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QToolBar>
#include <QMainWindow>
#include <QScrollArea>
#include <QPainter>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QFontDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QGraphicsLineItem>
#include <QGraphicsSceneWheelEvent>
#include <QStyle>
#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QGridLayout>
#include <QScrollArea>

// Forward declarations
class Layer;
class LayerManager;
class EditableTextItem;
class LayerProperties;
class DrawingGraphicsView;

// Custom graphics view for drawing
class DrawingGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    enum Tool {
        None,
        Pointer,
        Arrow,
        Text
    };

    explicit DrawingGraphicsView(QWidget *parent = nullptr);

    void setCurrentTool(Tool tool) { m_currentTool = tool; }
    void setImageBounds(const QRect &bounds) { m_imageBounds = bounds; }

signals:
    void arrowDrawn(const QPoint &start, const QPoint &end);
    void textRequested(const QPoint &position);
    void itemClicked(QGraphicsItem *item);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool isWithinImageBounds(const QPoint &point) const;
    QPoint clampToImageBounds(const QPoint &point) const;

    Tool m_currentTool;
    bool m_drawing;
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_imageBounds;
    QGraphicsLineItem *m_currentArrow;
};

// Custom editable text item
class EditableTextItem : public QGraphicsTextItem
{
    Q_OBJECT

public:
    explicit EditableTextItem(const QString &text = "", QGraphicsItem *parent = nullptr);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;

signals:
    void textChanged();
};

// Layer class to represent individual layers
class Layer : public QObject
{
    Q_OBJECT

public:
    enum LayerType {
        Background,
        Arrow,
        Text
    };

    explicit Layer(const QString &name, LayerType type, QObject *parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);

    LayerType type() const { return m_type; }

    QGraphicsItem* item() const { return m_item; }
    void setItem(QGraphicsItem *item);

signals:
    void visibilityChanged(bool visible);
    void nameChanged(const QString &name);

private:
    QString m_name;
    bool m_visible;
    LayerType m_type;
    QGraphicsItem *m_item;
};

// Layer manager widget
class LayerManager : public QWidget
{
    Q_OBJECT

public:
    explicit LayerManager(QWidget *parent = nullptr);

    void updateLayerList();
    void addLayer(Layer *layer);
    void removeLayer(Layer *layer);
    void removeSelectedLayer();
    Layer* selectedLayer() const;
    QList<Layer*> layers() const { return m_layers; }
    void selectLayer(Layer *layer);

signals:
    void layerSelected(Layer *layer);
    void layerVisibilityChanged(Layer *layer, bool visible);
    void deleteLayerRequested(Layer *layer);

private slots:
    void onItemSelectionChanged();
    void onDeleteButtonClicked();

private:
    QListWidget *m_layerList;
    QPushButton *m_deleteButton;
    QList<Layer*> m_layers;
};

// Layer properties panel widget
class LayerProperties : public QWidget
{
    Q_OBJECT

public:
    explicit LayerProperties(QWidget *parent = nullptr);
    void setLayer(Layer *layer);

private slots:
    void onBackgroundColorButtonClicked();
    void onTextColorButtonClicked();
    void onArrowColorButtonClicked();

private:
    void setupUI();
    void updatePropertiesForLayer();
    QPushButton* createColorButton(const QColor &color);

    Layer *m_currentLayer;

    // UI elements
    QGroupBox *m_backgroundGroup;
    QPushButton *m_backgroundColorButton;

    QGroupBox *m_textGroup;
    QPushButton *m_textColorButton;

    QGroupBox *m_arrowGroup;
    QPushButton *m_arrowColorButton;
};

class ScreenshotApp : public QApplication
{
    Q_OBJECT

public:
    explicit ScreenshotApp(int &argc, char **argv);
    ~ScreenshotApp();

private slots:
    void captureArea();
    void captureWindow();
    void showAbout();
    void quit();

private:
    void setupSystemTray();
    void showScreenshotDialog(const QPixmap &screenshot);
    QPixmap captureScreen();
    QPixmap captureScreenArea();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_captureAreaAction;
    QAction *m_captureWindowAction;
    QAction *m_aboutAction;
    QAction *m_quitAction;
};

class AreaSelector : public QWidget
{
    Q_OBJECT

public:
    explicit AreaSelector(QWidget *parent = nullptr);
    QRect selectedArea() const { return m_selectedArea; }
    void setScreenshot(const QPixmap &screenshot) { m_screenshot = screenshot; }

signals:
    void areaSelected(const QRect &area);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPoint m_startPoint;
    QPoint m_endPoint;
    QRect m_selectedArea;
    bool m_selecting;
    QRubberBand *m_rubberBand;
    QPixmap m_screenshot;
};

class ScreenshotDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScreenshotDialog(const QPixmap &screenshot, QWidget *parent = nullptr);

private slots:
    void copyToClipboard();
    void openInEditor();

private:
    QPixmap m_screenshot;
    QLabel *m_previewLabel;
    QPushButton *m_copyButton;
    QPushButton *m_editorButton;
    QPushButton *m_cancelButton;
};

class ImageEditor : public QMainWindow
{
    Q_OBJECT

public:
    explicit ImageEditor(const QPixmap &screenshot, QWidget *parent = nullptr);

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
    DrawingGraphicsView *m_view;  // Changed from QGraphicsView to DrawingGraphicsView
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

#endif // SCREENSHOTAPP_H
