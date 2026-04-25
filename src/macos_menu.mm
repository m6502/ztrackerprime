// macos_menu.mm
//
// Strip the Cmd-Q key equivalent from the auto-created NSApp menu so that
// Cmd-Q can be used as a Pattern Editor Transpose-Up shortcut (Cmd is
// treated as Alt-equivalent via KS_HAS_ALT). Quit is still reachable from
// the keyboard via Ctrl-Q (or Ctrl-Alt-Q) and from the macOS app-menu
// click — only the keyboard shortcut on the menu item is cleared.

#import <Cocoa/Cocoa.h>

extern "C" void zt_macos_disable_cmd_q(void)
{
    NSMenu *mainMenu = [NSApp mainMenu];
    if (!mainMenu) return;
    for (NSMenuItem *topItem in [mainMenu itemArray]) {
        NSMenu *submenu = [topItem submenu];
        if (!submenu) continue;
        for (NSMenuItem *item in [submenu itemArray]) {
            // Identify the Quit item by selector (locale-independent).
            if ([item action] == @selector(terminate:)) {
                [item setKeyEquivalent:@""];
                [item setKeyEquivalentModifierMask:0];
            }
        }
    }
}
