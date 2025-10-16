#ifndef CORE_SCREENSHOTAPP_H
#define CORE_SCREENSHOTAPP_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <memory>

namespace Core {

class ScreenshotDialog;
class SettingsDialog;
class TextSnipCapture;

} // namespace Core

namespace Capture {
class CaptureStrategy;
} // namespace Capture

namespace Core {

class ScreenshotApp : public QApplication
{
    Q_OBJECT

public:
    explicit ScreenshotApp(int &argc, char **argv);
    ~ScreenshotApp() override;

private slots:
    void captureArea() const;
    void captureWindow() const;
    void captureTextSnip();
    void onScreenshotReady(const QPixmap &screenshot);
    void onTextExtracted(const QString &text, bool success);
    static void showSettings();
    static void showAbout();
    static void quit();

private:
    void setupSystemTray();
    static QIcon createThemedTrayIcon(const QString &iconPath);

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_captureAreaAction{};
    QAction *m_captureWindowAction{};
    QAction *m_textSnipAction{};
    QAction *m_settingsAction{};
    QAction *m_aboutAction{};
    QAction *m_quitAction{};

    std::unique_ptr<Capture::CaptureStrategy> m_captureStrategy;
    std::unique_ptr<TextSnipCapture> m_textSnipCapture;
};

} // namespace Core

#endif // CORE_SCREENSHOTAPP_H
