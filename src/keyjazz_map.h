// keyjazz_map.h
//
// Single source of truth for the computer-keyboard -> note layout used by
// every keyjazz / note-entry path in zTracker:
//   * Pattern Editor cell entry          (CUI_Patterneditor.cpp, T_NOTE)
//   * Pattern Editor draw-mode audition  (CUI_Patterneditor.cpp)
//   * Instrument Editor audition         (CUI_InstEditor.cpp)
//   * Arpeggio Editor keyjazz slot       (CUI_Arpeggioeditor.cpp)
//
// Before this header existed, the layout was hand-copied as four separate
// switch statements. Adding a second layout meant editing four copies -- the
// exact "fix the widget, not every caller" foot-gun CLAUDE.md warns about. Now
// all four call keyjazz_offset() and the layout is one branch.
//
// The function returns a semitone OFFSET relative to (12 * base_octave). The
// caller adds the base itself:
//
//     int off = keyjazz_offset(scancode, layout);
//     if (off != KJ_NOT_A_NOTE) { int note = 12*base_octave + off; ... }
//
// Two layouts:
//
//   KJ_TRACKER  -- the classic Impulse / Fast Tracker two-row layout (default,
//                  byte-for-byte what zTracker always did):
//                    top row    Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P  ->   0..16
//                    bottom row Z S X D C V G B H N J M            -> -12..-1
//
//   KJ_PIANO    -- the Ableton Live / Logic "Musical Typing" layout, so users
//                  who keyjazz in those DAWs keep their muscle memory. The home
//                  row is the white keys, the row above is the black keys -- one
//                  piano octave climbing rightward, extended through ' :
//                    white  A S D F G H J K L ; '  ->  0 2 4 5 7 9 11 12 14 16 17
//                    black  W E   T Y U   O P      ->  1 3   6 8 10  13 15
//                  i.e. exactly the "awsedftgyhujk" run the user asked for
//                  (a=C w=C# s=D e=D# d=E f=F t=F# g=G y=G# h=A u=A# j=B k=C).
//                  Z/X/C/V are deliberately NOT note keys here: in Logic/Live
//                  they are octave-shift (Z/X) and velocity (C/V) controls.
//                  zTracker keeps its own base_octave controls; those keys are
//                  simply left free in piano mode.
//
// SDL-free in spirit: this header names SDL_SCANCODE_* tokens but does not
// include SDL itself. Production translation units already include SDL; the
// unit test (tests/test_keyjazz_map.cpp) pulls in the handful of SDL_SCANCODE_*
// values it needs from tests/sdl_stub.h.

#ifndef ZT_KEYJAZZ_MAP_H
#define ZT_KEYJAZZ_MAP_H

enum KeyjazzLayout { KJ_TRACKER = 0, KJ_PIANO = 1 };

// Returned for any key that is not a musical note key in the given layout.
// Matches the Arpeggio editor's historical -127 sentinel so behaviour is
// identical after the callers are repointed at this function.
static const int KJ_NOT_A_NOTE = -127;

static inline int keyjazz_offset(int scancode, KeyjazzLayout layout) {
    if (layout == KJ_PIANO) {
        switch (scancode) {
            // White keys -- home row (C D E F G A B C D E F)
            case SDL_SCANCODE_A:          return 0;   // C
            case SDL_SCANCODE_S:          return 2;   // D
            case SDL_SCANCODE_D:          return 4;   // E
            case SDL_SCANCODE_F:          return 5;   // F
            case SDL_SCANCODE_G:          return 7;   // G
            case SDL_SCANCODE_H:          return 9;   // A
            case SDL_SCANCODE_J:          return 11;  // B
            case SDL_SCANCODE_K:          return 12;  // C  (octave up)
            case SDL_SCANCODE_L:          return 14;  // D
            case SDL_SCANCODE_SEMICOLON:  return 16;  // E
            case SDL_SCANCODE_APOSTROPHE: return 17;  // F
            // Black keys -- upper row (C# D# . F# G# A# . C# D#)
            case SDL_SCANCODE_W:          return 1;   // C#
            case SDL_SCANCODE_E:          return 3;   // D#
            case SDL_SCANCODE_T:          return 6;   // F#
            case SDL_SCANCODE_Y:          return 8;   // G#
            case SDL_SCANCODE_U:          return 10;  // A#
            case SDL_SCANCODE_O:          return 13;  // C# (octave up)
            case SDL_SCANCODE_P:          return 15;  // D#
            default:                      return KJ_NOT_A_NOTE;
        }
    }

    // KJ_TRACKER (default)
    switch (scancode) {
        // Top row -- upper octave (offsets 0..16 from 12*base_octave)
        case SDL_SCANCODE_Q: return 0;
        case SDL_SCANCODE_2: return 1;
        case SDL_SCANCODE_W: return 2;
        case SDL_SCANCODE_3: return 3;
        case SDL_SCANCODE_E: return 4;
        case SDL_SCANCODE_R: return 5;
        case SDL_SCANCODE_5: return 6;
        case SDL_SCANCODE_T: return 7;
        case SDL_SCANCODE_6: return 8;
        case SDL_SCANCODE_Y: return 9;
        case SDL_SCANCODE_7: return 10;
        case SDL_SCANCODE_U: return 11;
        case SDL_SCANCODE_I: return 12;
        case SDL_SCANCODE_9: return 13;
        case SDL_SCANCODE_O: return 14;
        case SDL_SCANCODE_0: return 15;
        case SDL_SCANCODE_P: return 16;
        // Bottom row -- lower octave (offsets -12..-1; 12*(base_octave-1)+N)
        case SDL_SCANCODE_Z: return -12;
        case SDL_SCANCODE_S: return -11;
        case SDL_SCANCODE_X: return -10;
        case SDL_SCANCODE_D: return -9;
        case SDL_SCANCODE_C: return -8;
        case SDL_SCANCODE_V: return -7;
        case SDL_SCANCODE_G: return -6;
        case SDL_SCANCODE_B: return -5;
        case SDL_SCANCODE_H: return -4;
        case SDL_SCANCODE_N: return -3;
        case SDL_SCANCODE_J: return -2;
        case SDL_SCANCODE_M: return -1;
        default:             return KJ_NOT_A_NOTE;
    }
}

#endif // ZT_KEYJAZZ_MAP_H
