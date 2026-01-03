#include "WindowEnumerator.h"

namespace Capture {

// Platforms without a native window enumerator (e.g. Windows until EnumWindows
// is implemented, or Linux/Wayland where global window geometry is unavailable).
// Returning empty makes window-pick fall back to highlighting the screen under
// the cursor.
QVector<WindowInfo> enumerateWindowInfos()
{
    return {};
}

} // namespace Capture
