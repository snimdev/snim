#ifndef CORE_MACTRAYWORKAROUND_H
#define CORE_MACTRAYWORKAROUND_H

namespace Core {

/**
 * macOS only: replace Qt's -[QStatusItemDelegate statusItemMenuBeganTracking:]
 * with a no-op. That callback runs QCocoaSystemTrayIcon::emitActivated(), which
 * reads -[NSEvent clickCount] off NSApp.currentEvent without checking the event
 * type. On macOS 27 the status item is hosted out of process, so the current
 * event when the menu opens is a KitDefined event, clickCount raises, and the
 * uncaught ObjC exception aborts the app on every left click. Qt fixes this in
 * 6.12, where this call does nothing. Call once, after QApplication exists and
 * before the tray icon is created.
 */
void applyTrayMenuTrackingWorkaround();

} // namespace Core

#endif // CORE_MACTRAYWORKAROUND_H
