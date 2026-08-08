#ifndef HOTKEYS_HOTKEYBACKEND_H
#define HOTKEYS_HOTKEYBACKEND_H

#include "hotkeys/HotkeyAction.h"

#include <QList>
#include <QObject>
#include <QString>

namespace Hotkeys {

// Abstract base for the platform global-hotkey backends (the RecordingStrategy seam).
// Registration is per-binding best effort: a taken key fails alone via registrationFailed.
class HotkeyBackend : public QObject
{
    Q_OBJECT

public:
    enum class Capability {
        None = 0x0,
        // Configured sequences ARE the real bindings; the Linux portal clears this
        // (the desktop owns the keys, ours are only preferred triggers).
        UserConfiguresKeys = 0x1
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    explicit HotkeyBackend(QObject *parent = nullptr);
    ~HotkeyBackend() override;

    // Make exactly these bindings live, dropping whatever was registered before.
    virtual void registerAll(const QList<HotkeyBinding> &bindings) = 0;
    virtual void unregisterAll() = 0;

    // Can run here at all; says nothing about whether an individual sequence is free.
    [[nodiscard]] virtual bool isAvailable() const = 0;

signals:
    void activated(Hotkeys::HotkeyAction action);
    void registrationFailed(Hotkeys::HotkeyAction action, const QString &reason);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HotkeyBackend::Capabilities)

} // namespace Hotkeys

#endif // HOTKEYS_HOTKEYBACKEND_H
