// test_presets.cpp -- regression tests for preset_data.h
//
// Standalone executable. Returns 0 on success, non-zero on the first
// failed assertion. Driven by CTest via tests/CMakeLists.txt.
//
// Coverage focus: the bug fixed in commit 295f726 ("F4/Shift+F4: refresh
// slider widgets after preset is applied") -- the preset apply functions
// must (a) write the documented length / speed / data into the target
// struct, and (b) leave the rest of the struct in a clean state. The
// stale-widget revert was a UI-side bug, but having these data tests
// pinned down means future preset edits can't silently regress the
// values that the editor pages then need to mirror back into widgets.

#include "preset_data.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

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
// Arpeggio preset tests

static void test_arp_counts() {
    CHECK_EQ(AR_PRESET_COUNT, 10);
    for (int i = 0; i < AR_PRESET_COUNT; ++i) {
        CHECK(AR_PRESETS[i].name != nullptr);
        CHECK(AR_PRESETS[i].length > 0);
        CHECK(AR_PRESETS[i].speed > 0);
        CHECK(AR_PRESETS[i].len_pitches >= 0);
        CHECK(AR_PRESETS[i].len_pitches <= 16);
    }
}

static void test_arp_apply_major_triad() {
    arpeggio a;
    ar_apply_preset_to(&a, 0); // Major Triad Up
    CHECK_EQ(a.length, 3);
    CHECK_EQ(a.speed, 6);
    CHECK_EQ(a.repeat_pos, 0);
    CHECK_EQ(a.num_cc, 0);
    CHECK_EQ((int)a.pitch[0], 0);
    CHECK_EQ((int)a.pitch[1], 4);
    CHECK_EQ((int)a.pitch[2], 7);
    // Steps past len_pitches must be empty so the pattern doesn't ring on.
    CHECK_EQ((int)a.pitch[3], (int)ZTM_ARP_EMPTY_PITCH);
    CHECK_EQ((int)a.pitch[ZTM_ARPEGGIO_LEN - 1], (int)ZTM_ARP_EMPTY_PITCH);
    CHECK(std::strcmp(a.name, "Major Triad Up") == 0);
}

static void test_arp_apply_minor_triad() {
    arpeggio a;
    ar_apply_preset_to(&a, 1); // Minor Triad Up
    CHECK_EQ((int)a.pitch[0], 0);
    CHECK_EQ((int)a.pitch[1], 3);
    CHECK_EQ((int)a.pitch[2], 7);
}

static void test_arp_apply_major_scale() {
    arpeggio a;
    ar_apply_preset_to(&a, 8); // Major Scale (1 oct)
    CHECK_EQ(a.length, 8);
    int expected[8] = {0, 2, 4, 5, 7, 9, 11, 12};
    for (int i = 0; i < 8; ++i) CHECK_EQ((int)a.pitch[i], expected[i]);
    CHECK_EQ((int)a.pitch[8], (int)ZTM_ARP_EMPTY_PITCH);
}

static void test_arp_apply_overwrites_prior() {
    // Pitches from a previous preset must NOT survive when a shorter
    // preset is applied -- the editor depends on the wipe loop in
    // ar_apply_preset_to to keep stale tail values from rendering as
    // ghost notes.
    arpeggio a;
    ar_apply_preset_to(&a, 8); // 8-step major scale
    ar_apply_preset_to(&a, 0); // 3-step major triad (length=3)
    CHECK_EQ(a.length, 3);
    CHECK_EQ((int)a.pitch[3], (int)ZTM_ARP_EMPTY_PITCH);
    CHECK_EQ((int)a.pitch[7], (int)ZTM_ARP_EMPTY_PITCH);
}

static void test_arp_apply_bad_index_is_noop() {
    arpeggio a;
    a.length = 99;
    ar_apply_preset_to(&a, -1);
    CHECK_EQ(a.length, 99);
    ar_apply_preset_to(&a, AR_PRESET_COUNT);
    CHECK_EQ(a.length, 99);
    ar_apply_preset_to(nullptr, 0); // must not crash
}

// ---------------------------------------------------------------------------
// MIDI Macro preset tests

static void test_mm_counts() {
    CHECK_EQ(MM_PRESET_COUNT, 6);
    for (int i = 0; i < MM_PRESET_COUNT; ++i) {
        CHECK(MM_PRESETS[i].name != nullptr);
        CHECK(MM_PRESETS[i].len > 0);
        CHECK(MM_PRESETS[i].len < ZTM_MIDIMACRO_MAXLEN);
    }
}

static void test_mm_apply_cc_modulation() {
    midimacro m;
    mm_apply_preset_to(&m, 0); // CC 1 Modulation
    CHECK_EQ((int)m.data[0], 0xB0);                       // Status: CC on ch 0
    CHECK_EQ((int)m.data[1], 0x01);                       // CC #1
    CHECK_EQ((int)m.data[2], (int)ZTM_MIDIMAC_PARAM1);    // value=PARAM1
    CHECK_EQ((int)m.data[3], (int)ZTM_MIDIMAC_END);       // terminator
    CHECK(std::strcmp(m.name, "CC 1 Modulation (param=value)") == 0);
}

static void test_mm_apply_program_change() {
    midimacro m;
    mm_apply_preset_to(&m, 3); // Program Change
    CHECK_EQ((int)m.data[0], 0xC0);
    CHECK_EQ((int)m.data[1], (int)ZTM_MIDIMAC_PARAM1);
    CHECK_EQ((int)m.data[2], (int)ZTM_MIDIMAC_END);
}

static void test_mm_apply_overwrites_prior() {
    // Long preset's tail bytes must be wiped when a short one replaces it.
    midimacro m;
    mm_apply_preset_to(&m, 0); // 3 bytes
    mm_apply_preset_to(&m, 3); // 2 bytes
    CHECK_EQ((int)m.data[2], (int)ZTM_MIDIMAC_END);
    CHECK_EQ((int)m.data[3], (int)ZTM_MIDIMAC_END);
    // ...all the way to the end of the buffer.
    for (int i = 4; i < ZTM_MIDIMACRO_MAXLEN; ++i) {
        CHECK_EQ((int)m.data[i], (int)ZTM_MIDIMAC_END);
    }
}

static void test_mm_apply_bad_index_is_noop() {
    midimacro m;
    m.data[0] = 0xFEED;
    mm_apply_preset_to(&m, -1);
    CHECK_EQ((int)m.data[0], 0xFEED);
    mm_apply_preset_to(&m, MM_PRESET_COUNT);
    CHECK_EQ((int)m.data[0], 0xFEED);
    mm_apply_preset_to(nullptr, 0);
}

// ---------------------------------------------------------------------------

int main() {
    test_arp_counts();
    test_arp_apply_major_triad();
    test_arp_apply_minor_triad();
    test_arp_apply_major_scale();
    test_arp_apply_overwrites_prior();
    test_arp_apply_bad_index_is_noop();

    test_mm_counts();
    test_mm_apply_cc_modulation();
    test_mm_apply_program_change();
    test_mm_apply_overwrites_prior();
    test_mm_apply_bad_index_is_noop();

    std::printf("%d/%d checks passed\n", g_total - g_failures, g_total);
    return g_failures == 0 ? 0 : 1;
}
