// sdl_stub.h
//
// Tiny stand-in for <SDL.h> sufficient to compile keybuffer.{h,cpp} and
// any other test target that only needs the modifier-mask constants and
// keypad/letter SDLK_* values.
//
// Tests include THIS instead of <SDL.h> by relying on include order +
// the ZT_TEST_NO_SDL guard inside keybuffer.h.

#ifndef ZT_SDL_STUB_H
#define ZT_SDL_STUB_H

#include <cstdint>

// SDL3 keymod bitmask values (taken directly from SDL_keycode.h so that
// keybuffer.cpp's bit logic produces identical results to a real build).
typedef uint16_t SDL_Keymod;

#define SDL_KMOD_NONE   0x0000
#define SDL_KMOD_LSHIFT 0x0001
#define SDL_KMOD_RSHIFT 0x0002
#define SDL_KMOD_SHIFT  (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)
#define SDL_KMOD_LCTRL  0x0040
#define SDL_KMOD_RCTRL  0x0080
#define SDL_KMOD_CTRL   (SDL_KMOD_LCTRL  | SDL_KMOD_RCTRL)
#define SDL_KMOD_LALT   0x0100
#define SDL_KMOD_RALT   0x0200
#define SDL_KMOD_ALT    (SDL_KMOD_LALT   | SDL_KMOD_RALT)
#define SDL_KMOD_LGUI   0x0400
#define SDL_KMOD_RGUI   0x0800
#define SDL_KMOD_GUI    (SDL_KMOD_LGUI   | SDL_KMOD_RGUI)

// SDLK_* values used by keybuffer.cpp's keypad-to-digit normalisation.
// Real SDL3 values are 0x40000059..0x40000062 for KP_0..KP_9; for the
// stub the only requirement is that they're distinct from each other
// AND from the digit row.
#define SDLK_0 ((unsigned int)0x30)
#define SDLK_1 ((unsigned int)0x31)
#define SDLK_2 ((unsigned int)0x32)
#define SDLK_3 ((unsigned int)0x33)
#define SDLK_4 ((unsigned int)0x34)
#define SDLK_5 ((unsigned int)0x35)
#define SDLK_6 ((unsigned int)0x36)
#define SDLK_7 ((unsigned int)0x37)
#define SDLK_8 ((unsigned int)0x38)
#define SDLK_9 ((unsigned int)0x39)

#define SDLK_KP_0 ((unsigned int)0x40000059)
#define SDLK_KP_1 ((unsigned int)0x4000005A)
#define SDLK_KP_2 ((unsigned int)0x4000005B)
#define SDLK_KP_3 ((unsigned int)0x4000005C)
#define SDLK_KP_4 ((unsigned int)0x4000005D)
#define SDLK_KP_5 ((unsigned int)0x4000005E)
#define SDLK_KP_6 ((unsigned int)0x4000005F)
#define SDLK_KP_7 ((unsigned int)0x40000060)
#define SDLK_KP_8 ((unsigned int)0x40000061)
#define SDLK_KP_9 ((unsigned int)0x40000062)

// Other letter keys we pass to ListBox-style typeahead in tests.
#define SDLK_A ((unsigned int)0x61)
#define SDLK_B ((unsigned int)0x62)
#define SDLK_P ((unsigned int)0x70)
#define SDLK_S ((unsigned int)0x73)
#define SDLK_W ((unsigned int)0x77)
#define SDLK_Q ((unsigned int)0x71)

#define SDLK_UP        ((unsigned int)0x40000052)
#define SDLK_DOWN      ((unsigned int)0x40000051)
#define SDLK_LEFT      ((unsigned int)0x40000050)
#define SDLK_RIGHT     ((unsigned int)0x4000004F)
#define SDLK_RETURN    ((unsigned int)0x0D)
#define SDLK_SPACE     ((unsigned int)0x20)
#define SDLK_ESCAPE    ((unsigned int)0x1B)
#define SDLK_TAB       ((unsigned int)0x09)
#define SDLK_BACKSPACE ((unsigned int)0x08)
#define SDLK_DELETE    ((unsigned int)0x7F)
#define SDLK_HOME      ((unsigned int)0x40000054)
#define SDLK_END       ((unsigned int)0x40000055)
#define SDLK_PAGEUP    ((unsigned int)0x40000056)
#define SDLK_PAGEDOWN  ((unsigned int)0x40000057)

#endif // ZT_SDL_STUB_H
