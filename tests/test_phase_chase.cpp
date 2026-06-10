// Unit tests for src/phase_chase.h -- the closed-loop position controller
// shared by the Ableton Link and MIDI clock chases. Pure math, no stubs.
//
// Reference numbers (120 BPM): exact subtick 60e6/(96*120) = 5208.333 us,
// engine nominal (set_speed truncation) 5208 us, so the rate term alone
// rounds to 0; steady-tier feedback bound = 5208.33*0.003 = 15.6 us;
// move-tier bound = 5208.33*0.12 = 625 us; total clamp 15% = 781 us.

#include "phase_chase.h"

#include <stdio.h>

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

static const double EXACT_120   = 60000000.0 / (96.0 * 120.0);
static const double NOMINAL_120 = 5208.0;

static void test_zero_error_locks_at_rate_term() {
    // In lock, the only output is the rate term (0.33 us -> rounds to 0).
    CHECK(phase_chase_step(96.0, 96, EXACT_120, NOMINAL_120) == 0);
    CHECK(phase_chase_step(0.0, 0, EXACT_120, NOMINAL_120) == 0);
}

static void test_steady_tier_sign_and_clamp() {
    // 1 subtick (~5.2 ms) of error: inside the deadband, 0.3% authority.
    int behind = phase_chase_step(96.0, 95, EXACT_120, NOMINAL_120);
    CHECK(behind < 0);            // engine behind -> shorter subticks
    CHECK(behind >= -16);         // clamped at the steady bound
    int ahead = phase_chase_step(96.0, 97, EXACT_120, NOMINAL_120);
    CHECK(ahead > 0);             // engine ahead -> longer subticks
    CHECK(ahead <= 16);
}

static void test_move_tier_sign_and_clamp() {
    // Half a beat (250 ms): a deliberate move, 12% authority.
    int behind = phase_chase_step(96.0, 48, EXACT_120, NOMINAL_120);
    CHECK(behind < -16);
    CHECK(behind >= -626);
    int ahead = phase_chase_step(96.0, 144, EXACT_120, NOMINAL_120);
    CHECK(ahead > 16);
    CHECK(ahead <= 626);
}

static void test_deadband_boundary() {
    // Just under 10 ms (1.9 subticks behind): steady tier caps the
    // catch-up at -16. Just over (2.1): move tier engages (err/64 ~ -171).
    int inside  = phase_chase_step(96.0 + 1.9, 96, EXACT_120, NOMINAL_120);
    CHECK(inside < 0 && inside >= -16);
    int outside = phase_chase_step(96.0 + 2.1, 96, EXACT_120, NOMINAL_120);
    CHECK(outside < -16);
}

static void test_rate_term_fractional_master() {
    // Master at 119.6, engine nominal at int-120: zero position error must
    // still output the exact-vs-nominal difference (+17.75 -> 18).
    double exact_1196 = 60000000.0 / (96.0 * 119.6);
    int skew = phase_chase_step(96.0, 96, exact_1196, NOMINAL_120);
    CHECK(skew >= 17 && skew <= 19);
}

static void test_total_clamp() {
    // Pathological nominal mismatch: output is capped at 15% of the exact
    // subtick no matter what.
    int skew = phase_chase_step(0.0, 0, EXACT_120, NOMINAL_120 * 0.5);
    CHECK(skew <= (int)(EXACT_120 * 0.15) + 1);
    skew = phase_chase_step(0.0, 0, EXACT_120, NOMINAL_120 * 2.0);
    CHECK(skew >= -(int)(EXACT_120 * 0.15) - 1);
}

int main(void) {
    test_zero_error_locks_at_rate_term();
    test_steady_tier_sign_and_clamp();
    test_move_tier_sign_and_clamp();
    test_deadband_boundary();
    test_rate_term_fractional_master();
    test_total_clamp();
    printf("phase_chase: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
