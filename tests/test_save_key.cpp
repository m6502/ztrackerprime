// test_save_key.cpp -- exhaustive tests for dispatch_save_key().
//
// The "Ctrl-S in F4 freezes the page" bug came from a `break;` exit
// path that didn't consume the keypress. The fix lifted the per-page
// rules into the pure dispatch_save_key() function so:
//   * the rules are visible at a glance,
//   * the SAVE_KEY_SWALLOW return forces callers to consume,
//   * every page's behaviour is enumerated once and tested.

#include "save_key_dispatch.h"

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

static SaveKeyContext ctx() {
    SaveKeyContext c{};
    return c;
}

static void test_no_modifier_passes_through() {
    SaveKeyContext c = ctx();
    c.kstate_ctrl = false;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_PASS_THROUGH);
}

static void test_keybindings_lets_page_handle() {
    SaveKeyContext c = ctx();
    c.kstate_ctrl          = true;
    c.is_keybindings_state = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_LET_PAGE_HANDLE);
}

// REGRESSION: Ctrl-S in F4 (MIDI Macro editor) must SWALLOW. The
// previous bug was the dispatcher returned a default that opened the
// Save Song dialog, which yanked the user out of mid-edit.
static void test_macroedit_ctrl_s_swallows() {
    SaveKeyContext c = ctx();
    c.kstate_ctrl          = true;
    c.is_macroedit_state   = true;
    c.song_has_filename    = true;   // would otherwise pop save popup
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_SWALLOW);
}

// REGRESSION: same for Shift+F4 (Arpeggio editor).
static void test_arpedit_ctrl_s_swallows() {
    SaveKeyContext c = ctx();
    c.kstate_ctrl       = true;
    c.is_arpedit_state  = true;
    c.song_has_filename = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_SWALLOW);
}

// REGRESSION (2026-04-28): On macOS, users instinctively press Cmd-S
// (KS_HAS_ALT, since Cmd maps to KS_META|KS_ALT). The original swallow
// rule only matched KS_CTRL, so Cmd-S in F4 fell through to PASS_THROUGH
// -- the key sat in the buffer, the page locked up. Now both Ctrl-S and
// Cmd-S in F4/Shift+F4 swallow.
static void test_macroedit_cmd_s_swallows_on_macos() {
    SaveKeyContext c = ctx();
    c.kstate_has_alt       = true;       // Cmd-S on macOS
    c.is_macroedit_state   = true;
    c.song_has_filename    = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_SWALLOW);
}
static void test_arpedit_cmd_s_swallows_on_macos() {
    SaveKeyContext c = ctx();
    c.kstate_has_alt    = true;
    c.is_arpedit_state  = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_SWALLOW);
}
// And: Cmd-S OUTSIDE the editor states must NOT swallow -- it has its
// own meaning (Pattern Editor's "Set Instrument on selection" via
// KS_HAS_ALT).
static void test_cmd_s_outside_editor_passes_through() {
    SaveKeyContext c = ctx();
    c.kstate_has_alt = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_PASS_THROUGH);
}

// Ctrl-S with a saved filename pops the in-place save popup.
static void test_default_with_filename_opens_save_popup() {
    SaveKeyContext c = ctx();
    c.kstate_ctrl       = true;
    c.song_has_filename = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_OPEN_SAVE_POPUP);
}

// Ctrl-S without a filename opens Save As.
static void test_default_without_filename_opens_save_as() {
    SaveKeyContext c = ctx();
    c.kstate_ctrl       = true;
    c.song_has_filename = false;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_OPEN_SAVE_AS);
}

// Shift+Ctrl+S forces Save As regardless of filename state.
static void test_shift_ctrl_forces_save_as() {
    SaveKeyContext c = ctx();
    c.kstate_ctrl       = true;
    c.kstate_shift      = true;
    c.song_has_filename = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_OPEN_SAVE_AS);
}

// Shift alone (no Ctrl) is pass-through.
static void test_shift_alone_passes_through() {
    SaveKeyContext c = ctx();
    c.kstate_shift = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_PASS_THROUGH);
}

// Edge: an editor state flag is set but Ctrl is NOT held -> still
// pass through. Don't accidentally swallow plain S keypresses.
static void test_macroedit_without_ctrl_passes_through() {
    SaveKeyContext c = ctx();
    c.is_macroedit_state = true;
    CHECK_EQ((int)dispatch_save_key(c), (int)SAVE_KEY_PASS_THROUGH);
}

int main() {
    test_no_modifier_passes_through();
    test_keybindings_lets_page_handle();
    test_macroedit_ctrl_s_swallows();
    test_arpedit_ctrl_s_swallows();
    test_macroedit_cmd_s_swallows_on_macos();
    test_arpedit_cmd_s_swallows_on_macos();
    test_cmd_s_outside_editor_passes_through();
    test_default_with_filename_opens_save_popup();
    test_default_without_filename_opens_save_as();
    test_shift_ctrl_forces_save_as();
    test_shift_alone_passes_through();
    test_macroedit_without_ctrl_passes_through();

    std::printf("%d/%d checks passed\n", g_total - g_failures, g_total);
    return g_failures == 0 ? 0 : 1;
}
