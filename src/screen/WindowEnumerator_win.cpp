#include "screen/WindowEnumerator.h"
#include "screen/WinScreenMap.h"
#include "screen/WinWindowFilter.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtGui/qscreen_platform.h>

#include <iterator>

#include <windows.h>
#include <dwmapi.h>

namespace Screen {

QVector<WinScreenMap::Screen> WinScreenMap::currentScreens()
{
    QVector<Screen> map;
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        auto *native = screen->nativeInterface<QNativeInterface::QWindowsScreen>();
        if (!native)
            continue;   // offscreen and other non-Windows platforms
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(native->handle(), &info))
            continue;
        const RECT &m = info.rcMonitor;
        map.append({ QRect(m.left, m.top, m.right - m.left, m.bottom - m.top),
                     screen->geometry().topLeft(), screen->devicePixelRatio() });
    }
    return map;
}

QRect WinScreenMap::frameBounds(void *window)
{
    const HWND hwnd = static_cast<HWND>(window);
    // The extended frame excludes the invisible resize borders GetWindowRect includes.
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r))))
        GetWindowRect(hwnd, &r);
    return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
}

namespace {

WinWindowFilter::Candidate describe(HWND hwnd)
{
    WinWindowFilter::Candidate w;
    w.visible = IsWindowVisible(hwnd);
    w.minimised = IsIconic(hwnd);

    DWORD cloaked = 0;
    w.cloaked = SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
                && cloaked != 0;
    w.toolWindow = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0;

    wchar_t className[256] = {};
    const int length = GetClassNameW(hwnd, className, int(std::size(className)));
    w.className = QString::fromWCharArray(className, qMax(length, 0));

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    w.processId = processId;

    w.bounds = WinScreenMap::frameBounds(hwnd);
    return w;
}

BOOL CALLBACK collect(HWND hwnd, LPARAM param)
{
    reinterpret_cast<QVector<HWND> *>(param)->append(hwnd);
    return TRUE;
}

} // namespace

QVector<WindowInfo> enumerateWindowInfos()
{
    QVector<WindowInfo> result;
    const QVector<WinScreenMap::Screen> screens = WinScreenMap::currentScreens();
    if (screens.isEmpty())
        return result;

    // EnumWindows walks the top-level windows in z-order, topmost first.
    QVector<HWND> windows;
    EnumWindows(collect, reinterpret_cast<LPARAM>(&windows));

    const quint32 ownProcessId = GetCurrentProcessId();
    for (HWND hwnd : windows) {
        const WinWindowFilter::Candidate w = describe(hwnd);
        if (!WinWindowFilter::isPickable(w, ownProcessId))
            continue;
        const QRect logical = WinScreenMap::toLogical(w.bounds, screens);
        if (logical.isEmpty())
            continue;
        result.append(WindowInfo{ logical, quint64(reinterpret_cast<quintptr>(hwnd)) });
    }
    return result;
}

} // namespace Screen
