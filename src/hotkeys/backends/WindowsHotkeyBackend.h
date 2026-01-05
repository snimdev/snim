#ifndef HOTKEYS_WINDOWSHOTKEYBACKEND_H
#define HOTKEYS_WINDOWSHOTKEYBACKEND_H

#include "hotkeys/HotkeyBackend.h"

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QHash>

namespace Hotkeys {

/**
 * Win32 backend: RegisterHotKey() with a null window, so WM_HOTKEY reaches the calling
 * thread's queue and this native event filter. No Windows types in the header; the .cpp
 * body is Q_OS_WIN-gated and only listed in the WIN32 build.
 */
class WindowsHotkeyBackend : public HotkeyBackend, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowsHotkeyBackend(QObject *parent = nullptr);
    ~WindowsHotkeyBackend() override;

    void registerAll(const QList<HotkeyBinding> &bindings) override;
    void unregisterAll() override;

    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] Capabilities capabilities() const override;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    // RegisterHotKey id (1..n, handed out per registerAll pass) to the action it fires.
    QHash<int, HotkeyAction> m_registered;
};

} // namespace Hotkeys

#endif // HOTKEYS_WINDOWSHOTKEYBACKEND_H
