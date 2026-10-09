#include "hotkeys/screenshotkey/MacScreenshotKey.h"

#include <QStandardPaths>

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

// Built with -fobjc-arc: ARC manages the NS* objects, CFBridgingRelease takes the CF copies.
namespace {

// MacSymbolicHotkeys.cpp repeats these so it compiles with no Apple headers; drift
// breaks the build here instead of turning off the wrong shortcut.
static_assert(kVK_ANSI_3 == 20, "kVK_ANSI_3 no longer matches MacSymbolicHotkeys.cpp");
static_assert(kVK_ANSI_4 == 21, "kVK_ANSI_4 no longer matches MacSymbolicHotkeys.cpp");
static_assert(kVK_ANSI_5 == 23, "kVK_ANSI_5 no longer matches MacSymbolicHotkeys.cpp");
static_assert((NSEventModifierFlagShift | NSEventModifierFlagCommand) == NSUInteger(0x120000),
              "Shift | Command no longer matches MacSymbolicHotkeys.cpp");

} // namespace

namespace Hotkeys {

namespace {

CFStringRef symbolicHotkeysDomain()
{
    return CFSTR("com.apple.symbolichotkeys");
}

CFStringRef symbolicHotkeysKey()
{
    return CFSTR("AppleSymbolicHotKeys");
}

NSString *entryKey(int number)
{
    return [NSString stringWithFormat:@"%d", number];
}

// AppleSymbolicHotKeys as the prefs hold it now; nil when it was never written.
NSDictionary *readSymbolicHotkeys()
{
    // System Settings may have changed it since this process last read it.
    CFPreferencesAppSynchronize(symbolicHotkeysDomain());
    id value = CFBridgingRelease(
        CFPreferencesCopyAppValue(symbolicHotkeysKey(), symbolicHotkeysDomain()));
    return [value isKindOfClass:[NSDictionary class]] ? value : nil;
}

MacSymbolicState stateIn(NSDictionary *hotkeys, int number)
{
    id entry = hotkeys[entryKey(number)];
    if (![entry isKindOfClass:[NSDictionary class]])
        return MacSymbolicState::Absent;
    id enabled = entry[@"enabled"];
    // Without a readable flag the entry still answers to its key.
    if (![enabled respondsToSelector:@selector(boolValue)])
        return MacSymbolicState::Enabled;
    return [enabled boolValue] ? MacSymbolicState::Enabled : MacSymbolicState::Disabled;
}

MacSymbolicStates statesIn(NSDictionary *hotkeys)
{
    MacSymbolicStates states;
    for (const MacSymbolicHotkey &hotkey : macScreenshotHotkeys())
        states.insert(hotkey.id, stateIn(hotkeys, hotkey.id));
    return states;
}

std::optional<MacSymbolicStates> readStates()
{
    // Tests run on the developer's own Mac: never their prefs.
    if (QStandardPaths::isTestModeEnabled())
        return std::nullopt;
    MacSymbolicStates states;
    @autoreleasepool {
        states = statesIn(readSymbolicHotkeys());
    }
    return states;
}

void applyEdit(NSMutableDictionary *hotkeys, const MacSymbolicHotkey &hotkey,
               MacSymbolicWrite edit)
{
    NSString *key = entryKey(hotkey.id);
    switch (edit) {
    case MacSymbolicWrite::Keep:
        return;
    case MacSymbolicWrite::Remove:
        [hotkeys removeObjectForKey:key];
        return;
    case MacSymbolicWrite::AddDisabled:
        // The shape System Settings writes for a shortcut it turns off.
        hotkeys[key] = @{
            @"enabled" : @NO,
            @"value" : @{
                @"parameters" : @[ @(hotkey.parameters[0]), @(hotkey.parameters[1]),
                                   @(hotkey.parameters[2]) ],
                @"type" : @"standard",
            },
        };
        return;
    case MacSymbolicWrite::Disable:
    case MacSymbolicWrite::Enable: {
        id current = hotkeys[key];
        if (![current isKindOfClass:[NSDictionary class]])
            return;
        NSMutableDictionary *entry = [current mutableCopy];
        entry[@"enabled"] = edit == MacSymbolicWrite::Enable ? @YES : @NO;
        hotkeys[key] = entry;
        return;
    }
    }
}

bool writeEdits(const MacSymbolicWrites &edits)
{
    if (QStandardPaths::isTestModeEnabled())
        return false;
    bool saved = false;
    @autoreleasepool {
        // Every other entry is written back as it was.
        NSMutableDictionary *hotkeys = nil;
        if (NSDictionary *current = readSymbolicHotkeys())
            hotkeys = [current mutableCopy];
        else
            hotkeys = [NSMutableDictionary dictionary];
        for (const MacSymbolicHotkey &hotkey : macScreenshotHotkeys())
            applyEdit(hotkeys, hotkey, edits.value(hotkey.id, MacSymbolicWrite::Keep));
        CFPreferencesSetAppValue(symbolicHotkeysKey(), (__bridge CFPropertyListRef)hotkeys,
                                 symbolicHotkeysDomain());
        saved = CFPreferencesAppSynchronize(symbolicHotkeysDomain());
    }
    return saved;
}

} // namespace

MacScreenshotKey::Prefs macSymbolicPrefs()
{
    return {&readStates, &writeEdits, &MacScreenshotKey::canActivateSettings,
            &MacScreenshotKey::activateSettings};
}

} // namespace Hotkeys
