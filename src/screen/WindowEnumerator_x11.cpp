#include "screen/WindowEnumerator.h"

#include "screen/X11ScreenMap.h"
#include "screen/X11Windows.h"

namespace Screen {

// The X11 desktop's windows, from its window manager. Elsewhere (a Wayland session, the
// offscreen platform) there is no X connection and the picker falls back to screens.
QVector<WindowInfo> enumerateWindowInfos()
{
    const QVector<X11Windows::Window> windows = X11Windows::pickableWindows();
    if (windows.isEmpty())
        return {};

    const QVector<X11ScreenMap::Screen> screens = X11ScreenMap::currentScreens();

    QVector<WindowInfo> result;
    for (const X11Windows::Window &window : windows) {
        const QRect logical = X11ScreenMap::toLogical(window.visible, screens);
        if (!logical.isEmpty())
            result.append(WindowInfo{logical, window.id});
    }
    return result;
}

} // namespace Screen
