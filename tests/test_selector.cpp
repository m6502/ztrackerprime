// test_selector.cpp -- exhaustive tests for preset_selector.h decisions.
//
// Each scenario corresponds to a real bug the user hit during PR #64 dev.
// The pure functions in preset_selector.h drive the listbox subclasses
// in CUI_Midimacroeditor.cpp / CUI_Arpeggioeditor.cpp; if you change a
// PresetEvent shape, run this and the editor behaviour will follow.

#include "preset_selector.h"

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

static PresetCursor make_cursor(int n, int active, int cur) {
    PresetCursor s;
    s.num_items    = n;
    s.active_index = active;
    s.cursor_row   = cur;
    return s;
}

// ---------------------------------------------------------------------------
// Click

// REGRESSION: clicking the row that's already the active preset must still
// apply. The earlier int_data dedupe guard hid this case, leaving "click
// once works, click again does nothing" depending on which row was active.
static void test_click_on_active_row_applies() {
    auto s = make_cursor(/*n*/10, /*active*/3, /*cur*/3);
    auto e = preset_on_click(s, 3);
    CHECK(e.fire_apply);
    CHECK_EQ(e.new_cursor_row, 3);
    CHECK(e.consumed);
}

static void test_click_on_different_row_applies_and_moves() {
    auto s = make_cursor(10, 2, 2);
    auto e = preset_on_click(s, 7);
    CHECK(e.fire_apply);
    CHECK_EQ(e.new_cursor_row, 7);
    CHECK(e.consumed);
}

static void test_click_on_first_row_when_active_is_first() {
    // The exact "clicking once works, clicking again does nothing" scenario
    // the user hit -- listbox starts at cursor=0/active=0 and a click on
    // row 0 must apply.
    auto s = make_cursor(10, 0, 0);
    auto e = preset_on_click(s, 0);
    CHECK(e.fire_apply);
    CHECK_EQ(e.new_cursor_row, 0);
}

static void test_click_out_of_bounds_negative_is_noop() {
    auto s = make_cursor(10, 0, 0);
    auto e = preset_on_click(s, -1);
    CHECK(!e.fire_apply);
    CHECK_EQ(e.new_cursor_row, -1);
    CHECK(!e.consumed);
}

static void test_click_out_of_bounds_high_is_noop() {
    auto s = make_cursor(10, 0, 0);
    auto e = preset_on_click(s, 99);
    CHECK(!e.fire_apply);
    CHECK_EQ(e.new_cursor_row, -1);
    CHECK(!e.consumed);
}

static void test_click_on_empty_listbox_is_noop() {
    auto s = make_cursor(0, 0, 0);
    auto e = preset_on_click(s, 0);
    CHECK(!e.fire_apply);
    CHECK(!e.consumed);
}

// ---------------------------------------------------------------------------
// Up arrow

// REGRESSION: parent ListBox's "Up at top -> ret = -1 -> previous tab stop"
// surrendered focus the moment the user pressed Up after clicking the first
// row. The override now keeps focus and consumes the keypress.
static void test_up_at_top_stays_focused() {
    auto s = make_cursor(10, 0, 0);
    auto e = preset_on_up(s);
    CHECK(!e.fire_apply);
    CHECK_EQ(e.new_cursor_row, -1);  // unchanged
    CHECK(e.consumed);
    CHECK(e.stay_focused);
}

static void test_up_in_middle_moves_one_up() {
    auto s = make_cursor(10, 5, 5);
    auto e = preset_on_up(s);
    CHECK(!e.fire_apply);
    CHECK_EQ(e.new_cursor_row, 4);
    CHECK(e.consumed);
}

static void test_up_does_not_apply() {
    // Cursor movement alone doesn't apply -- the user must Enter / Space /
    // click to commit. (Otherwise scrolling through the list would
    // overwrite their data on every keypress.)
    auto s = make_cursor(10, 5, 5);
    auto e = preset_on_up(s);
    CHECK(!e.fire_apply);
}

// ---------------------------------------------------------------------------
// Down arrow

static void test_down_at_bottom_stays_focused() {
    auto s = make_cursor(10, 9, 9);
    auto e = preset_on_down(s);
    CHECK(!e.fire_apply);
    CHECK_EQ(e.new_cursor_row, -1);  // unchanged
    CHECK(e.consumed);
    CHECK(e.stay_focused);
}

static void test_down_in_middle_moves_one_down() {
    auto s = make_cursor(10, 0, 5);
    auto e = preset_on_down(s);
    CHECK_EQ(e.new_cursor_row, 6);
    CHECK(e.consumed);
}

// ---------------------------------------------------------------------------
// Enter / Space

// REGRESSION: Space wasn't a synonym for Enter in the bare ListBox; the
// editor pages need both because users default to Space for "go".
static void test_apply_fires_and_does_not_move_cursor() {
    auto s = make_cursor(10, 2, 5);
    auto e = preset_on_apply(s);
    CHECK(e.fire_apply);
    CHECK_EQ(e.new_cursor_row, -1);   // cursor stays at 5
    CHECK(e.consumed);
}

static void test_apply_on_empty_is_noop() {
    auto s = make_cursor(0, 0, 0);
    auto e = preset_on_apply(s);
    CHECK(!e.fire_apply);
    CHECK(!e.consumed);
}

// ---------------------------------------------------------------------------
// P-cycle

// REGRESSION: ListBox typeahead with use_key_select=true ate P as a
// prefix-jump character so the page-level cycle handler never got it.
// preset_on_cycle moves to the next index modulo num_items and fires apply.
static void test_cycle_advances_active_by_one() {
    auto s = make_cursor(10, 3, 3);
    auto e = preset_on_cycle(s);
    CHECK(e.fire_apply);
    CHECK_EQ(e.new_cursor_row, 4);
    CHECK(e.consumed);
}

static void test_cycle_wraps_at_end() {
    auto s = make_cursor(10, 9, 9);
    auto e = preset_on_cycle(s);
    CHECK(e.fire_apply);
    CHECK_EQ(e.new_cursor_row, 0);
}

static void test_cycle_with_one_item_stays_at_zero() {
    auto s = make_cursor(1, 0, 0);
    auto e = preset_on_cycle(s);
    CHECK(e.fire_apply);
    CHECK_EQ(e.new_cursor_row, 0);
}

static void test_cycle_on_empty_is_noop() {
    auto s = make_cursor(0, 0, 0);
    auto e = preset_on_cycle(s);
    CHECK(!e.fire_apply);
    CHECK(!e.consumed);
}

// ---------------------------------------------------------------------------
// Multi-step interaction sequences (regression scenarios)

// Click row 0 (active becomes 0). Click row 0 again. Click row 5.
// Each click must apply.
static void test_scenario_repeated_clicks_each_apply() {
    PresetCursor s = make_cursor(10, 0, 0);

    // First click on row 0 (already active).
    auto e1 = preset_on_click(s, 0);
    CHECK(e1.fire_apply);
    // Caller would now setCursor(0), call apply_preset, set active_index=0.
    s.cursor_row = e1.new_cursor_row;
    s.active_index = 0;

    // Second click on row 0 again -- must apply, not silently no-op.
    auto e2 = preset_on_click(s, 0);
    CHECK(e2.fire_apply);
    s.cursor_row = e2.new_cursor_row;

    // Third click on row 5.
    auto e3 = preset_on_click(s, 5);
    CHECK(e3.fire_apply);
    CHECK_EQ(e3.new_cursor_row, 5);
}

// Click row 0, then arrow Up -- cursor should stay at 0 with focus retained.
// The original bug was that this surrendered focus and Up never moved the
// cursor again because the listbox was no longer the focused element.
static void test_scenario_click_then_up_stays_put() {
    PresetCursor s = make_cursor(10, 0, 0);
    auto click = preset_on_click(s, 0);
    s.cursor_row = click.new_cursor_row;
    s.active_index = 0;

    auto up = preset_on_up(s);
    CHECK(up.consumed);
    CHECK(up.stay_focused);
    CHECK_EQ(up.new_cursor_row, -1);   // didn't move
}

// Cycle from preset 0 through all 10 -- must visit every index once and
// wrap back to 0 after exactly num_items cycles.
static void test_scenario_cycle_full_loop() {
    PresetCursor s = make_cursor(10, 0, 0);
    bool visited[10] = {false};
    visited[0] = true;
    for (int step = 0; step < 10; ++step) {
        auto e = preset_on_cycle(s);
        CHECK(e.fire_apply);
        s.active_index = e.new_cursor_row;
        s.cursor_row   = e.new_cursor_row;
        visited[s.active_index] = true;
    }
    // After 10 cycles starting from 0 we are back at 0 and have touched
    // every index exactly once along the way.
    CHECK_EQ(s.active_index, 0);
    for (int i = 0; i < 10; ++i) CHECK(visited[i]);
}

// ---------------------------------------------------------------------------

int main() {
    test_click_on_active_row_applies();
    test_click_on_different_row_applies_and_moves();
    test_click_on_first_row_when_active_is_first();
    test_click_out_of_bounds_negative_is_noop();
    test_click_out_of_bounds_high_is_noop();
    test_click_on_empty_listbox_is_noop();

    test_up_at_top_stays_focused();
    test_up_in_middle_moves_one_up();
    test_up_does_not_apply();

    test_down_at_bottom_stays_focused();
    test_down_in_middle_moves_one_down();

    test_apply_fires_and_does_not_move_cursor();
    test_apply_on_empty_is_noop();

    test_cycle_advances_active_by_one();
    test_cycle_wraps_at_end();
    test_cycle_with_one_item_stays_at_zero();
    test_cycle_on_empty_is_noop();

    test_scenario_repeated_clicks_each_apply();
    test_scenario_click_then_up_stays_put();
    test_scenario_cycle_full_loop();

    std::printf("%d/%d checks passed\n", g_total - g_failures, g_total);
    return g_failures == 0 ? 0 : 1;
}
