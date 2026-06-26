// macos_menu.mm
//
// Builds zTracker's macOS menu bar (zTracker / File / Playback / Instruments /
// Settings / Window). Each item injects the equivalent in-app keystroke through
// the shared KeyBuffer, so a menu click takes the exact same path as pressing
// the key — and each item's keyEquivalent shows (and triggers) that shortcut.
//
// We also strip the Cmd-Q / Cmd-W key equivalents from any standard items: the
// Pattern Editor reuses Cmd-Q as Transpose-Up (Cmd is treated as Alt via
// KS_HAS_ALT) and Cmd-W as clear-unused-volumes, so neither may be claimed by
// the menu bar. Quit is still reachable from the keyboard via Ctrl-Q and from
// the app-menu click.

#import <Cocoa/Cocoa.h>
#include "keybuffer.h"

#define FN_KEY(n) [NSString stringWithFormat:@"%C", (unichar)(NSF##n##FunctionKey)]

extern KeyBuffer Keys;

// View-menu hooks (src/CUI_MainMenu.cpp): reuse the in-app deferred resolution
// change so a menu click is applied safely at the next frame.
extern "C" void zt_view_apply_size_preset(int idx);
extern "C" void zt_view_apply_zoom(float zoom);
extern "C" int zt_view_size_preset_count(void);
extern "C" const char *zt_view_size_preset_label(int idx);

// NSMenuItem carrying the keystroke to inject when chosen. ztMod is an
// SDL_KMOD_* mask (KeyBuffer::insert converts it to the KS_* state); ztCode is
// the SDL scancode, needed only by layout-position lookups (note entry).
@interface ZTKeyMenuItem : NSMenuItem
@property (nonatomic, assign) unsigned int ztKey;
@property (nonatomic, assign) unsigned int ztMod;
@property (nonatomic, assign) unsigned int ztCode;
@property (nonatomic, assign) float ztZoom;
@end
@implementation ZTKeyMenuItem
@end

@interface ZTMenuTarget : NSObject
- (void)inject:(id)sender;
- (void)applySize:(id)sender;
- (void)applyZoom:(id)sender;
@end
@implementation ZTMenuTarget
- (void)inject:(id)sender {
    // Drop events initiated by the menubar hotkey, SDL will handle the keystroke
    NSEvent *ev = [NSApp currentEvent];
    if (ev && ev.type == NSEventTypeKeyDown) {
        return;
    }
    ZTKeyMenuItem *item = (ZTKeyMenuItem *)sender;
    Keys.insert((KBKey)item.ztKey, (KBMod)item.ztMod, 0, item.ztCode);
}
- (void)applySize:(id)sender {
    zt_view_apply_size_preset((int)[(NSMenuItem *)sender tag]);
}
- (void)applyZoom:(id)sender {
    zt_view_apply_zoom(((ZTKeyMenuItem *)sender).ztZoom);
}
@end

// AppKit auto-injects a native "Enter Full Screen" (toggleFullScreen:) item
// into any menu titled "View". Strip it before each open so only zt's own
// Fullscreen item (Alt+Enter) appears.
@interface ZTViewMenuDelegate : NSObject <NSMenuDelegate>
@end
@implementation ZTViewMenuDelegate
- (void)menuNeedsUpdate:(NSMenu *)menu {
    for (NSInteger i = menu.numberOfItems - 1; i >= 0; i--) {
        if ([menu itemAtIndex:i].action == @selector(toggleFullScreen:))
            [menu removeItemAtIndex:i];
    }
}
@end

// Retained for the process lifetime; NSMenuItem holds its target weakly.
static ZTMenuTarget *g_zt_menu_target = nil;
static ZTViewMenuDelegate *g_zt_view_delegate = nil;

static void zt_add_key_item(NSMenu *menu, NSString *title,
                            unsigned int key, unsigned int mod, unsigned int code,
                            NSString *keyEquiv, NSEventModifierFlags mask) {
    // keyEquiv/mask DISPLAY the shortcut in the menu. The keyboard double-fire
    // that would otherwise cause (menu action + SDL both seeing the key) is
    // handled in -inject:, which ignores keyDown-triggered fires.
    ZTKeyMenuItem *item = [[ZTKeyMenuItem alloc] initWithTitle:title
                                                        action:@selector(inject:)
                                                 keyEquivalent:keyEquiv];
    item.target = g_zt_menu_target;
    item.ztKey  = key;
    item.ztMod  = mod;
    item.ztCode = code;
    [item setKeyEquivalentModifierMask:mask];
    [menu addItem:item];
}

static void zt_add_size_item(NSMenu *menu, NSString *title, int idx) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:@selector(applySize:)
                                           keyEquivalent:@""];
    item.target = g_zt_menu_target;
    item.tag = idx;
    [menu addItem:item];
}

static void zt_add_zoom_item(NSMenu *menu, NSString *title, float zoom) {
    ZTKeyMenuItem *item = [[ZTKeyMenuItem alloc] initWithTitle:title
                                                        action:@selector(applyZoom:)
                                                 keyEquivalent:@""];
    item.target = g_zt_menu_target;
    item.ztZoom = zoom;
    [menu addItem:item];
}

// Works for both a menu-bar submenu (the bar shows the submenu title) and a
// nested submenu (the dropdown shows the item title) -- set both.
static NSMenu *zt_add_submenu(NSMenu *parent, NSString *title) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:title];
    [item setSubmenu:submenu];
    [parent addItem:item];
    return submenu;
}

static void zt_strip_key_equivalent_for_selector(SEL sel)
{
    NSMenu *mainMenu = [NSApp mainMenu];
    if (!mainMenu) return;
    for (NSMenuItem *topItem in [mainMenu itemArray]) {
        NSMenu *submenu = [topItem submenu];
        if (!submenu) continue;
        for (NSMenuItem *item in [submenu itemArray]) {
            if ([item action] == sel) {
                [item setKeyEquivalent:@""];
                [item setKeyEquivalentModifierMask:0];
            }
        }
    }
}

static void zt_macos_create_menu(void) {
    NSString *appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
    if ([appName length] == 0) appName = [[NSProcessInfo processInfo] processName];

    g_zt_menu_target = [[ZTMenuTarget alloc] init];

    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@""];

    // --- zTracker (application menu) ---
    NSMenu *appMenu = zt_add_submenu(mainMenu, appName);
    // About -> in-app About page (Alt+F12). Shown as Cmd-equivalent ⌥F12.
    zt_add_key_item(appMenu, [NSString stringWithFormat:@"About %@", appName],
                    SDLK_F12, SDL_KMOD_ALT, SDL_SCANCODE_F12,
                    FN_KEY(12), NSEventModifierFlagOption);
    [appMenu addItem:[NSMenuItem separatorItem]];
    // Main Menu -> in-app Main Menu page (ESC). Click-only: ESC is too
    // context-dependent (closes popups, cancels edits) to claim as a shortcut.
    zt_add_key_item(appMenu, @"Main Menu", SDLK_ESCAPE, SDL_KMOD_NONE, 0, @"", 0);
    [appMenu addItem:[NSMenuItem separatorItem]];
    // Quit: click-only. Cmd-Q is reserved for Pattern Editor Transpose-Up.
    [appMenu addItemWithTitle:[NSString stringWithFormat:@"Quit %@", appName]
                       action:@selector(terminate:)
                keyEquivalent:@""];

    // --- File ---
    NSMenu *fileMenu = zt_add_submenu(mainMenu, @"File");
    zt_add_key_item(fileMenu, @"New",  SDLK_N, SDL_KMOD_ALT,  SDL_SCANCODE_N, @"n", NSEventModifierFlagCommand);
    zt_add_key_item(fileMenu, @"Load", SDLK_L, SDL_KMOD_CTRL, SDL_SCANCODE_L, @"l", NSEventModifierFlagControl);
    zt_add_key_item(fileMenu, @"Save", SDLK_S, SDL_KMOD_CTRL, SDL_SCANCODE_S, @"s", NSEventModifierFlagControl);

    // --- View ---
    NSMenu *viewMenu = zt_add_submenu(mainMenu, @"View");
    NSMenu *sizeMenu = zt_add_submenu(viewMenu, @"Size");
    for (int i = 0, n = zt_view_size_preset_count(); i < n; i++) {
        zt_add_size_item(sizeMenu,
                         [NSString stringWithUTF8String:zt_view_size_preset_label(i)], i);
    }
    NSMenu *zoomMenu = zt_add_submenu(viewMenu, @"Zoom");
    zt_add_zoom_item(zoomMenu, @"1.0x",  1.0f);
    zt_add_zoom_item(zoomMenu, @"1.5x",  1.5f);
    zt_add_zoom_item(zoomMenu, @"1.75x", 1.75f);
    zt_add_zoom_item(zoomMenu, @"2.0x",  2.0f);
    zt_add_zoom_item(zoomMenu, @"3.0x",  3.0f);
    // Fullscreen -> Alt+Enter (attempt_fullscreen_toggle, main.cpp).
    [viewMenu addItem:[NSMenuItem separatorItem]];
    zt_add_key_item(viewMenu, @"Fullscreen", SDLK_RETURN, SDL_KMOD_ALT, 0, @"\r", NSEventModifierFlagOption);
    g_zt_view_delegate = [[ZTViewMenuDelegate alloc] init];
    viewMenu.delegate = g_zt_view_delegate;

    // --- Song ---
    NSMenu *songMenu = zt_add_submenu(mainMenu, @"Song");
    zt_add_key_item(songMenu, @"Pattern Editor",    SDLK_F2, SDL_KMOD_NONE,  0, FN_KEY(2), 0);
    zt_add_key_item(songMenu, @"Instrument Editor", SDLK_F3, SDL_KMOD_NONE,  0, FN_KEY(3), 0);
    [songMenu addItem:[NSMenuItem separatorItem]];
    zt_add_key_item(songMenu, @"MIDI Macro Editor", SDLK_F4, SDL_KMOD_NONE,  0, FN_KEY(4), 0);
    zt_add_key_item(songMenu, @"Arpeggio Editor",   SDLK_F4, SDL_KMOD_SHIFT, 0, FN_KEY(4), NSEventModifierFlagShift);
    [songMenu addItem:[NSMenuItem separatorItem]];
    zt_add_key_item(songMenu, @"Song Configuration", SDLK_F11, SDL_KMOD_NONE, 0, FN_KEY(11), 0);

    // --- Playback ---
    NSMenu *playMenu = zt_add_submenu(mainMenu, @"Playback");
    zt_add_key_item(playMenu, @"Play Song",    SDLK_F5, SDL_KMOD_NONE, 0, FN_KEY(5), 0);
    zt_add_key_item(playMenu, @"Play Pattern", SDLK_F6, SDL_KMOD_NONE, 0, FN_KEY(6), 0);
    zt_add_key_item(playMenu, @"Stop",         SDLK_F8, SDL_KMOD_NONE, 0, FN_KEY(8), 0);

    // --- Settings ---
    NSMenu *setMenu = zt_add_submenu(mainMenu, @"Settings");
    zt_add_key_item(setMenu, @"System Configuration", SDLK_F12, SDL_KMOD_NONE, 0, FN_KEY(12), 0);
    zt_add_key_item(setMenu, @"Global Configuration", SDLK_F12, SDL_KMOD_CTRL, 0, FN_KEY(12), NSEventModifierFlagControl);

    // --- Window (standard; AppKit fills in the window list) ---
    NSMenu *windowMenu = zt_add_submenu(mainMenu, @"Window");
    NSMenuItem *minItem = [[NSMenuItem alloc] initWithTitle:@"Minimize"
                                                     action:@selector(performMiniaturize:)
                                              keyEquivalent:@"m"];
    [minItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
    [windowMenu addItem:minItem];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

    // --- Help ---
    NSMenu *helpMenu = zt_add_submenu(mainMenu, @"Help");
    zt_add_key_item(helpMenu, @"Help", SDLK_F1, SDL_KMOD_NONE, 0, FN_KEY(1), 0);

    [NSApp setMainMenu:mainMenu];
    [NSApp setWindowsMenu:windowMenu];
}

extern "C" void zt_macos_menu_init(void) {

    zt_macos_create_menu();

    // Belt-and-suspenders: clear Cmd-Q / Cmd-W from any standard item AppKit
    // may have attached, so those chords reach the app instead of the menu.
    zt_strip_key_equivalent_for_selector(@selector(terminate:));
    zt_strip_key_equivalent_for_selector(@selector(performClose:));
}
