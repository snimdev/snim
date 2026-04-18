#ifndef CORE_SINGLEINSTANCE_H
#define CORE_SINGLEINSTANCE_H

#include <functional>

class QObject;

/**
 * Windows single-instance guard. A per-session named mutex decides which Snim runs; a later
 * launch pings the running one over a local socket and exits. The Inno Setup script's
 * AppMutex names the same mutex, so setup can tell when Snim is still running.
 */
namespace Core::SingleInstance {

// Claims the instance mutex; false when another Snim in this session already holds it.
[[nodiscard]] bool claim();

// Tells the running instance that Snim was launched again.
void notifyRunningInstance();

// Calls onLaunch each time a later launch pings this instance.
void listen(QObject *parent, std::function<void()> onLaunch);

} // namespace Core::SingleInstance

#endif // CORE_SINGLEINSTANCE_H
