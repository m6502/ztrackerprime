// Regression tests for src/pattern_guard.h.
//
// Locks in the fix for the 2026-06-03 bus error: clicking an empty order slot
// on F11 set cur_edit_pattern to the 0x100 "empty" order sentinel, and F6 then
// drove player::play(0x100) -> playback() -> patterns[256] out of bounds ->
// SIGBUS. player::play_immediately() now routes the pattern index through
// zt_pattern_index_playable() before the patterns[] deref; this test pins the
// helpers' contract so a future edit can't quietly let a sentinel back through.
//
// Pure header, no SDL / no module.h -- ZT_PATTERN_GUARD_COUNT defaults to 256
// here, matching the static_assert against ZTM_MAX_PATTERNS in playback.cpp.

#include "pattern_guard.h"

#include <stdio.h>

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

static void test_valid_range_is_playable() {
    CHECK(zt_pattern_index_playable(0));
    CHECK(zt_pattern_index_playable(1));
    CHECK(zt_pattern_index_playable(128));
    CHECK(zt_pattern_index_playable(255));   // last real slot
}

static void test_sentinels_and_oob_not_playable() {
    CHECK(!zt_pattern_index_playable(256));   // 0x100 = empty / end-of-song
    CHECK(!zt_pattern_index_playable(257));   // 0x101 = skip
    CHECK(!zt_pattern_index_playable(0x100)); // the exact crash value
    CHECK(!zt_pattern_index_playable(1000));
    CHECK(!zt_pattern_index_playable(-1));    // negative (defensive)
    CHECK(!zt_pattern_index_playable(-12345));
}

static void test_orderlist_select_passes_real_patterns() {
    // A real pattern in a slot is selected as-is.
    CHECK(zt_orderlist_select_pattern(0,   7) == 0);
    CHECK(zt_orderlist_select_pattern(42,  7) == 42);
    CHECK(zt_orderlist_select_pattern(255, 7) == 255);
}

static void test_orderlist_select_keeps_fallback_on_sentinel() {
    // The bug: an empty (0x100) or skip (0x101) slot must keep the caller's
    // current pattern, never leak the sentinel.
    CHECK(zt_orderlist_select_pattern(0x100, 5) == 5);
    CHECK(zt_orderlist_select_pattern(0x101, 5) == 5);
    CHECK(zt_orderlist_select_pattern(256,   0) == 0);
    CHECK(zt_orderlist_select_pattern(99999, 12) == 12);
    CHECK(zt_orderlist_select_pattern(-1,    12) == 12);
    // Fallback is returned verbatim even though it, too, is just trusted as-is
    // (callers always pass the live cur_edit_pattern, which is itself kept in
    // range elsewhere).
    CHECK(zt_orderlist_select_pattern(0x100, 0) == 0);
}

int main(void) {
    test_valid_range_is_playable();
    test_sentinels_and_oob_not_playable();
    test_orderlist_select_passes_real_patterns();
    test_orderlist_select_keeps_fallback_on_sentinel();
    printf("pattern_guard: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
