// test_ccenv.cpp — pure-logic tests for the CC envelope interpolation
// and advance core. SDL-free, module.h-free. Both headers operate on
// raw arrays so the data model can evolve without touching this file.

#include "../src/ccenv_interp.h"
#include "../src/ccenv_advance.h"

#include <cstdio>
#include <cstring>

static int g_failures = 0;

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_failures++; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    int _a = (int)(a); int _b = (int)(b); \
    if (_a != _b) { \
        printf("FAIL %s:%d: %s == %s  got %d vs %d\n", \
               __FILE__, __LINE__, #a, #b, _a, _b); \
        g_failures++; \
    } \
} while (0)

// ---------------------------------------------------------------------------

static void test_interp_clamp(void) {
    unsigned short t[2] = {0, 64};
    unsigned char  v[2] = {0, 127};
    ASSERT_EQ(ccenv_interp_raw(t, v, 2, -10),    0);
    ASSERT_EQ(ccenv_interp_raw(t, v, 2,   0),    0);
    ASSERT_EQ(ccenv_interp_raw(t, v, 2,  32),   63);
    ASSERT_EQ(ccenv_interp_raw(t, v, 2,  64),  127);
    ASSERT_EQ(ccenv_interp_raw(t, v, 2, 200),  127);
}

static void test_interp_single(void) {
    unsigned short t[1] = {0};
    unsigned char  v[1] = {64};
    ASSERT_EQ(ccenv_interp_raw(t, v, 1, 0),   64);
    ASSERT_EQ(ccenv_interp_raw(t, v, 1, 999), 64);
}

static void test_interp_vstep(void) {
    unsigned short t[2] = {16, 16};
    unsigned char  v[2] = {0,  127};
    ASSERT_EQ(ccenv_interp_raw(t, v, 2, 16), 127);
}

static void test_interp_multi(void) {
    unsigned short t[4] = {0, 16, 32, 48};
    unsigned char  v[4] = {0, 127, 0,  64};
    ASSERT_EQ(ccenv_interp_raw(t, v, 4,  0),   0);
    ASSERT_EQ(ccenv_interp_raw(t, v, 4,  8),  63);
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 16), 127);
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 32),   0);
    ASSERT_EQ(ccenv_interp_raw(t, v, 4, 48),  64);
}

static void test_step_disabled(void) {
    unsigned short t[2] = {0, 64};
    unsigned char  v[2] = {0, 127};
    struct ccenv_voice_state vs = {0, 0, 0};
    int got = ccenv_step(&vs, t, v, 2, 0, 0, 1, 0, 1);
    ASSERT_EQ(got, 0);
    ASSERT_EQ(vs.done, 1);
}

static void test_step_terminate(void) {
    unsigned short t[2] = {0, 4};
    unsigned char  v[2] = {0, 127};
    struct ccenv_voice_state vs = {0, 0, 0};
    int last = 0;
    for (int i = 0; i < 10; i++)
        last = ccenv_step(&vs, t, v, 2, CCENV_F_ENABLED, 0, 1, 0, 1);
    ASSERT_EQ(vs.position, 4);
    ASSERT_EQ(vs.done,     1);
    ASSERT_EQ(last,        127);
}

static void test_step_sustain_holds(void) {
    unsigned short t[3] = {0, 2, 6};
    unsigned char  v[3] = {0, 127, 0};
    struct ccenv_voice_state vs = {0, 0, 0};
    int last = 0;
    for (int i = 0; i < 50; i++) {
        last = ccenv_step(&vs, t, v, 3,
                          CCENV_F_ENABLED | CCENV_F_SUSTAIN,
                          0, 2, 1, 1);
    }
    ASSERT_EQ(vs.done, 0);
    ASSERT_EQ(vs.position, 2);
    ASSERT_EQ(last, 127);
}

static void test_step_sustain_release(void) {
    unsigned short t[3] = {0, 4, 8};
    unsigned char  v[3] = {0, 127, 0};
    struct ccenv_voice_state vs = {0, 0, 0};
    for (int i = 0; i < 20; i++)
        ccenv_step(&vs, t, v, 3,
                   CCENV_F_ENABLED | CCENV_F_SUSTAIN, 0, 2, 1, 1);
    ASSERT_EQ(vs.position, 4);
    ASSERT_EQ(vs.done, 0);
    vs.key_off = 1;
    int last = 0;
    for (int i = 0; i < 20; i++)
        last = ccenv_step(&vs, t, v, 3,
                          CCENV_F_ENABLED | CCENV_F_SUSTAIN, 0, 2, 1, 1);
    ASSERT_EQ(vs.position, 8);
    ASSERT_EQ(vs.done, 1);
    ASSERT_EQ(last, 0);
}

static void test_step_loop(void) {
    unsigned short t[2] = {0, 4};
    unsigned char  v[2] = {0, 127};
    struct ccenv_voice_state vs = {0, 0, 0};
    int saw_127 = 0, then_0 = 0, was_127 = 0;
    for (int i = 0; i < 20; i++) {
        int val = ccenv_step(&vs, t, v, 2,
                             CCENV_F_ENABLED | CCENV_F_LOOP, 0, 1, 0, 0);
        if (val == 127) { saw_127 = 1; was_127 = 1; }
        else if (was_127 && val == 0) then_0 = 1;
    }
    ASSERT_EQ(vs.done, 0);
    ASSERT(saw_127);
    ASSERT(then_0);
}

int main(void) {
    printf("test_ccenv: starting\n");
    test_interp_clamp();
    test_interp_single();
    test_interp_vstep();
    test_interp_multi();
    test_step_disabled();
    test_step_terminate();
    test_step_sustain_holds();
    test_step_sustain_release();
    test_step_loop();
    if (g_failures == 0) {
        printf("test_ccenv: all good\n");
        return 0;
    }
    printf("test_ccenv: %d failure(s)\n", g_failures);
    return 1;
}
