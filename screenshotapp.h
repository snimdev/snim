#ifndef SCREENSHOTAPP_H
#define SCREENSHOTAPP_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

// Include the separated class headers
#include "DrawingGraphicsView.h"
#include "EditableTextItem.h"
#include "Layer.h"
#include "LayerManager.h"
#include "LayerProperties.h"
#include "AreaSelector.h"
#include "ScreenshotDialog.h"
#include "ImageEditor.h"

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


#endif // SCREENSHOTAPP_H
