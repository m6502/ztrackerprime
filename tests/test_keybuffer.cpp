// test_keybuffer.cpp -- exercises the actual KeyBuffer class from
// src/keybuffer.{h,cpp}, not a mock or simulator.
//
// We bypass SDL3 with sdl_stub.h + the ZT_TEST_NO_SDL guard added to
// keybuffer.h. Everything that actually executes -- FIFO ordering, the
// SDL_KMOD_* -> KS_* translation, the macOS Cmd -> KS_META|KS_ALT
// mapping, the keypad-to-digit normalisation, the size cap -- runs the
// real code path.
//
// Locks down the contract that the listbox / preset selectors / global
// keyhandler depend on.

// ZT_TEST_NO_SDL is set via tests/CMakeLists.txt's
// target_compile_definitions(test_keybuffer PRIVATE ZT_TEST_NO_SDL).
// keybuffer.h reads that define and pulls in tests/sdl_stub.h instead
// of the real <SDL.h>.
#include "keybuffer.h"

#include <cstdio>

static int g_failures = 0;
static int g_total    = 0;

#define CHECK(expr) do {                                       \
    ++g_total;                                                 \
    if (!(expr)) {                                             \
        ++g_failures;                                          \
        std::fprintf(stderr, "FAIL  %s:%d  %s\n",              \
                     __FILE__, __LINE__, #expr);               \
    }                                                          \
} while (0)

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

// ---------------------------------------------------------------------------
// Empty-buffer behaviour: checkkey() / getkey() return 0 when nothing
// is pending, and don't underflow the cursize counter.

static void test_empty_returns_zero() {
    KeyBuffer kb;
    CHECK_EQ((int)kb.checkkey(), 0);
    CHECK_EQ((int)kb.getkey(),   0);
    CHECK_EQ((int)kb.size(),     0);
}

static void test_empty_after_double_getkey() {
    KeyBuffer kb;
    (void)kb.getkey();
    (void)kb.getkey();
    CHECK_EQ((int)kb.size(), 0);
}

// ---------------------------------------------------------------------------
// Insert + checkkey/getkey: checkkey peeks (idempotent), getkey consumes.
// REGRESSION: the "Ctrl-S froze the page" bug came from a `break` exit
// that left a key in the buffer, so every subsequent checkkey() returned
// the same stuck event. These tests pin the peek-vs-consume contract.

static void test_checkkey_peeks_idempotent() {
    KeyBuffer kb;
    kb.insert(SDLK_S, SDL_KMOD_LCTRL);
    CHECK_EQ((int)kb.size(), 1);
    CHECK_EQ((int)kb.checkkey(), (int)SDLK_S);
    CHECK_EQ((int)kb.checkkey(), (int)SDLK_S);   // peek doesn't consume
    CHECK_EQ((int)kb.checkkey(), (int)SDLK_S);
    CHECK_EQ((int)kb.size(), 1);
}

static void test_getkey_consumes() {
    KeyBuffer kb;
    kb.insert(SDLK_S, SDL_KMOD_LCTRL);
    CHECK_EQ((int)kb.getkey(), (int)SDLK_S);
    CHECK_EQ((int)kb.size(), 0);
    CHECK_EQ((int)kb.checkkey(), 0);
}

// REGRESSION SCENARIO: simulate the global Ctrl-S handler swallowing
// the keypress. Insert Ctrl-S, getkey() it -> buffer empty -> next
// checkkey() returns 0 (no stuck event).
static void test_swallow_pattern_drains_buffer() {
    KeyBuffer kb;
    kb.insert(SDLK_S, SDL_KMOD_LCTRL);
    // Page sees Ctrl-S, decides to swallow:
    (void)kb.getkey();
    // Next frame's checkkey must return 0, not Ctrl-S again.
    CHECK_EQ((int)kb.checkkey(), 0);
    CHECK_EQ((int)kb.size(), 0);
}

// FIFO ordering across several inserts.
static void test_fifo_order() {
    KeyBuffer kb;
    kb.insert(SDLK_A);
    kb.insert(SDLK_B);
    kb.insert(SDLK_P);
    CHECK_EQ((int)kb.size(), 3);
    CHECK_EQ((int)kb.getkey(), (int)SDLK_A);
    CHECK_EQ((int)kb.getkey(), (int)SDLK_B);
    CHECK_EQ((int)kb.getkey(), (int)SDLK_P);
    CHECK_EQ((int)kb.size(), 0);
}

// ---------------------------------------------------------------------------
// SDL_KMOD_* -> KS_* translation. The listbox / preset overrides depend
// on (kstate & KS_CTRL) etc., which is generated here.

static void test_state_ctrl_only() {
    KeyBuffer kb;
    kb.insert(SDLK_S, SDL_KMOD_LCTRL);
    (void)kb.checkkey();
    int s = (int)kb.getstate();
    CHECK(s & KS_CTRL);
    CHECK(!(s & KS_SHIFT));
    CHECK(!(s & KS_ALT));
    CHECK(!(s & KS_META));
}

static void test_state_shift_ctrl() {
    KeyBuffer kb;
    kb.insert(SDLK_S, SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT);
    (void)kb.checkkey();
    int s = (int)kb.getstate();
    CHECK(s & KS_CTRL);
    CHECK(s & KS_SHIFT);
}

static void test_state_alt_alone_no_ctrl() {
    KeyBuffer kb;
    kb.insert(SDLK_W, SDL_KMOD_LALT);
    (void)kb.checkkey();
    int s = (int)kb.getstate();
    CHECK(s & KS_ALT);
    CHECK(!(s & KS_CTRL));
}

#ifdef __APPLE__
// macOS-only: Cmd (SDL_KMOD_GUI) maps to BOTH KS_META and KS_ALT so
// that Cmd+key works wherever Alt+key does. The KS_HAS_ALT macro
// accepts either path. Pin this contract -- editors rely on it for
// Cmd-W (=Alt-W) doing the same as Ctrl-W.
static void test_macos_cmd_maps_to_meta_and_alt() {
    KeyBuffer kb;
    kb.insert(SDLK_W, SDL_KMOD_LGUI);
    (void)kb.checkkey();
    int s = (int)kb.getstate();
    CHECK(s & KS_META);
    CHECK(s & KS_ALT);
    CHECK(KS_HAS_ALT(s));
}

static void test_macos_cmd_shift_does_not_set_ctrl() {
    KeyBuffer kb;
    kb.insert(SDLK_W, SDL_KMOD_LGUI | SDL_KMOD_LSHIFT);
    (void)kb.checkkey();
    int s = (int)kb.getstate();
    CHECK(s & KS_META);
    CHECK(s & KS_ALT);
    CHECK(s & KS_SHIFT);
    CHECK(!(s & KS_CTRL));
    // KS_HAS_ALT requires NO Ctrl AND NO Shift, so shift+cmd should NOT
    // count as "Alt-equivalent" for shortcuts that explicitly need Alt
    // alone.
    CHECK(!KS_HAS_ALT(s));
}
#endif

// ---------------------------------------------------------------------------
// KS_HAS_ALT: the shorthand that lets editors accept Alt or Cmd
// interchangeably. Used by the Pattern Editor's Set-Instrument path
// and the F4 macro/arpeggio editor's preset-cycle.

static void test_ks_has_alt_alt_alone_yes() {
    CHECK(KS_HAS_ALT(KS_ALT));
}
static void test_ks_has_alt_meta_alone_yes() {
    CHECK(KS_HAS_ALT(KS_META));
}
static void test_ks_has_alt_meta_plus_alt_yes() {
    CHECK(KS_HAS_ALT(KS_META | KS_ALT));
}
static void test_ks_has_alt_alt_plus_ctrl_no() {
    // Alt+Ctrl explicitly excluded (would otherwise overlap with
    // Ctrl-only shortcuts).
    CHECK(!KS_HAS_ALT(KS_ALT | KS_CTRL));
}
static void test_ks_has_alt_alt_plus_shift_no() {
    CHECK(!KS_HAS_ALT(KS_ALT | KS_SHIFT));
}
static void test_ks_has_alt_no_alt_no() {
    CHECK(!KS_HAS_ALT(KS_NO_SHIFT_KEYS));
    CHECK(!KS_HAS_ALT(KS_CTRL));
    CHECK(!KS_HAS_ALT(KS_SHIFT));
}

// ---------------------------------------------------------------------------
// Keypad numbers normalise to the digit-row equivalents. The pattern
// editor's Alt+digit Set-Instrument shortcut and the value-slider's
// digit-typing both depend on this.

static void test_kp_normalises_to_digit_row() {
    KeyBuffer kb;
    kb.insert(SDLK_KP_0);
    kb.insert(SDLK_KP_5);
    kb.insert(SDLK_KP_9);
    CHECK_EQ((int)kb.getkey(), (int)SDLK_0);
    CHECK_EQ((int)kb.getkey(), (int)SDLK_5);
    CHECK_EQ((int)kb.getkey(), (int)SDLK_9);
}

// ---------------------------------------------------------------------------
// Buffer overflow: inserts beyond maxsize must be silently dropped, not
// corrupt internal state.

static void test_overflow_drops_extras_no_corruption() {
    KeyBuffer kb;
    for (int i = 0; i < 500; ++i) kb.insert(SDLK_A);
    int sz = (int)kb.size();
    CHECK(sz <= 200);   // maxsize is 200 in keybuffer.cpp
    // Drain everything; getkey must not underflow.
    int drained = 0;
    while (kb.size() > 0) {
        (void)kb.getkey();
        ++drained;
        if (drained > 1000) {
            CHECK(!"size() never reached zero");
            break;
        }
    }
    CHECK_EQ((int)kb.size(), 0);
    CHECK_EQ((int)kb.checkkey(), 0);
}

// flush() empties the buffer in O(1), regardless of pending size.
static void test_flush_empties_buffer() {
    KeyBuffer kb;
    for (int i = 0; i < 50; ++i) kb.insert(SDLK_A);
    CHECK((int)kb.size() > 0);
    kb.flush();
    CHECK_EQ((int)kb.size(), 0);
    CHECK_EQ((int)kb.checkkey(), 0);
}

// ---------------------------------------------------------------------------

int main() {
    test_empty_returns_zero();
    test_empty_after_double_getkey();
    test_checkkey_peeks_idempotent();
    test_getkey_consumes();
    test_swallow_pattern_drains_buffer();
    test_fifo_order();
    test_state_ctrl_only();
    test_state_shift_ctrl();
    test_state_alt_alone_no_ctrl();
#ifdef __APPLE__
    test_macos_cmd_maps_to_meta_and_alt();
    test_macos_cmd_shift_does_not_set_ctrl();
#endif
    test_ks_has_alt_alt_alone_yes();
    test_ks_has_alt_meta_alone_yes();
    test_ks_has_alt_meta_plus_alt_yes();
    test_ks_has_alt_alt_plus_ctrl_no();
    test_ks_has_alt_alt_plus_shift_no();
    test_ks_has_alt_no_alt_no();
    test_kp_normalises_to_digit_row();
    test_overflow_drops_extras_no_corruption();
    test_flush_empties_buffer();

    std::printf("%d/%d checks passed\n", g_total - g_failures, g_total);
    return g_failures == 0 ? 0 : 1;
}
