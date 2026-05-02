#ifndef APP_TRAYMENU_H
#define APP_TRAYMENU_H

#include "app/Notifier.h"
#include "hotkeys/HotkeyAction.h"

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;

namespace App {

/**
 * The tray view: icon, menu and its actions, availability gating and shortcut hints.
 * It only raises the actions; the composition root (Application) wires them to the
 * workflows, and the workflows report back through the Notifier interface.
 */
class TrayMenu : public QObject, public Notifier
{
    Q_OBJECT

public:
    explicit TrayMenu(QObject *parent = nullptr);
    ~TrayMenu() override;

    // Creates and shows the icon; call once the actions are wired and gated.
    void show();
    void hide();

    void notify(const QString &title, const QString &text,
                QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                int msecs = 10000) override;

    // Greys out both recording actions with the platform's reason (e.g. macOS < 12.3).
    void setRecordingUnavailable();
    // Record Area doubles as Stop; Record Window waits, one session at a time.
    void setRecordingActive(bool recording);

    // The tray action a hotkey fires; every hotkey action has one.
    [[nodiscard]] QAction *actionFor(Hotkeys::HotkeyAction action) const;

    // Menu hint text only: a tray menu never dispatches a QAction shortcut.
    void refreshShortcutHints();

    [[nodiscard]] QMenu *menu() const { return m_menu; }
    [[nodiscard]] QAction *captureAreaAction() const { return m_captureAreaAction; }
    [[nodiscard]] QAction *captureWindowAction() const { return m_captureWindowAction; }
    [[nodiscard]] QAction *captureFullScreenAction() const { return m_captureFullScreenAction; }
    [[nodiscard]] QAction *textSnipAction() const { return m_textSnipAction; }
    [[nodiscard]] QAction *recordAreaAction() const { return m_recordAreaAction; }
    [[nodiscard]] QAction *recordWindowAction() const { return m_recordWindowAction; }
    [[nodiscard]] QAction *settingsAction() const { return m_settingsAction; }
    [[nodiscard]] QAction *aboutAction() const { return m_aboutAction; }
    [[nodiscard]] QAction *checkUpdatesAction() const { return m_checkUpdatesAction; }
    // Linux only; nullptr elsewhere.
    [[nodiscard]] QAction *desktopIntegrationAction() const { return m_desktopIntegrationAction; }
    [[nodiscard]] QAction *quitAction() const { return m_quitAction; }

signals:
    void doubleClicked();

private:
    static QIcon createThemedTrayIcon(const QString &iconPath);

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;   // parentless: a widget cannot have a QObject parent
    QAction *m_captureAreaAction{};
    QAction *m_captureWindowAction{};
    QAction *m_captureFullScreenAction{};
    QAction *m_textSnipAction{};
    QAction *m_recordAreaAction{};
    QAction *m_recordWindowAction{};
    QAction *m_settingsAction{};
    QAction *m_aboutAction{};
    QAction *m_checkUpdatesAction{};
    QAction *m_desktopIntegrationAction{};   // hidden once the entry is in place
    QAction *m_quitAction{};
};

} // namespace App

#endif // APP_TRAYMENU_H
