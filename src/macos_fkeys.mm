// macos_fkeys.mm — macOS-only. Capture aux-control key events
// (NSEventTypeSystemDefined, subtype 8) delivered to the frontmost
// app when the System Settings toggle "Use F1, F2, etc. keys as
// standard function keys" is OFF. Map them to synthesized SDL
// F-key events so zTracker's F1..F12 bindings work regardless of
// that setting.
//
// Scope and limitations:
//   * F3 (Mission Control) and F4 (Launchpad/Spotlight) are
//     symbolic hotkeys handled by WindowServer before they reach
//     the app. They cannot be captured via NSEvent and are not
//     handled here. User can disable those shortcuts in
//     System Settings > Keyboard > Keyboard Shortcuts if desired.
//   * Because this uses a local event monitor (no Accessibility
//     permission), the OS media daemon will still perform its
//     default action (volume/brightness change) in addition to the
//     zTracker F-key handler firing. That is the expected tradeoff
//     for avoiding a CGEventTap + Accessibility prompt.
//   * The monitor is local: it only fires while zt.app is
//     frontmost, which is the desired behavior.

#import <AppKit/AppKit.h>
#import <IOKit/hidsystem/ev_keymap.h>
#include <SDL3/SDL.h>
#include <cstring>

void keyhandler(SDL_KeyboardEvent *e);

static id g_fkey_monitor = nil;

static SDL_Keycode aux_to_fkey(int auxKey) {
  switch (auxKey) {
    case NX_KEYTYPE_BRIGHTNESS_DOWN:   return SDLK_F1;
    case NX_KEYTYPE_BRIGHTNESS_UP:     return SDLK_F2;
    case NX_KEYTYPE_ILLUMINATION_DOWN: return SDLK_F5;
    case NX_KEYTYPE_ILLUMINATION_UP:   return SDLK_F6;
    case NX_KEYTYPE_PREVIOUS:          return SDLK_F7;
    case NX_KEYTYPE_PLAY:              return SDLK_F8;
    case NX_KEYTYPE_NEXT:              return SDLK_F9;
    case NX_KEYTYPE_MUTE:              return SDLK_F10;
    case NX_KEYTYPE_SOUND_DOWN:        return SDLK_F11;
    case NX_KEYTYPE_SOUND_UP:          return SDLK_F12;
    default:                           return SDLK_UNKNOWN;
  }
}

static SDL_Scancode fkey_to_scancode(SDL_Keycode k) {
  switch (k) {
    case SDLK_F1:  return SDL_SCANCODE_F1;
    case SDLK_F2:  return SDL_SCANCODE_F2;
    case SDLK_F5:  return SDL_SCANCODE_F5;
    case SDLK_F6:  return SDL_SCANCODE_F6;
    case SDLK_F7:  return SDL_SCANCODE_F7;
    case SDLK_F8:  return SDL_SCANCODE_F8;
    case SDLK_F9:  return SDL_SCANCODE_F9;
    case SDLK_F10: return SDL_SCANCODE_F10;
    case SDLK_F11: return SDL_SCANCODE_F11;
    case SDLK_F12: return SDL_SCANCODE_F12;
    default:       return SDL_SCANCODE_UNKNOWN;
  }
}

void zt_macos_install_fkey_monitor(void) {
  if (g_fkey_monitor) return;
  g_fkey_monitor = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSEventMaskSystemDefined
      handler:^NSEvent * _Nullable (NSEvent * _Nonnull ev) {
    if ([ev subtype] != 8) return ev;

    int data1    = (int)[ev data1];
    int auxKey   = (data1 & 0xFFFF0000) >> 16;
    int keyFlags = (data1 & 0x0000FFFF);
    int keyState = (keyFlags & 0xFF00) >> 8;  // 0x0A=down, 0x0B=up
    bool isDown  = (keyState == 0x0A);
    bool isUp    = (keyState == 0x0B);
    if (!isDown && !isUp) return ev;

    SDL_Keycode k = aux_to_fkey(auxKey);
    if (k == SDLK_UNKNOWN) return ev;

    NSEventModifierFlags mflags = [ev modifierFlags];
    SDL_Keymod mod = SDL_KMOD_NONE;
    if (mflags & NSEventModifierFlagShift)   mod = (SDL_Keymod)(mod | SDL_KMOD_SHIFT);
    if (mflags & NSEventModifierFlagControl) mod = (SDL_Keymod)(mod | SDL_KMOD_CTRL);
    if (mflags & NSEventModifierFlagOption)  mod = (SDL_Keymod)(mod | SDL_KMOD_ALT);
    if (mflags & NSEventModifierFlagCommand) mod = (SDL_Keymod)(mod | SDL_KMOD_GUI);

    SDL_KeyboardEvent k_ev;
    std::memset(&k_ev, 0, sizeof(k_ev));
    k_ev.type     = isDown ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    k_ev.key      = k;
    k_ev.scancode = fkey_to_scancode(k);
    k_ev.mod      = mod;
    k_ev.down     = isDown;
    k_ev.repeat   = false;
    keyhandler(&k_ev);
    return nil;
  }];
}

void zt_macos_remove_fkey_monitor(void) {
  if (g_fkey_monitor) {
    [NSEvent removeMonitor:g_fkey_monitor];
    g_fkey_monitor = nil;
  }
}
