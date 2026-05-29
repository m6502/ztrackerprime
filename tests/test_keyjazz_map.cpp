// test_keyjazz_map.cpp -- tests for the shared keyjazz note table.
//
// keyjazz_map.h is the single source of truth for the computer-keyboard ->
// note layout used by the Pattern Editor (cell entry + draw-mode audition),
// the Instrument Editor, and the Arpeggio Editor. Two layouts:
//
//   KJ_TRACKER  classic Impulse/Fast Tracker two-row layout (default)
//   KJ_PIANO    Ableton Live / Logic "Musical Typing" piano layout
//
// The KJ_TRACKER cases are the regression guard for the 2026-05 refactor that
// collapsed four hand-copied switches into this one function: they must match
// the legacy tables byte-for-byte. The KJ_PIANO cases lock in the requested
// "awsedftgyhujk" run and the mutual-exclusivity of the two layouts.

#include "sdl_stub.h"      // SDL_SCANCODE_* without linking SDL
#include "keyjazz_map.h"

#include <cstdio>

static int g_failures = 0;
static int g_total    = 0;

#define CHECK_EQ(a, b) do {                                    \
    ++g_total;                                                 \
    auto _av = (a);                                            \
    auto _bv = (b);                                            \
    if (!(_av == _bv)) {                                       \
        ++g_failures;                                          \
        std::fprintf(stderr, "FAIL  %s:%d  %s == %s "          \
                     "(got %lld vs %lld)\n",                   \
                     __FILE__, __LINE__, #a, #b,               \
                     (long long)_av, (long long)_bv);          \
    }                                                          \
} while (0)

static int trk(int sc) { return keyjazz_offset(sc, KJ_TRACKER); }
static int pno(int sc) { return keyjazz_offset(sc, KJ_PIANO);   }

int main() {
    // ---- Sentinel value contract -----------------------------------------
    // The Arpeggio editor's callers test `off != -127`; keep it pinned.
    CHECK_EQ(KJ_NOT_A_NOTE, -127);

    // ---- KJ_TRACKER: top row (upper octave, offsets 0..16) ---------------
    CHECK_EQ(trk(SDL_SCANCODE_Q), 0);
    CHECK_EQ(trk(SDL_SCANCODE_2), 1);
    CHECK_EQ(trk(SDL_SCANCODE_W), 2);
    CHECK_EQ(trk(SDL_SCANCODE_3), 3);
    CHECK_EQ(trk(SDL_SCANCODE_E), 4);
    CHECK_EQ(trk(SDL_SCANCODE_R), 5);
    CHECK_EQ(trk(SDL_SCANCODE_5), 6);
    CHECK_EQ(trk(SDL_SCANCODE_T), 7);
    CHECK_EQ(trk(SDL_SCANCODE_6), 8);
    CHECK_EQ(trk(SDL_SCANCODE_Y), 9);
    CHECK_EQ(trk(SDL_SCANCODE_7), 10);
    CHECK_EQ(trk(SDL_SCANCODE_U), 11);
    CHECK_EQ(trk(SDL_SCANCODE_I), 12);
    CHECK_EQ(trk(SDL_SCANCODE_9), 13);
    CHECK_EQ(trk(SDL_SCANCODE_O), 14);
    CHECK_EQ(trk(SDL_SCANCODE_0), 15);
    CHECK_EQ(trk(SDL_SCANCODE_P), 16);

    // ---- KJ_TRACKER: bottom row (lower octave, offsets -12..-1) ----------
    CHECK_EQ(trk(SDL_SCANCODE_Z), -12);
    CHECK_EQ(trk(SDL_SCANCODE_S), -11);
    CHECK_EQ(trk(SDL_SCANCODE_X), -10);
    CHECK_EQ(trk(SDL_SCANCODE_D), -9);
    CHECK_EQ(trk(SDL_SCANCODE_C), -8);
    CHECK_EQ(trk(SDL_SCANCODE_V), -7);
    CHECK_EQ(trk(SDL_SCANCODE_G), -6);
    CHECK_EQ(trk(SDL_SCANCODE_B), -5);
    CHECK_EQ(trk(SDL_SCANCODE_H), -4);
    CHECK_EQ(trk(SDL_SCANCODE_N), -3);
    CHECK_EQ(trk(SDL_SCANCODE_J), -2);
    CHECK_EQ(trk(SDL_SCANCODE_M), -1);

    // ---- KJ_TRACKER: keys with no note are sentinel ----------------------
    CHECK_EQ(trk(SDL_SCANCODE_A), KJ_NOT_A_NOTE);  // unused in tracker layout
    CHECK_EQ(trk(SDL_SCANCODE_K), KJ_NOT_A_NOTE);
    CHECK_EQ(trk(SDL_SCANCODE_L), KJ_NOT_A_NOTE);
    CHECK_EQ(trk(SDL_SCANCODE_F), KJ_NOT_A_NOTE);
    CHECK_EQ(trk(SDL_SCANCODE_1), KJ_NOT_A_NOTE);  // Note Off lives in the editor switch
    CHECK_EQ(trk(SDL_SCANCODE_4), KJ_NOT_A_NOTE);  // peek-play
    CHECK_EQ(trk(SDL_SCANCODE_8), KJ_NOT_A_NOTE);  // peek-play row
    CHECK_EQ(trk(SDL_SCANCODE_GRAVE), KJ_NOT_A_NOTE);

    // ---- KJ_PIANO: the exact "awsedftgyhujk" run = chromatic C..C --------
    CHECK_EQ(pno(SDL_SCANCODE_A), 0);   // C
    CHECK_EQ(pno(SDL_SCANCODE_W), 1);   // C#
    CHECK_EQ(pno(SDL_SCANCODE_S), 2);   // D
    CHECK_EQ(pno(SDL_SCANCODE_E), 3);   // D#
    CHECK_EQ(pno(SDL_SCANCODE_D), 4);   // E
    CHECK_EQ(pno(SDL_SCANCODE_F), 5);   // F
    CHECK_EQ(pno(SDL_SCANCODE_T), 6);   // F#
    CHECK_EQ(pno(SDL_SCANCODE_G), 7);   // G
    CHECK_EQ(pno(SDL_SCANCODE_Y), 8);   // G#
    CHECK_EQ(pno(SDL_SCANCODE_H), 9);   // A
    CHECK_EQ(pno(SDL_SCANCODE_U), 10);  // A#
    CHECK_EQ(pno(SDL_SCANCODE_J), 11);  // B
    CHECK_EQ(pno(SDL_SCANCODE_K), 12);  // C

    // ---- KJ_PIANO: the extended upper keys (L ; ' and O P) ---------------
    CHECK_EQ(pno(SDL_SCANCODE_O), 13);          // C#
    CHECK_EQ(pno(SDL_SCANCODE_L), 14);          // D
    CHECK_EQ(pno(SDL_SCANCODE_P), 15);          // D#
    CHECK_EQ(pno(SDL_SCANCODE_SEMICOLON), 16);  // E
    CHECK_EQ(pno(SDL_SCANCODE_APOSTROPHE), 17); // F

    // ---- KJ_PIANO: Z/X/C/V are NOT notes (Logic reserves them) -----------
    CHECK_EQ(pno(SDL_SCANCODE_Z), KJ_NOT_A_NOTE);  // octave shift in Logic/Live
    CHECK_EQ(pno(SDL_SCANCODE_X), KJ_NOT_A_NOTE);
    CHECK_EQ(pno(SDL_SCANCODE_C), KJ_NOT_A_NOTE);  // velocity in Logic/Live
    CHECK_EQ(pno(SDL_SCANCODE_V), KJ_NOT_A_NOTE);
    CHECK_EQ(pno(SDL_SCANCODE_B), KJ_NOT_A_NOTE);
    CHECK_EQ(pno(SDL_SCANCODE_N), KJ_NOT_A_NOTE);
    CHECK_EQ(pno(SDL_SCANCODE_M), KJ_NOT_A_NOTE);
    CHECK_EQ(pno(SDL_SCANCODE_Q), KJ_NOT_A_NOTE);  // top row not used in piano
    CHECK_EQ(pno(SDL_SCANCODE_R), KJ_NOT_A_NOTE);
    CHECK_EQ(pno(SDL_SCANCODE_I), KJ_NOT_A_NOTE);
    CHECK_EQ(pno(SDL_SCANCODE_2), KJ_NOT_A_NOTE);  // no number keys in piano

    // ---- Mutual exclusivity: shared keys mean different notes ------------
    // These are exactly why the two layouts cannot coexist on one keymap and
    // why the feature is a mode toggle, not an additive binding.
    CHECK_EQ(trk(SDL_SCANCODE_S), -11);  // tracker: C#-ish low
    CHECK_EQ(pno(SDL_SCANCODE_S), 2);    // piano:   D
    CHECK_EQ(trk(SDL_SCANCODE_W), 2);    // tracker: D (upper)
    CHECK_EQ(pno(SDL_SCANCODE_W), 1);    // piano:   C#
    CHECK_EQ(trk(SDL_SCANCODE_E), 4);    // tracker: E
    CHECK_EQ(pno(SDL_SCANCODE_E), 3);    // piano:   D#

    if (g_failures == 0)
        std::printf("test_keyjazz_map: all %d checks passed\n", g_total);
    else
        std::fprintf(stderr, "test_keyjazz_map: %d/%d checks FAILED\n",
                     g_failures, g_total);
    return g_failures ? 1 : 0;
}
