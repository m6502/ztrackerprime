// Unit tests for src/midi_clock_sync.cpp.
//
// Compiled with ZT_TEST_NO_SDL: the fake app environment comes from
// zt_link_stub.h and the time source is the pokable g_test_now_ms, so the
// whole pump -- transport request consumption, the sliding-window tempo
// estimate, dropout handling, and the phase chase -- runs headlessly.
// Because this file #includes midi_clock_sync.cpp, the pump's file-statics
// (s_tempo_est, s_anchor_clocks, ...) are directly readable and pokable.
//
// The simulated master sends clocks by bumping g_mclk_clocks /
// g_mclk_last_clock_ms exactly as the MIDI-in thread does.

#include "midi_clock_sync.h"
#include "midi_clock_sync.cpp"

#include <stdio.h>
#include <math.h>

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

// Simulate the master running for `ms` at `bpm`: advance wall time and the
// clock counter in small steps, pumping each step like the main loop would.
static void run_master(unsigned ms, double bpm) {
    double clock_period = 60000.0 / (24.0 * bpm);
    static double clock_acc = 0.0;
    unsigned step = 10;
    for (unsigned t = 0; t < ms; t += step) {
        g_test_now_ms += step;
        clock_acc += step / clock_period;
        while (clock_acc >= 1.0) {
            g_mclk_clocks++;
            g_mclk_last_clock_ms = g_test_now_ms;
            clock_acc -= 1.0;
        }
        zt_midi_clock_pump();
    }
}

static void reset_env(void) {
    *ztPlayer = player();
    *song = zt_module();
    zt_config_globals = zt_conf();
    zt_config_globals.midi_in_sync             = 1;
    zt_config_globals.midi_in_sync_chase_tempo = 1;
    cur_edit_pattern = 0;
    zt_midi_clock_pump();   // record request baselines, reset estimate
}

static void test_disabled_requests_do_not_replay() {
    *ztPlayer = player();
    zt_config_globals = zt_conf();        // midi_in_sync = 0
    g_mclk_start_req += 3;                // master spammed START while off
    zt_midi_clock_pump();                 // disabled: baselines track
    zt_config_globals.midi_in_sync = 1;
    zt_midi_clock_pump();
    CHECK(ztPlayer->play_immediately_calls == 0);   // nothing replayed
}

static void test_start_plays_song_from_top() {
    reset_env();
    song->orderlist[0] = 5;               // orderlist has entries
    g_mclk_start_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->play_immediately_calls == 1);
    CHECK(ztPlayer->last_row == 0);
    CHECK(ztPlayer->last_pm  == 1);       // song mode
    CHECK(ztPlayer->playing  == 1);
}

static void test_start_loops_pattern_on_empty_orderlist() {
    reset_env();
    song->orderlist[0] = 0x100;           // empty song
    cur_edit_pattern = 7;
    g_mclk_start_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->last_pat == 7);
    CHECK(ztPlayer->last_pm  == 0);       // loop current pattern
}

static void test_stop_stops() {
    reset_env();
    g_mclk_start_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->playing == 1);
    g_mclk_stop_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->playing == 0);
    CHECK(ztPlayer->stop_calls >= 1);
}

static void test_spp_continue_and_start_reset() {
    reset_env();
    ztPlayer->calc_pos_row   = 8;         // scripted SPP mapping
    ztPlayer->calc_pos_order = 2;
    g_mclk_spp_raw = 64;
    g_mclk_spp_seq++;
    g_mclk_continue_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->last_row == 8);       // CONTINUE honors the SPP position
    CHECK(ztPlayer->last_pat == 2);
    CHECK(ztPlayer->last_pm  == 3);
    // A second CONTINUE reuses the same queued position.
    g_mclk_continue_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->last_row == 8);
    // START resets to the top.
    g_mclk_start_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->last_row == 0);
    CHECK(ztPlayer->last_pm  == 1);
}

static void test_tempo_estimate_fractional_no_song_bpm_write() {
    reset_env();
    song->bpm = 99;                       // sentinel: must never change
    g_mclk_start_req++;
    zt_midi_clock_pump();                 // playing (nominal adopted 99)
    run_master(2000, 120.4);
    CHECK(s_tempo_est > 119.4 && s_tempo_est < 121.4);
    CHECK(ztPlayer->bpm == 120);          // nominal follows rounded master
    CHECK(song->bpm == 99);               // chase never writes song->bpm
}

static void test_position_chase_locks_and_lags() {
    reset_env();
    g_mclk_start_req++;
    zt_midi_clock_pump();
    run_master(2000, 120.0);
    // Engine kept pace exactly: 4 subticks per clock since the anchor.
    ztPlayer->played_subticks = (g_mclk_clocks - s_anchor_clocks) * 4;
    g_mclk_last_clock_ms = g_test_now_ms; // fresh clock: no interpolation
    zt_midi_clock_pump();
    int locked = ztPlayer->chase_skew_us;
    CHECK(locked >= -17 && locked <= 17); // rate term + steady feedback only
    // Engine half a beat behind: move-tier catch-up (negative skew).
    ztPlayer->played_subticks -= 48;
    zt_midi_clock_pump();
    CHECK(ztPlayer->chase_skew_us < -16);
    CHECK(ztPlayer->chase_skew_us >= -630);
}

static void test_offset_shifts_setpoint() {
    reset_env();
    g_mclk_start_req++;
    zt_midi_clock_pump();
    run_master(2000, 120.0);
    ztPlayer->played_subticks = (g_mclk_clocks - s_anchor_clocks) * 4;
    g_mclk_last_clock_ms = g_test_now_ms;
    zt_midi_clock_pump();
    CHECK(ztPlayer->chase_skew_us >= -17 && ztPlayer->chase_skew_us <= 17);
    // 171 ms offset: the engine must move EARLIER -> it is suddenly behind
    // the shifted setpoint -> aggressive negative skew.
    zt_config_globals.sync_offset_ms = 171;
    zt_midi_clock_pump();
    CHECK(ztPlayer->chase_skew_us < -16);
}

static void test_dropout_pauses_chase_then_warm_relocks() {
    reset_env();
    g_mclk_start_req++;
    zt_midi_clock_pump();
    run_master(2000, 120.0);
    CHECK(s_tempo_est > 0.0);
    ztPlayer->chase_skew_us = 77;
    g_test_now_ms += 1500;                // no clocks for 1.5 s
    zt_midi_clock_pump();
    CHECK(ztPlayer->chase_skew_us == 0);  // chase paused: engine runs free at nominal
    // Warm re-lock (Tier 2): the sample window is dropped but the last tempo is
    // PRESERVED, so a brief dropout re-locks instantly instead of cold-starting.
    CHECK(s_tempo_est > 119.0 && s_tempo_est < 121.0);   // tempo kept warm
    run_master(2000, 120.0);              // clocks resume
    CHECK(s_tempo_est > 119.0 && s_tempo_est < 121.0);   // still locked
}

static void test_snap_on_large_jump() {
    reset_env();
    g_mclk_start_req++;
    zt_midi_clock_pump();
    run_master(2000, 120.0);
    ztPlayer->played_subticks = (g_mclk_clocks - s_anchor_clocks) * 4;
    g_mclk_last_clock_ms = g_test_now_ms;
    zt_midi_clock_pump();                       // locked
    CHECK(ztPlayer->chase_skew_us >= -17 && ztPlayer->chase_skew_us <= 17);
    // Simulate a main-loop stall: the engine fell ~1.5 s (3 beats) behind the
    // master grid -- far past any deliberate move. Tier 2 should SNAP the
    // anchor (skew 0, error collapses) rather than slew for many seconds.
    ztPlayer->played_subticks -= 96 * 3;
    ztPlayer->chase_skew_us = 1234;             // sentinel that must be cleared
    zt_midi_clock_pump();
    CHECK(ztPlayer->chase_skew_us == 0);        // snapped, not a huge slew
    // Next frame after the snap the engine is re-anchored: tiny residual error.
    g_mclk_last_clock_ms = g_test_now_ms;
    zt_midi_clock_pump();
    CHECK(s_offset_ms > -30.0 && s_offset_ms < 30.0);
}

// Closed-loop simulation: a fractional-tempo master streams clocks while the
// engine advances played_subticks at its OWN steered rate (nominal subtick +
// the chase's skew, exactly as the real timer does). The pump steers each ms.
// optional `jitter_ms`: deterministically perturb the clock timestamp to model
// input timing noise. Returns the worst |offset| (ms) seen after settling.
static double run_closed_loop(double master_bpm, int total_ms, int jitter_ms) {
    const double nominal_us = 5208.333;              // stub subtick (120 nominal)
    double clock_period_ms  = 60000.0 / (24.0 * master_bpm);
    double clock_acc = 0.0, subtick_acc_us = 0.0, worst = 0.0;
    unsigned jseed = 12345;                          // deterministic, no rng
    for (int ms = 0; ms < total_ms; ms++) {
        g_test_now_ms += 1;
        clock_acc += 1.0 / clock_period_ms;
        while (clock_acc >= 1.0) {
            g_mclk_clocks++;
            unsigned stamp = g_test_now_ms;
            if (jitter_ms) {                          // +/- jitter_ms, deterministic
                jseed = jseed * 1103515245u + 12345u;
                int j = (int)((jseed >> 16) % (2u * jitter_ms + 1u)) - jitter_ms;
                stamp = (unsigned)((int)g_test_now_ms + j);
            }
            g_mclk_last_clock_ms = stamp;
            clock_acc -= 1.0;
        }
        double actual_us = nominal_us + (double)ztPlayer->chase_skew_us;
        if (actual_us < 1000.0) actual_us = 1000.0;
        subtick_acc_us += 1000.0;                     // 1 ms
        while (subtick_acc_us >= actual_us) { ztPlayer->played_subticks++; subtick_acc_us -= actual_us; }
        zt_midi_clock_pump();
        if (ms > total_ms / 2) {                      // measure after settling
            double a = s_offset_ms < 0 ? -s_offset_ms : s_offset_ms;
            if (a > worst) worst = a;
        }
    }
    return worst;
}

static void test_sim_fractional_tempo_locks() {
    reset_env();
    g_mclk_start_req++;
    zt_midi_clock_pump();                              // START, anchor now
    // A fractional master the old int-BPM chase would drift on (~0.3%/min):
    // the phase-lock holds it within a few ms of the grid.
    double worst = run_closed_loop(120.37, 6000, /*jitter_ms=*/0);
    CHECK(worst < 12.0);
}

static void test_sim_jittery_clock_stays_bounded() {
    reset_env();
    g_mclk_start_req++;
    zt_midi_clock_pump();
    // +/-3 ms timestamp jitter (USB / scheduler) must not unlock the chase.
    double worst = run_closed_loop(132.5, 6000, /*jitter_ms=*/3);
    CHECK(worst < 25.0);
}

static void test_transport_only_mode_never_touches_rate() {
    reset_env();
    zt_config_globals.midi_in_sync_chase_tempo = 0;
    g_mclk_start_req++;
    zt_midi_clock_pump();
    CHECK(ztPlayer->playing == 1);        // transport still follows
    int nominal = ztPlayer->bpm;
    run_master(2000, 140.0);
    CHECK(ztPlayer->bpm == nominal);      // rate untouched
    CHECK(ztPlayer->chase_skew_us == 0);
    CHECK(ztPlayer->chase_external_tempo_calls == 0);
}

int main(void) {
    test_disabled_requests_do_not_replay();
    test_start_plays_song_from_top();
    test_start_loops_pattern_on_empty_orderlist();
    test_stop_stops();
    test_spp_continue_and_start_reset();
    test_tempo_estimate_fractional_no_song_bpm_write();
    test_position_chase_locks_and_lags();
    test_offset_shifts_setpoint();
    test_dropout_pauses_chase_then_warm_relocks();
    test_snap_on_large_jump();
    test_sim_fractional_tempo_locks();
    test_sim_jittery_clock_stays_bounded();
    test_transport_only_mode_never_touches_rate();
    printf("midi_clock_sync: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
