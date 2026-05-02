#include "screen/WindowEnumerator.h"

#import <CoreGraphics/CoreGraphics.h>
#import <CoreFoundation/CoreFoundation.h>

#include <unistd.h> // getpid

namespace Screen {

QVector<WindowInfo> enumerateWindowInfos()
{
    QVector<WindowInfo> result;

    // On-screen windows, front-to-back, excluding desktop/wallpaper elements.
    CFArrayRef windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
        kCGNullWindowID);
    if (!windows)
        return result;

    const pid_t myPid = getpid();
    const CFIndex count = CFArrayGetCount(windows);
    for (CFIndex i = 0; i < count; ++i) {
        auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, i));
        if (!info)
            continue;

        // Only normal application windows live on layer 0. Skip the menu bar,
        // Dock, status items, shadows, etc. (non-zero layers).
        int layer = 0;
        if (auto layerRef = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowLayer)))
            CFNumberGetValue(layerRef, kCFNumberIntType, &layer);
        if (layer != 0)
            continue;

        // Skip our own windows (the selection overlay).
        int pid = 0;
        if (auto pidRef = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowOwnerPID)))
            CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
        if (pid == myPid)
            continue;

        auto boundsDict = static_cast<CFDictionaryRef>(CFDictionaryGetValue(info, kCGWindowBounds));
        if (!boundsDict)
            continue;
        CGRect bounds;
        if (!CGRectMakeWithDictionaryRepresentation(boundsDict, &bounds))
            continue;

        // CGWindowBounds are in global display points (top-left origin), which
        // matches Qt's virtual-desktop logical coordinate space on macOS.
        QRect r(static_cast<int>(bounds.origin.x), static_cast<int>(bounds.origin.y),
                static_cast<int>(bounds.size.width), static_cast<int>(bounds.size.height));
        if (r.width() < 8 || r.height() < 8)
            continue; // ignore tiny/utility windows

        // The CGWindowID lets a recorder target this exact window.
        quint64 windowId = 0;
        if (auto idRef = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowNumber))) {
            long long n = 0;
            CFNumberGetValue(idRef, kCFNumberLongLongType, &n);
            windowId = static_cast<quint64>(n);
        }

        result.append(WindowInfo{ r, windowId });
    }

    CFRelease(windows);
    return result; // CGWindowList already returns front-to-back order
}

} // namespace Screen
