#include "hotkeys/GlobalHotkeyManager.h"

#include "hotkeys/HotkeyBackendFactory.h"
#include "hotkeys/HotkeyBindings.h"

namespace Hotkeys {

GlobalHotkeyManager::GlobalHotkeyManager(QObject *parent)
    : GlobalHotkeyManager(HotkeyBackendFactory::create(), parent)
{
}

GlobalHotkeyManager::GlobalHotkeyManager(std::unique_ptr<HotkeyBackend> backend, QObject *parent)
    : QObject(parent), m_backend(std::move(backend))
{
    wireBackend();
}

void GlobalHotkeyManager::wireBackend()
{
    if (!m_backend)
        return;

    connect(m_backend.get(), &HotkeyBackend::activated,
            this, &GlobalHotkeyManager::actionTriggered);

    connect(m_backend.get(), &HotkeyBackend::registrationFailed, this,
            [this](HotkeyAction action, const QString &reason) {
                // NativeText so the user sees Cmd, not Ctrl, on macOS.
                const QString keys =
                    HotkeyBindings::sequence(action).toString(QKeySequence::NativeText);
                emit registrationFailed(
                    tr("Global hotkey %1 for %2 could not be registered (%3)")
                        .arg(keys, hotkeyActionDescription(action), reason));
            });
}

void GlobalHotkeyManager::applyBindings()
{
    // Nothing to unregister either: an unavailable backend never registered anything.
    if (!isAvailable())
        return;

    m_backend->unregisterAll();
    m_backend->registerAll(HotkeyBindings::activeBindings());
}

bool GlobalHotkeyManager::isAvailable() const
{
    return m_backend && m_backend->isAvailable();
}

HotkeyBackend::Capabilities GlobalHotkeyManager::capabilities() const
{
    if (!m_backend)
        return HotkeyBackend::Capability::None;
    return m_backend->capabilities();
}

} // namespace Hotkeys
