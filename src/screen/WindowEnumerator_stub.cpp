#include "screen/WindowEnumerator.h"

namespace Screen {

// Platforms without a native window enumerator (e.g. Linux/Wayland, where global
// window geometry is unavailable).
// Returning empty makes window-pick fall back to highlighting the screen under
// the cursor.
QVector<WindowInfo> enumerateWindowInfos()
{
    return {};
}

} // namespace Screen
