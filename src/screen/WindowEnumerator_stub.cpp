#include "screen/WindowEnumerator.h"

namespace Screen {

// Platforms without a native window enumerator, Linux builds without XCB among them.
// Returning empty makes window-pick fall back to highlighting the screen under
// the cursor.
QVector<WindowInfo> enumerateWindowInfos()
{
    return {};
}

} // namespace Screen
