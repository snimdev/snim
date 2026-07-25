#include "screen/WindowEnumerator.h"

#include "screen/X11ScreenMap.h"
#include "screen/X11Windows.h"

#include <QGuiApplication>
#include <QScreen>

namespace Screen {

// The X11 desktop's windows, from its window manager. Elsewhere (a Wayland session, the
// offscreen platform) there is no X connection and the picker falls back to screens.
QVector<WindowInfo> enumerateWindowInfos()
{
    const QVector<X11Windows::Window> windows = X11Windows::pickableWindows();
    if (windows.isEmpty())
        return {};

    QVector<X11ScreenMap::Screen> screens;
    const QList<QScreen *> all = QGuiApplication::screens();
    for (const QScreen *screen : all)
        screens.append(X11ScreenMap::Screen{screen->geometry(), screen->devicePixelRatio()});

    QVector<WindowInfo> result;
    for (const X11Windows::Window &window : windows) {
        const QRect logical = X11ScreenMap::toLogical(window.visible, screens);
        if (!logical.isEmpty())
            result.append(WindowInfo{logical, window.id});
    }
    return result;
}

} // namespace Screen
