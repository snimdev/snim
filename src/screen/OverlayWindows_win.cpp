#include "screen/OverlayWindows.h"

#include <QGuiApplication>
#include <QWidget>

#include <windows.h>
#include <dwmapi.h>

namespace Screen {

namespace {

// The HWND of a top-level widget, or null for child widgets and non-Windows platforms.
HWND topLevelHandle(QWidget *widget)
{
    if (!widget || !widget->isWindow())
        return nullptr;
    // Offscreen and test platforms hand out fake winIds that are not HWNDs.
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        return nullptr;
    return reinterpret_cast<HWND>(widget->winId());
}

// No DWM fade or zoom when the window appears.
void disableTransitions(HWND hwnd)
{
    const BOOL disabled = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disabled, sizeof(disabled));
}

// Other topmost windows (the taskbar) can still cover a stays-on-top window: re-assert after show.
void raiseTopmost(HWND hwnd, bool activate)
{
    UINT flags = SWP_NOMOVE | SWP_NOSIZE;
    if (!activate)
        flags |= SWP_NOACTIVATE;
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
}

void takeForeground(HWND hwnd)
{
    if (GetForegroundWindow() == hwnd || SetForegroundWindow(hwnd))
        return;

    // The foreground lock refuses a background process; sharing the foreground
    // thread's input state lifts it for this call.
    const DWORD ownThread = GetCurrentThreadId();
    const DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const bool attached = foregroundThread != 0 && foregroundThread != ownThread
                          && AttachThreadInput(ownThread, foregroundThread, TRUE);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    if (attached)
        AttachThreadInput(ownThread, foregroundThread, FALSE);
}

} // namespace

void configureOverlayWindow(QWidget *widget)
{
    HWND hwnd = topLevelHandle(widget);
    if (!hwnd)
        return;
    disableTransitions(hwnd);
    raiseTopmost(hwnd, /*activate=*/true);
    // An overlay opened from a global hotkey must take the keyboard (Enter/Esc).
    takeForeground(hwnd);
}

void configureRecordingHud(QWidget *widget)
{
    HWND hwnd = topLevelHandle(widget);
    if (!hwnd)
        return;
    disableTransitions(hwnd);
    raiseTopmost(hwnd, /*activate=*/false);
}

// Off macOS the options bar is a child of the overlay, so there is nothing to float.
void configureSelectionHud(QWidget *) {}

void excludeFromCapture(QWidget *widget)
{
    HWND hwnd = topLevelHandle(widget);
    if (!hwnd)
        return;
    // Fails before Windows 10 2004, where recording is unavailable anyway.
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
}

quint64 nativeWindowId(QWidget *widget)
{
    HWND hwnd = widget ? topLevelHandle(widget->window()) : nullptr;
    return quint64(reinterpret_cast<quintptr>(hwnd));
}

} // namespace Screen
