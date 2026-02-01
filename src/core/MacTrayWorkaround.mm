#include "core/MacTrayWorkaround.h"

#import <objc/runtime.h>

#include <QDebug>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QVersionNumber>

namespace Core {

namespace {

// Stands in for Qt's callback, whose only job was to emit activated(): a status item
// that owns a menu can never raise the DoubleClick that Snim listens for.
void trayMenuBeganTrackingNoop(id, SEL, id)
{
}

} // namespace

void applyTrayMenuTrackingWorkaround()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    // Offscreen and test platforms never load the cocoa plugin, so the class is absent.
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        return;

    // First feature release carrying the upstream event-type check.
    if (QLibraryInfo::version() >= QVersionNumber(6, 12, 0))
        return;

    Class delegateClass = objc_getClass("QStatusItemDelegate");
    Method method = delegateClass
                        ? class_getInstanceMethod(delegateClass, sel_registerName("statusItemMenuBeganTracking:"))
                        : nullptr;
    if (!method) {
        qWarning() << "[Tray] Qt's status item delegate changed, menu-open workaround skipped";
        return;
    }

    // A void no-op only stands in for a method that returns void.
    const char *types = method_getTypeEncoding(method);
    if (!types || types[0] != 'v') {
        qWarning() << "[Tray] unexpected callback signature" << types;
        return;
    }

    const IMP noop = reinterpret_cast<IMP>(trayMenuBeganTrackingNoop);
    method_setImplementation(method, noop);

    // Relative method lists keep replaced IMPs in a runtime side table, so read it back.
    if (method_getImplementation(method) != noop) {
        qWarning() << "[Tray] could not replace Qt's menu-tracking callback";
        return;
    }

    qInfo() << "[Tray] neutralised Qt's menu-tracking activation callback, Qt"
            << QLibraryInfo::version().toString();
}

} // namespace Core
