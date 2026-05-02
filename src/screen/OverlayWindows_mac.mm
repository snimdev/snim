#include "screen/OverlayWindows.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <QGuiApplication>
#include <QWidget>

namespace Screen {

void configureOverlayWindow(QWidget *widget)
{
    if (!widget)
        return;
    // Offscreen and test platforms hand out fake winIds that are not NSViews.
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        return;

    // On macOS WId is the NSView*; its window is the backing NSWindow.
    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = view ? [view window] : nil;
    if (!window)
        return;

    // Sit above the menu bar and Dock, and never animate the window in. This is
    // what makes the overlay appear instantly with no Spaces transition and no
    // menu-bar slide-away (the distracting "fullscreen" animation we want gone).
    window.level = (NSInteger)CGShieldingWindowLevel();
    window.animationBehavior = NSWindowAnimationBehaviorNone;
    window.collectionBehavior = (NSWindowCollectionBehaviorCanJoinAllSpaces |
                                 NSWindowCollectionBehaviorStationary |
                                 NSWindowCollectionBehaviorFullScreenAuxiliary |
                                 NSWindowCollectionBehaviorIgnoresCycle);
    window.opaque = NO;
    window.hasShadow = NO;

    // Agent (LSUIElement) apps don't auto-activate; force focus so the overlay
    // receives key events (Enter/Esc/arrows).
    [NSApp activateIgnoringOtherApps:YES];
    [window makeKeyAndOrderFront:nil];
}

void configureRecordingHud(QWidget *widget)
{
    if (!widget)
        return;
    // Offscreen and test platforms hand out fake winIds that are not NSViews.
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        return;

    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = view ? [view window] : nil;
    if (!window)
        return;

    // Float above normal windows, across all Spaces, with no appear animation.
    window.level = NSStatusWindowLevel;
    window.animationBehavior = NSWindowAnimationBehaviorNone;
    window.collectionBehavior = (NSWindowCollectionBehaviorCanJoinAllSpaces |
                                 NSWindowCollectionBehaviorStationary |
                                 NSWindowCollectionBehaviorFullScreenAuxiliary |
                                 NSWindowCollectionBehaviorIgnoresCycle);
    // Stay on screen when Snim is deactivated. This is the key difference from
    // the selection overlay: while recording, the user clicks OTHER apps, which
    // deactivates us; without this the Tool window would hide and the Stop control
    // would vanish.
    window.hidesOnDeactivate = NO;
    window.opaque = NO;

    // Never steal focus from the app being recorded: a Qt::Tool window is an
    // NSPanel, so make it a non-activating floating panel and just order it front.
    if ([window isKindOfClass:[NSPanel class]]) {
        NSPanel *panel = static_cast<NSPanel *>(window);
        panel.floatingPanel = YES;
        panel.becomesKeyOnlyIfNeeded = YES;
        panel.styleMask |= NSWindowStyleMaskNonactivatingPanel;
    }
    [window orderFrontRegardless];
}

void configureSelectionHud(QWidget *widget)
{
    if (!widget)
        return;
    // Offscreen and test platforms hand out fake winIds that are not NSViews.
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        return;

    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = view ? [view window] : nil;
    if (!window)
        return;

    // Same level as the selection overlay; being ordered front AFTER it stacks the
    // bar on top. Non-activating, so the overlay keeps the keyboard (Enter/Esc).
    window.level = (NSInteger)CGShieldingWindowLevel();
    window.animationBehavior = NSWindowAnimationBehaviorNone;
    window.collectionBehavior = (NSWindowCollectionBehaviorCanJoinAllSpaces |
                                 NSWindowCollectionBehaviorStationary |
                                 NSWindowCollectionBehaviorFullScreenAuxiliary |
                                 NSWindowCollectionBehaviorIgnoresCycle);
    window.hidesOnDeactivate = NO;
    window.opaque = NO;
    if ([window isKindOfClass:[NSPanel class]]) {
        NSPanel *panel = static_cast<NSPanel *>(window);
        panel.floatingPanel = YES;
        panel.becomesKeyOnlyIfNeeded = YES;
        panel.styleMask |= NSWindowStyleMaskNonactivatingPanel;
    }
    [window orderFrontRegardless];
}

// The ScreenCaptureKit filter already leaves the whole app out of the recording.
void excludeFromCapture(QWidget *) {}

quint64 nativeWindowId(QWidget *widget)
{
    if (!widget)
        return 0;
    NSView *view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = view ? [view window] : nil;
    return window ? static_cast<quint64>([window windowNumber]) : 0;
}

} // namespace Screen
