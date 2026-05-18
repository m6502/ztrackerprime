// test_ccenv.cpp — unit tests for the CC envelope pure-logic core
// (ccenv_interp.h + ccenv_advance.h). No SDL, no module.h, no ztracker
// runtime — both headers are deliberately self-contained so this test
// links in the same translation unit as fixtures we build inline.
//
// Coverage:
//   interp: clamp-left, clamp-right, midpoint linear, asymmetric
//           segments, vertical step, 1-node + 2-node, multi-segment
//   step:   disabled envelope, sustain loop while key down, sustain
//           released on key_off, regular loop, terminate at last node,
//           speed gating (caller drives via a counter -- direct step
//           always advances)

#include "../src/ccenv_interp.h"
#include "../src/ccenv_advance.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int g_failures = 0;

#define ASSERT(cond) do {                                                    \
    if (!(cond)) {                                                           \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
        g_failures++;                                                        \
    }                                                                        \
} while (0)

#define ASSERT_EQ(a, b) do {                                                 \
    int _a = (int)(a); int _b = (int)(b);                                    \
    if (_a != _b) {                                                          \
        printf("FAIL %s:%d: %s == %s (got %d vs %d)\n",                      \
               __FILE__, __LINE__, #a, #b, _a, _b);                          \
        g_failures++;                                                        \
    }                                                                        \
} while (0)

// ---------------------------------------------------------------------------
// Interpolation

static void test_interp_clamp_and_linear(void) {
    unsigned short t[2] = {0, 64};
    unsigned char  v[2] = {0, 127};
    ASSERT_EQ(ccenv_interp_raw(t, v, 2, -10),   0);   // clamp left
    ASSERT_EQ(ccenv_interp_raw(t, v, 2,   0),   0);
    ASSERT_EQ(ccenv_interp_raw(t, v, 2,  32),  63);   // midpoint
    ASSERT_EQ(ccenv_interp_raw(t, v, 2,  64), 127);
    ASSERT_EQ(ccenv_interp_raw(t, v, 2, 100), 127);   // clamp right
}

static void test_interp_single_node(void) {
    unsigned short t[1] = {0};
    unsigned char  v[1] = {64};
    ASSERT_EQ(ccenv_interp_raw(t, v, 1, 0),    64);
    ASSERT_EQ(ccenv_interp_raw(t, v, 1, 1000), 64);
}

static void test_interp_vertical_step(void) {
    // Two identical ticks resolves to the right-hand value.
    unsigned short t[2] = {16, 16};
    unsigned char  v[2] = {0,  127};
    ASSERT_EQ(ccenv_interp_raw(t, v, 2, 16), 127);
}

static void test_interp_multi_segment(void) {
    // V-shape: 0 -> 127 -> 0 -> 64
    unsigned short t[4] = {0, 16, 32, 48};
    unsigned char  v[4] = {0, 127, 0,  64};
    ASSERT_EQ(ccenv_interp_raw(t, v, 4,  0),    0);
    ASSERT_EQ(ccenv_interp_raw(t, v, 4,  8),   63);    // halfway up
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 16),  127);
    // Integer division on the descending segment truncates toward 0
    // for the negative slope, so the midpoint comes out 64 (vs 63 on
    // the ascending side at pos=8). The asymmetry is acceptable -- the
    // value emitted at runtime is what matters, not whether it matches
    // the ascending side bit-for-bit.
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 24),   64);    // halfway down
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 32),    0);
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 40),   32);    // halfway up to 64
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 48),   64);
}

// ---------------------------------------------------------------------------
// Advance / step

static void test_step_disabled_emits_done(void) {
    unsigned short t[2] = {0, 64};
    unsigned char  v[2] = {0, 127};
    struct ccenv_voice_state vs = {0, 0, 0};
    int got = ccenv_step(&vs, t, v, 2, /*flags*/0,
                         0, 1, 0, 1);
    ASSERT_EQ(got, 0);
    ASSERT_EQ(vs.done, 1);
}

static void test_step_terminate(void) {
    unsigned short t[2] = {0, 4};
    unsigned char  v[2] = {0, 127};
    struct ccenv_voice_state vs = {0, 0, 0};
    int last = 0;
    for (int i = 0; i < 10; i++) {
        last = ccenv_step(&vs, t, v, 2, CCENV_F_ENABLED,
                          0, 1, 0, 1);
    }
    // After 10 steps, position clamped at last node tick=4, done set.
    ASSERT_EQ(vs.position, 4);
    ASSERT_EQ(vs.done,     1);
    ASSERT_EQ(last,        127);
}

static void test_step_sustain_holds_while_key_down(void) {
    // 3 nodes: 0..2..6 (values 0, 127, 0). Sustain region == node 1
    // (degenerate single-node sustain). With key held, position should
    // never advance past tick[1]=2.
    unsigned short t[3] = {0, 2, 6};
    unsigned char  v[3] = {0, 127, 0};
    struct ccenv_voice_state vs = {0, 0, 0};
    int last = 0;
    for (int i = 0; i < 50; i++) {
        last = ccenv_step(&vs, t, v, 3,
                          CCENV_F_ENABLED | CCENV_F_SUSTAIN,
                          0, 2, /*sustain*/ 1, 1);
    }
    ASSERT_EQ(vs.done, 0);
    ASSERT(vs.position >= 2 && vs.position <= 2);
    ASSERT_EQ(last, 127);
}

static void test_step_sustain_releases_on_keyoff(void) {
    // 3 nodes: 0..4..8 (values 0, 127, 0). Sustain region = node 1.
    // Hold key for 20 steps (position pinned at tick=4), then release.
    // After release with no LOOP flag, envelope walks to tick=8 (final
    // node) and sets done.
    unsigned short t[3] = {0, 4, 8};
    unsigned char  v[3] = {0, 127, 0};
    struct ccenv_voice_state vs = {0, 0, 0};
    for (int i = 0; i < 20; i++) {
        ccenv_step(&vs, t, v, 3,
                   CCENV_F_ENABLED | CCENV_F_SUSTAIN,
                   0, 2, 1, 1);
    }
    ASSERT_EQ(vs.position, 4);
    ASSERT_EQ(vs.done,     0);
    vs.key_off = 1;
    int last = 0;
    for (int i = 0; i < 20; i++) {
        last = ccenv_step(&vs, t, v, 3,
                          CCENV_F_ENABLED | CCENV_F_SUSTAIN,
                          0, 2, 1, 1);
    }
    ASSERT_EQ(vs.position, 8);
    ASSERT_EQ(vs.done,     1);
    ASSERT_EQ(last,        0);
}

static void test_step_loop(void) {
    // Loop from node 0 (tick 0) to node 1 (tick 4), values 0..127.
    // Without sustain, position should bounce 0,1,2,3,4 -> 0 -> 1 ...
    unsigned short t[2] = {0, 4};
    unsigned char  v[2] = {0, 127};
    struct ccenv_voice_state vs = {0, 0, 0};
    int saw_127 = 0, saw_0_after_127 = 0, was_127 = 0;
    for (int i = 0; i < 20; i++) {
        int val = ccenv_step(&vs, t, v, 2,
                             CCENV_F_ENABLED | CCENV_F_LOOP,
                             0, 1, 0, 0);
        if (val == 127) { saw_127 = 1; was_127 = 1; }
        else if (was_127 && val == 0) saw_0_after_127 = 1;
    }
    ASSERT_EQ(vs.done, 0);
    ASSERT(saw_127);
    ASSERT(saw_0_after_127);
}

// ---------------------------------------------------------------------------

int main(void) {
    printf("test_ccenv: starting\n");

    test_interp_clamp_and_linear();
    test_interp_single_node();
    test_interp_vertical_step();
    test_interp_multi_segment();

    test_step_disabled_emits_done();
    test_step_terminate();
    test_step_sustain_holds_while_key_down();
    test_step_sustain_releases_on_keyoff();
    test_step_loop();

    if (g_failures == 0) {
        printf("test_ccenv: all good\n");
        return 0;
    }
    printf("test_ccenv: %d failure(s)\n", g_failures);
    return 1;
}
