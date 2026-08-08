#ifndef HOTKEYS_MACHOTKEYBACKEND_H
#define HOTKEYS_MACHOTKEYBACKEND_H

#include "hotkeys/HotkeyBackend.h"

#include <memory>

namespace Hotkeys {

// macOS global hotkeys via Carbon RegisterEventHotKey (needs no Accessibility grant,
// unlike a CGEventTap). Carbon types stay in the .mm behind a pimpl so moc never sees them.
class MacHotkeyBackend : public HotkeyBackend
{
    Q_OBJECT

public:
    explicit MacHotkeyBackend(QObject *parent = nullptr);
    ~MacHotkeyBackend() override;

    void registerAll(const QList<HotkeyBinding> &bindings) override;
    void unregisterAll() override;

    // Carbon is always there; only individual sequences can fail.
    [[nodiscard]] bool isAvailable() const override { return true; }

    // Backend hook: called by the Carbon handler after it hops onto this thread.
    void reportActivated(HotkeyAction action);

    struct Impl;   // defined in the .mm (Carbon handler, refs, id map)

private:
    std::unique_ptr<Impl> d;
};

} // namespace Hotkeys

#endif // HOTKEYS_MACHOTKEYBACKEND_H
