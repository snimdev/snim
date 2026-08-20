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
    explicit HotkeyBackend(QObject *parent = nullptr);
    ~HotkeyBackend() override;

    // Make exactly these bindings live, dropping whatever was registered before.
    virtual void registerAll(const QList<HotkeyBinding> &bindings) = 0;
    virtual void unregisterAll() = 0;

signals:
    void activated(Hotkeys::HotkeyAction action);
    void registrationFailed(Hotkeys::HotkeyAction action, const QString &reason);
};

} // namespace Hotkeys

#endif // HOTKEYS_HOTKEYBACKEND_H
