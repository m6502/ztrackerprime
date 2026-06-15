/*************************************************************************
 * phase_chase.h
 *
 * The closed-loop position controller shared by the external-sync chases
 * (Ableton Link in ableton_link.cpp, MIDI clock in midi_clock_sync.cpp).
 * Pure logic, no app or SDL dependencies, exhaustively unit-tested
 * (tests/test_phase_chase.cpp) -- same pattern as preset_selector.h.
 *
 * The caller compares two clocks each frame: where the master timeline
 * says the engine should be (expected subticks since some anchor) versus
 * the engine's own odometer (player::played_subticks), and applies the
 * returned skew to player::chase_skew_us. Any residual rate error -- int
 * BPM rounding, set_speed()'s whole-us truncation, a peer machine's
 * crystal -- integrates into position error, which the loop cancels:
 *
 *   skew = (exact master subtick - engine nominal subtick)   rate term
 *        - bounded P feedback on the position error
 *
 * Feedback authority is tiered: inside PHASE_CHASE_DEADBAND_US the
 * correction stays far below audibility; beyond it the error is a
 * deliberate move (offset slider, tempo step) and may slew hard -- the
 * output is MIDI, so a brief rush while the user turns a knob is
 * harmless. Positive position error = engine behind = shorter subticks.
 *************************************************************************/
#ifndef ZT_PHASE_CHASE_H
#define ZT_PHASE_CHASE_H

#include <cmath>   // lround

#define PHASE_CHASE_DEADBAND_US   10000.0   /* steady-state vs deliberate-move */
#define PHASE_CHASE_FB_STEADY     0.003     /* inaudible lock corrections */
#define PHASE_CHASE_FB_MOVE       0.12      /* ~171 ms move locks in ~1.4 s */
#define PHASE_CHASE_SKEW_CLAMP    0.15      /* absolute authority ceiling */

static inline int phase_chase_step(double expected_subticks,
                                   int    played_subticks,
                                   double exact_subtick_us,
                                   double nominal_subtick_us)
{
    double pos_err  = (expected_subticks - (double)played_subticks) * exact_subtick_us;
    double feedback = pos_err / 64.0;
    double fb_max   = exact_subtick_us *
        ((pos_err > PHASE_CHASE_DEADBAND_US || pos_err < -PHASE_CHASE_DEADBAND_US)
             ? PHASE_CHASE_FB_MOVE : PHASE_CHASE_FB_STEADY);
    if (feedback >  fb_max) feedback =  fb_max;
    if (feedback < -fb_max) feedback = -fb_max;
    double skew = (exact_subtick_us - nominal_subtick_us) - feedback;
    double skew_max = exact_subtick_us * PHASE_CHASE_SKEW_CLAMP;
    if (skew >  skew_max) skew =  skew_max;
    if (skew < -skew_max) skew = -skew_max;
    return (int)lround(skew);
}

#endif /* ZT_PHASE_CHASE_H */
