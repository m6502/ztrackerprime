/*************************************************************************
 * midi_clock_sync.cpp
 *
 * See midi_clock_sync.h. Layout mirrors ableton_link.cpp:
 *   1. observer counters (written by the MIDI-in thread, read here)
 *   2. main-thread pump: transport consume -> tempo estimate -> phase chase
 *
 * The unit test (tests/test_midi_clock_sync.cpp) #includes this file with
 * ZT_TEST_NO_SDL: zt.h is replaced by the fake app environment and the
 * time source by a pokable counter, so every path is drivable headlessly.
 *************************************************************************/
#include "midi_clock_sync.h"
#include "phase_chase.h"

#include <cmath>   // lround

#ifdef ZT_TEST_NO_SDL
#include "zt_link_stub.h"     // tests/: fake player, song, conf, globals
static unsigned g_test_now_ms = 0;          // pokable by the unit test
static unsigned mclk_now_ms(void) { return g_test_now_ms; }
#else
#include "zt.h"               // ztPlayer, song, zt_config_globals, cur_edit_pattern
static unsigned mclk_now_ms(void) { return (unsigned)SDL_GetTicks(); }
#endif

// ---------------------------------------------------------------------------
// Observer counters (MIDI-in thread writes, pump reads).

int      g_mclk_clocks        = 0;
unsigned g_mclk_last_clock_ms = 0;
int      g_mclk_start_req     = 0;
int      g_mclk_continue_req  = 0;
int      g_mclk_stop_req      = 0;
int      g_mclk_spp_raw       = 0;
int      g_mclk_spp_seq       = 0;

// ---------------------------------------------------------------------------
// Pump state (main thread only).

// Master tempo estimate: clock-count/wall-time samples ~250 ms apart over a
// ~1 s sliding window. Replaces the old per-interval ring that lived on the
// MIDI thread; the window average is immune to single-clock jitter.
#define MCLK_SAMPLES        5
#define MCLK_SAMPLE_MS      250
#define MCLK_DROPOUT_MS     1000
static int      s_sample_clocks[MCLK_SAMPLES];
static unsigned s_sample_ms[MCLK_SAMPLES];
static int      s_sample_count = 0;     // 0..MCLK_SAMPLES, ring fill
static int      s_sample_head  = 0;
static double   s_tempo_est    = 0.0;   // 0 = no estimate yet
static int      s_last_nominal_bpm = 0;

// Position chase: clock count at the consumed START/CONTINUE.
static int      s_anchor_clocks = 0;
static bool     s_chase_valid   = false;

// Transport request baselines + the queued SPP position (mirrors the old
// static queued_row/queued_order in midi-io.cpp: SPP sets it, START zeroes
// it, CONTINUE plays from it, and it persists across CONTINUEs).
static int s_last_start = -1, s_last_cont = -1, s_last_stop = -1;
static int s_last_spp_seq = -1;
static int s_queued_row = 0, s_queued_order = 0;

static void mclk_reset_estimate(void) {
    s_sample_count = 0;
    s_sample_head  = 0;
    s_tempo_est    = 0.0;
    s_last_nominal_bpm = 0;
}

static void mclk_update_estimate(unsigned now) {
    // Dropout: master stopped sending clocks -> forget everything and stop
    // chasing until clocks resume (rebuild a clean window like the old ring).
    unsigned last_clock = g_mclk_last_clock_ms;
    if (last_clock == 0 || (now - last_clock) > MCLK_DROPOUT_MS) {
        mclk_reset_estimate();
        return;
    }
    int tail = (s_sample_head + MCLK_SAMPLES - s_sample_count) % MCLK_SAMPLES;
    if (s_sample_count == 0 ||
        now - s_sample_ms[(s_sample_head + MCLK_SAMPLES - 1) % MCLK_SAMPLES] >= MCLK_SAMPLE_MS) {
        s_sample_clocks[s_sample_head] = g_mclk_clocks;
        s_sample_ms[s_sample_head]     = now;
        s_sample_head = (s_sample_head + 1) % MCLK_SAMPLES;
        if (s_sample_count < MCLK_SAMPLES) s_sample_count++;
        tail = (s_sample_head + MCLK_SAMPLES - s_sample_count) % MCLK_SAMPLES;
    }
    if (s_sample_count < 2) return;
    int      head_i  = (s_sample_head + MCLK_SAMPLES - 1) % MCLK_SAMPLES;
    int      dclocks = s_sample_clocks[head_i] - s_sample_clocks[tail];
    unsigned dms     = s_sample_ms[head_i] - s_sample_ms[tail];
    if (dclocks <= 0 || dms == 0) return;
    // 24 clocks per beat: BPM = clocks/24 per minute.
    double est = ((double)dclocks / 24.0) * 60000.0 / (double)dms;
    if (est >= 20.0 && est <= 500.0)
        s_tempo_est = est;
}

static void mclk_consume_transport(void) {
    int s  = g_mclk_start_req;
    int c  = g_mclk_continue_req;
    int st = g_mclk_stop_req;
    int sq = g_mclk_spp_seq;
    bool start_intent = (s_last_start >= 0 && s != s_last_start);
    bool cont_intent  = (s_last_cont  >= 0 && c != s_last_cont);
    bool stop_intent  = (s_last_stop  >= 0 && st != s_last_stop);
    bool spp_changed  = (s_last_spp_seq >= 0 && sq != s_last_spp_seq);
    s_last_start = s; s_last_cont = c; s_last_stop = st; s_last_spp_seq = sq;

    if (spp_changed) {
        if (ztPlayer->calc_pos(g_mclk_spp_raw, &s_queued_row, &s_queued_order)) {
            s_queued_row   = 0;
            s_queued_order = 0;
        }
    }
    if (stop_intent) {
        ztPlayer->stop();
        s_chase_valid = false;
    }
    if (start_intent || cont_intent) {
        if (start_intent) {                  // START always means position 0
            s_queued_row   = 0;
            s_queued_order = 0;
        }
        if (ztPlayer->playing)
            ztPlayer->stop();
        // Same target rules the MIDI-thread handler used (midi-io.cpp,
        // pre-refactor): an SPP-queued position plays from there; otherwise
        // play the song from the start when the orderlist has entries, else
        // loop the current pattern.
        if (s_queued_row || s_queued_order) {
            ztPlayer->play(s_queued_row, s_queued_order, 3);
        } else if (song->orderlist[0] < 0x100) {
            ztPlayer->play(0, cur_edit_pattern, 1);
        } else {
            ztPlayer->play(0, cur_edit_pattern, 0);
        }
        s_anchor_clocks = g_mclk_clocks;     // expected position 0 is NOW
        s_chase_valid   = true;
    }
}

static void mclk_chase(unsigned now) {
    if (!zt_config_globals.midi_in_sync_chase_tempo) {
        // Transport-only mode: never touch the engine rate (matches the old
        // behavior where the tempo ring was gated on this flag).
        ztPlayer->chase_skew_us = 0;
        s_chase_valid = false;
        return;
    }
    if (!ztPlayer->playing || s_tempo_est <= 0.0) {
        ztPlayer->chase_skew_us = 0;
        return;
    }
    // Keep the engine nominal on the rounded master tempo (the old
    // chase_external_tempo contract: never writes song->bpm). The fractional
    // remainder is the chase's rate term.
    int bpm_int = (int)lround(s_tempo_est);
    if (bpm_int != s_last_nominal_bpm) {
        ztPlayer->chase_external_tempo(bpm_int);
        s_last_nominal_bpm = bpm_int;
    }
    if (!s_chase_valid) {
        // Started outside a consumed START (e.g. local F6 while slaved):
        // anchor at the current position so the lock is drift-only.
        s_anchor_clocks = g_mclk_clocks - ztPlayer->played_subticks / 4;
        s_chase_valid   = true;
        return;
    }
    // Expected position: 4 subticks per clock, plus intra-clock
    // interpolation from wall time so the reference doesn't advance in
    // 4-subtick stairs (~20 ms at 120 BPM) that would kick the controller
    // out of its steady-state deadband.
    double clock_period_ms = 60000.0 / (24.0 * s_tempo_est);
    double frac = (double)(now - g_mclk_last_clock_ms) / clock_period_ms * 4.0;
    if (frac < 0.0) frac = 0.0;
    if (frac > 3.99) frac = 3.99;
    // sync_offset_ms: play early so notes monitored through the master
    // DAW's audio chain sound on the grid -- live-dialable, same as Link.
    double off_subticks = (double)zt_config_globals.sync_offset_ms
                          * s_tempo_est / 60000.0 * 96.0;
    double expected = (double)(g_mclk_clocks - s_anchor_clocks) * 4.0
                      + frac + off_subticks;
    double exact_us   = 60000000.0 / (96.0 * s_tempo_est);
    double nominal_us = (double)(ztPlayer->subtick_len_ms + ztPlayer->subtick_len_mms);
    ztPlayer->chase_skew_us = phase_chase_step(expected, ztPlayer->played_subticks,
                                               exact_us, nominal_us);
}

void zt_midi_clock_pump(void) {
    if (!ztPlayer) return;
    if (!zt_config_globals.midi_in_sync) {
        // Track baselines while disabled so a pile of requests received
        // before enabling doesn't replay as intents; leave chase_skew_us
        // alone -- the Link pump may own it.
        s_last_start   = g_mclk_start_req;
        s_last_cont    = g_mclk_continue_req;
        s_last_stop    = g_mclk_stop_req;
        s_last_spp_seq = g_mclk_spp_seq;
        s_chase_valid  = false;
        mclk_reset_estimate();
        return;
    }
    unsigned now = mclk_now_ms();
    mclk_consume_transport();
    mclk_update_estimate(now);
    mclk_chase(now);
}
