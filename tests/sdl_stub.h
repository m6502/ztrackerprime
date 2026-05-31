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
#define SDL_KMOD_MODE   0x4000  // AltGr (Mode) key, matches SDL3

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
#define SDLK_L ((unsigned int)0x6C)
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

// SDL3 physical scancodes (real values from SDL_scancode.h) — used by the
// keyjazz_map test so the shared note table can be exercised SDL-free.
#define SDL_SCANCODE_A 4
#define SDL_SCANCODE_B 5
#define SDL_SCANCODE_C 6
#define SDL_SCANCODE_D 7
#define SDL_SCANCODE_E 8
#define SDL_SCANCODE_F 9
#define SDL_SCANCODE_G 10
#define SDL_SCANCODE_H 11
#define SDL_SCANCODE_I 12
#define SDL_SCANCODE_J 13
#define SDL_SCANCODE_K 14
#define SDL_SCANCODE_L 15
#define SDL_SCANCODE_M 16
#define SDL_SCANCODE_N 17
#define SDL_SCANCODE_O 18
#define SDL_SCANCODE_P 19
#define SDL_SCANCODE_Q 20
#define SDL_SCANCODE_R 21
#define SDL_SCANCODE_S 22
#define SDL_SCANCODE_T 23
#define SDL_SCANCODE_U 24
#define SDL_SCANCODE_V 25
#define SDL_SCANCODE_W 26
#define SDL_SCANCODE_X 27
#define SDL_SCANCODE_Y 28
#define SDL_SCANCODE_Z 29
#define SDL_SCANCODE_1 30
#define SDL_SCANCODE_2 31
#define SDL_SCANCODE_3 32
#define SDL_SCANCODE_4 33
#define SDL_SCANCODE_5 34
#define SDL_SCANCODE_6 35
#define SDL_SCANCODE_7 36
#define SDL_SCANCODE_8 37
#define SDL_SCANCODE_9 38
#define SDL_SCANCODE_0 39
#define SDL_SCANCODE_SEMICOLON  51
#define SDL_SCANCODE_APOSTROPHE 52
#define SDL_SCANCODE_GRAVE      53

#endif // ZT_SDL_STUB_H
