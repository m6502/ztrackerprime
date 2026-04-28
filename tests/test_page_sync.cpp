// test_page_sync.cpp -- integration tests for the editor page's
// preset-apply / widget-refresh / sync cycle.
//
// These mirror the per-frame work CUI_Arpeggioeditor::update does, in
// the same order:
//   1. UI->update() runs (the listbox subclass's OnSelect fires
//      ar_apply_preset, sets ar_preset_just_applied).
//   2. The page polls ar_preset_just_applied and refreshes the slider
//      widgets from the freshly-applied arpeggio data.
//   3. The page reads slider widget values back and runs sync_arp_
//      widgets_to_data, which writes the widget values into the
//      arpeggio struct only if they differ.
//
// The bug we keep hitting lives in the boundary between (1) and (3):
// if (2) is missing or runs in the wrong order, (3) sees stale widget
// values and reverts the freshly-applied preset on the same frame.
// The user's report -- "Enter freezes UI" / "Space selects but values
// don't change" -- maps onto step (3) reverting (1).
//
// Each test below sets up the state, runs the simulated frame, and
// asserts the post-frame data + widgets.

#include "preset_data.h"
#include "page_sync.h"

#include <cstdio>
#include <cstring>

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

// One simulated "page frame": run the preset_just_applied refresh
// (when set), then the slider-to-data sync. Mirrors the order in
// CUI_Arpeggioeditor::update.
struct ArpEditorFrame {
    arpeggio   data;
    ArpSliders widgets;
    bool       preset_just_applied;

    ArpEditorFrame() : preset_just_applied(false) {
        widgets.length = data.length;
        widgets.speed  = data.speed;
        widgets.repeat_pos = data.repeat_pos;
        widgets.num_cc = data.num_cc;
        for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i) widgets.cc[i] = data.cc[i];
    }

    // Simulate the listbox firing OnSelect with index `idx`. Mirrors
    // ArPresetSelector::OnSelect: apply the preset to `data`, mark the
    // flag.
    void on_select_preset(int idx) {
        ar_apply_preset_to(&data, idx);
        preset_just_applied = true;
    }

    // Simulate one tick of CUI_Arpeggioeditor::update: refresh widgets
    // if the flag is set, then run the widget->data sync.
    void tick() {
        if (preset_just_applied) {
            preset_just_applied = false;
            refresh_arp_widgets_from_data(widgets, data);
        }
        sync_arp_widgets_to_data(widgets, data);
    }
};

// ---------------------------------------------------------------------------
// REGRESSION SUITE: preset apply must survive the widget sync.

// "Press Enter on a preset" / "click a preset row" flow.
static void test_preset_survives_one_tick() {
    ArpEditorFrame f;
    // Pre-state: empty arpeggio, sliders at defaults.
    CHECK_EQ(f.data.length, 0);

    // User clicks Major Triad Up (idx 0): length=3, speed=6.
    f.on_select_preset(0);
    f.tick();

    CHECK_EQ(f.data.length, 3);
    CHECK_EQ(f.data.speed,  6);
    CHECK_EQ((int)f.data.pitch[0], 0);
    CHECK_EQ((int)f.data.pitch[1], 4);
    CHECK_EQ((int)f.data.pitch[2], 7);
    // Widgets must mirror data after refresh -- otherwise the next
    // tick's sync would clobber the data again.
    CHECK_EQ(f.widgets.length, 3);
    CHECK_EQ(f.widgets.speed,  6);
}

// Same scenario over multiple ticks (idle frames). Preset must remain
// applied on every subsequent tick.
static void test_preset_persists_across_idle_frames() {
    ArpEditorFrame f;
    f.on_select_preset(8); // Major Scale (1 oct): length=8
    f.tick();
    CHECK_EQ(f.data.length, 8);

    for (int i = 0; i < 50; ++i) f.tick();
    CHECK_EQ(f.data.length, 8);
    CHECK_EQ(f.data.speed,  4);
    CHECK_EQ((int)f.data.pitch[7], 12);
}

// THE EXACT BUG: if preset_just_applied is FALSE when the page ticks
// (i.e., the page forgot to set/check the flag), the sync block sees
// stale widget values and reverts the freshly-applied preset.
//
// Locking this in so any future refactor that drops the flag-based
// refresh fails this test loudly.
static void test_without_flag_sync_reverts_preset() {
    ArpEditorFrame f;
    // Apply preset directly (skipping the on_select_preset path that
    // sets the flag).
    ar_apply_preset_to(&f.data, 0); // Major Triad Up -> length=3
    CHECK_EQ(f.data.length, 3);
    // No flag set -> tick runs ONLY sync, sees widgets.length=0 and
    // f.data.length=3 -> reverts to 0.
    f.tick();
    CHECK_EQ(f.data.length, 0);  // demonstrates the bug
}

// User changes a preset, then bumps the speed slider manually. The
// manual change must flow into the data (slider->data sync). The
// preset_just_applied flag is only set on the apply tick.
static void test_manual_slider_change_flows_into_data() {
    ArpEditorFrame f;
    f.on_select_preset(0);   // Major Triad: length=3, speed=6
    f.tick();
    CHECK_EQ(f.data.speed, 6);

    // User drags Speed slider to 12.
    f.widgets.speed = 12;
    f.tick();
    CHECK_EQ(f.data.speed, 12);
    CHECK_EQ(f.data.length, 3);  // length untouched
}

// Sequence: apply preset A, apply preset B. Each apply must replace
// the previous data cleanly with no leftover from A.
static void test_consecutive_preset_applies() {
    ArpEditorFrame f;
    f.on_select_preset(8);   // Major Scale: length=8, pitch[7]=12
    f.tick();
    CHECK_EQ(f.data.length, 8);
    CHECK_EQ((int)f.data.pitch[7], 12);

    f.on_select_preset(0);   // Major Triad: length=3, pitch[2]=7
    f.tick();
    CHECK_EQ(f.data.length, 3);
    CHECK_EQ((int)f.data.pitch[2], 7);
    CHECK_EQ((int)f.data.pitch[3], (int)ZTM_ARP_EMPTY_PITCH);
    CHECK_EQ((int)f.data.pitch[7], (int)ZTM_ARP_EMPTY_PITCH);
}

// repeat_pos clamp: if user shrinks the length below repeat_pos via the
// slider, sync_arp_widgets_to_data must clamp repeat_pos to length-1.
static void test_repeat_pos_clamped_when_length_shrinks() {
    ArpEditorFrame f;
    f.data.length     = 8;
    f.data.repeat_pos = 6;
    f.widgets.length  = 4;
    f.widgets.repeat_pos = 6;
    f.tick();
    CHECK_EQ(f.data.length, 4);
    CHECK_EQ(f.data.repeat_pos, 3);
}

// ---------------------------------------------------------------------------
// MIDI Macro: preset apply must mirror length back into the slider.

struct MmEditorFrame {
    midimacro  data;
    MmSliders  widgets;
    bool       preset_just_applied;

    MmEditorFrame() : preset_just_applied(false) {
        refresh_mm_widgets_from_data(widgets, data);
    }
    void on_select_preset(int idx) {
        mm_apply_preset_to(&data, idx);
        preset_just_applied = true;
    }
    void tick() {
        if (preset_just_applied) {
            preset_just_applied = false;
            refresh_mm_widgets_from_data(widgets, data);
        }
        // No widgets-to-data sync for the macro (length is the only
        // numeric slider; data bytes come from grid input). The page
        // does set length explicitly via mm_set_length() -- we model
        // that as the user editing widgets.length.
    }
};

static void test_mm_preset_length_reflects_in_widget() {
    MmEditorFrame f;
    f.on_select_preset(0);   // CC 1 Modulation: 3 bytes + END
    f.tick();
    CHECK_EQ(f.widgets.length, 3);
}

static void test_mm_consecutive_presets_widget_follows() {
    MmEditorFrame f;
    f.on_select_preset(0);   // 3 bytes
    f.tick();
    CHECK_EQ(f.widgets.length, 3);
    f.on_select_preset(3);   // Program Change: 2 bytes
    f.tick();
    CHECK_EQ(f.widgets.length, 2);
}

// ---------------------------------------------------------------------------

int main() {
    test_preset_survives_one_tick();
    test_preset_persists_across_idle_frames();
    test_without_flag_sync_reverts_preset();
    test_manual_slider_change_flows_into_data();
    test_consecutive_preset_applies();
    test_repeat_pos_clamped_when_length_shrinks();

    test_mm_preset_length_reflects_in_widget();
    test_mm_consecutive_presets_widget_follows();

    std::printf("%d/%d checks passed\n", g_total - g_failures, g_total);
    return g_failures == 0 ? 0 : 1;
}
