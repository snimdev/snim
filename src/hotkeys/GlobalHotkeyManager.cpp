#include "hotkeys/GlobalHotkeyManager.h"

#include "hotkeys/HotkeyBackendFactory.h"
#include "hotkeys/HotkeyBindings.h"

#include <QTimer>

#include <utility>

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
    connect(m_backend.get(), &HotkeyBackend::activated,
            this, &GlobalHotkeyManager::actionTriggered);

    connect(m_backend.get(), &HotkeyBackend::registrationFailed, this,
            [this](HotkeyAction action, const QString &reason) {
                // NativeText so the user sees Cmd, not Ctrl, on macOS.
                const QString keys =
                    HotkeyBindings::sequence(action).toString(QKeySequence::NativeText);
                m_failures.insert(action, reason);
                m_pendingFailures.append(
                    tr("Global hotkey %1 for %2 could not be registered (%3)")
                        .arg(keys, hotkeyActionDescription(action), reason));
                scheduleFailureFlush();
            });
}

void GlobalHotkeyManager::applyBindings()
{
    const QList<HotkeyBinding> bindings = HotkeyBindings::activeBindings();
    m_requestedBindings = bindings.size();
    m_pendingFailures.clear();
    m_failures.clear();

    m_backend->unregisterAll();
    m_backend->registerAll(bindings);
    // Even a clean pass flushes, so observers drop the previous pass's failures.
    scheduleFailureFlush();
}

QString GlobalHotkeyManager::failureReason(HotkeyAction a) const
{
    return m_failures.value(a);
}

bool GlobalHotkeyManager::allFailed() const
{
    return m_requestedBindings > 0 && m_failures.size() >= m_requestedBindings;
}

void GlobalHotkeyManager::scheduleFailureFlush()
{
    if (m_flushScheduled)
        return;
    m_flushScheduled = true;
    // Deferred one turn: a failing pass reports every binding in one burst, and the
    // "nothing registered" case has to come out as a single message.
    QTimer::singleShot(0, this, &GlobalHotkeyManager::flushFailures);
}

void GlobalHotkeyManager::flushFailures()
{
    m_flushScheduled = false;
    emit failuresChanged();

    const QStringList failures = std::exchange(m_pendingFailures, {});
    if (failures.isEmpty())
        return;

    if (m_requestedBindings > 0 && failures.size() >= m_requestedBindings) {
        // Not one hotkey survived: on Linux that is the portal refusing a caller with no
        // app id, which a launcher start (an app-scoped systemd unit) gives it.
        emit registrationFailed(tr("Global hotkeys are unavailable; starting Snim from "
                                   "the application menu can fix this."));
        return;
    }

    for (const QString &message : failures)
        emit registrationFailed(message);
}

} // namespace Hotkeys
