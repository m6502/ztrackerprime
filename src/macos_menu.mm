// macos_menu.mm
//
// Strip the Cmd-Q key equivalent from the auto-created NSApp menu so that
// Cmd-Q can be used as a Pattern Editor Transpose-Up shortcut (Cmd is
// treated as Alt-equivalent via KS_HAS_ALT). Quit is still reachable from
// the keyboard via Ctrl-Q (or Ctrl-Alt-Q) and from the macOS app-menu
// click — only the keyboard shortcut on the menu item is cleared.
//
// Same treatment for Cmd-W: the Window menu's Close item would otherwise
// dismiss the SDL window instead of letting Cmd-W reach the app. The user
// expects Cmd-W to behave like Ctrl-W (clear unused volumes in selection).

#import <Cocoa/Cocoa.h>

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

extern "C" void zt_macos_disable_cmd_q(void)
{
    zt_strip_key_equivalent_for_selector(@selector(terminate:));
}

extern "C" void zt_macos_disable_cmd_w(void)
{
    // performClose: is the standard "Close Window" action.
    zt_strip_key_equivalent_for_selector(@selector(performClose:));
}
