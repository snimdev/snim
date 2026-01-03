#include "MacOverlay.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <QWidget>

namespace Capture {

void configureOverlayWindow(QWidget *widget)
{
    if (!widget)
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

} // namespace Capture
