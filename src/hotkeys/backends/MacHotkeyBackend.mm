#include "hotkeys/backends/MacHotkeyBackend.h"

#include "hotkeys/MacKeyMapping.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QString>
#include <QtGlobal>

#include <optional>

#include <Carbon/Carbon.h>

namespace {

// MacKeyMapping.cpp repeats these values so it can compile with no Apple headers;
// drift between it and the SDK breaks the build here instead of a hotkey at runtime.
static_assert(cmdKey == 0x0100, "Carbon cmdKey no longer matches MacKeyMapping.cpp");
static_assert(shiftKey == 0x0200, "Carbon shiftKey no longer matches MacKeyMapping.cpp");
static_assert(optionKey == 0x0800, "Carbon optionKey no longer matches MacKeyMapping.cpp");
static_assert(controlKey == 0x1000, "Carbon controlKey no longer matches MacKeyMapping.cpp");
static_assert(kVK_ANSI_A == 0x00, "kVK_ANSI_A no longer matches MacKeyMapping.cpp");
static_assert(kVK_F5 == 0x60, "kVK_F5 no longer matches MacKeyMapping.cpp");
static_assert(kVK_LeftArrow == 0x7B, "kVK_LeftArrow no longer matches MacKeyMapping.cpp");

// Tells this app's hotkeys apart from every other client of the shared Carbon target.
constexpr OSType kHotkeySignature = 'NSHK';

} // namespace

namespace Hotkeys {

// Carbon state, kept out of the header.
struct MacHotkeyBackend::Impl {
    EventHandlerRef handler = nullptr;       // installed once, in the ctor
    QList<EventHotKeyRef> refs;              // one per live registration
    QHash<quint32, HotkeyAction> actions;    // EventHotKeyID.id -> what it fires
    quint32 nextId = 1;                      // never reset, see unregisterAll()

    // Called from the Carbon handler; nested, so it may reach the owner's pimpl.
    static bool dispatch(MacHotkeyBackend *backend, quint32 hotkeyId);
};

bool MacHotkeyBackend::Impl::dispatch(MacHotkeyBackend *backend, quint32 hotkeyId)
{
    const auto it = backend->d->actions.constFind(hotkeyId);
    if (it == backend->d->actions.constEnd())
        return false;

    // Queued: the handler runs inside native event dispatch (menu/drag tracking may be
    // live), and a posted call is dropped if the backend dies before the tick.
    const HotkeyAction action = *it;
    QMetaObject::invokeMethod(
        backend, [backend, action]() { backend->reportActivated(action); },
        Qt::QueuedConnection);
    return true;
}

} // namespace Hotkeys

namespace {

OSStatus carbonHotkeyHandler(EventHandlerCallRef, EventRef event, void *userData)
{
    auto *backend = static_cast<Hotkeys::MacHotkeyBackend *>(userData);
    if (!backend || !event)
        return eventNotHandledErr;

    EventHotKeyID hotkeyId = {};
    if (GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                          sizeof(hotkeyId), nullptr, &hotkeyId) != noErr)
        return eventNotHandledErr;
    if (hotkeyId.signature != kHotkeySignature)
        return eventNotHandledErr;

    if (!Hotkeys::MacHotkeyBackend::Impl::dispatch(backend, hotkeyId.id))
        return eventNotHandledErr;
    return noErr;
}

} // namespace

namespace Hotkeys {

MacHotkeyBackend::MacHotkeyBackend(QObject *parent)
    : HotkeyBackend(parent), d(std::make_unique<Impl>())
{
    // One handler for every hotkey: they are told apart by their EventHotKeyID.
    const EventTypeSpec eventType = {kEventClassKeyboard, kEventHotKeyPressed};
    InstallEventHandler(GetApplicationEventTarget(), carbonHotkeyHandler, 1, &eventType,
                        this, &d->handler);
}

MacHotkeyBackend::~MacHotkeyBackend()
{
    unregisterAll();
    if (d->handler) {
        RemoveEventHandler(d->handler);
        d->handler = nullptr;
    }
}

void MacHotkeyBackend::registerAll(const QList<HotkeyBinding> &bindings)
{
    // registerAll() replaces the set, so the previous refs go first.
    unregisterAll();

    for (const HotkeyBinding &binding : bindings) {
        const std::optional<CarbonHotkey> hotkey = toCarbonHotkey(binding.sequence);
        if (!hotkey) {
            emit registrationFailed(
                binding.action, tr("this key combination cannot be used as a global hotkey"));
            continue;
        }

        const quint32 id = d->nextId++;
        const EventHotKeyID hotkeyId = {kHotkeySignature, id};
        EventHotKeyRef ref = nullptr;
        const OSStatus status = RegisterEventHotKey(hotkey->keyCode, hotkey->modifiers,
                                                    hotkeyId, GetApplicationEventTarget(),
                                                    0, &ref);
        if (status != noErr || !ref) {
            // Per-binding best effort: one rejected key must not drop the rest.
            emit registrationFailed(binding.action,
                                    status == eventHotKeyExistsErr
                                        ? tr("already in use by another application")
                                        : tr("registration failed (status %1)").arg(status));
            continue;
        }

        d->refs.append(ref);
        d->actions.insert(id, binding.action);
    }
}

void MacHotkeyBackend::unregisterAll()
{
    while (!d->refs.isEmpty())
        UnregisterEventHotKey(d->refs.takeLast());
    // Ids stay monotonic: an event for a just-unregistered key can still be in the
    // native queue, and a reused id would map it onto whatever took that slot.
    d->actions.clear();
}

void MacHotkeyBackend::reportActivated(HotkeyAction action)
{
    emit activated(action);
}

} // namespace Hotkeys
