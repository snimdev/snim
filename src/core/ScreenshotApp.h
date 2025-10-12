#ifndef CORE_SCREENSHOTAPP_H
#define CORE_SCREENSHOTAPP_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

namespace Core {

class ScreenshotDialog;
class SettingsDialog;

} // namespace Core

namespace Capture {
class AreaSelector;
} // namespace Capture

namespace Core {

class ScreenshotApp : public QApplication
{
    Q_OBJECT

public:
    explicit ScreenshotApp(int &argc, char **argv);
    ~ScreenshotApp();

private slots:
    void captureArea();
    void captureWindow();
    void showSettings();
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
    QAction *m_settingsAction;
    QAction *m_aboutAction;
    QAction *m_quitAction;
};

} // namespace Core

#endif // CORE_SCREENSHOTAPP_H
