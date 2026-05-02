#ifndef APP_NOTIFIER_H
#define APP_NOTIFIER_H

#include <QString>
#include <QSystemTrayIcon>

namespace App {

/**
 * Balloon-style notifications, so a workflow can report without owning a tray icon.
 * TrayMenu is the real one; tests substitute a recording fake.
 */
class Notifier
{
public:
    virtual ~Notifier() = default;

    // Same defaults as QSystemTrayIcon::showMessage.
    virtual void notify(const QString &title, const QString &text,
                        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                        int msecs = 10000) = 0;
};

} // namespace App

#endif // APP_NOTIFIER_H
