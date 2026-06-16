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

#include <cmath>   // lround, fabs

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

std::atomic<int>      g_mclk_clocks        {0};
std::atomic<unsigned> g_mclk_last_clock_ms {0};
std::atomic<int>      g_mclk_start_req     {0};
std::atomic<int>      g_mclk_continue_req  {0};
std::atomic<int>      g_mclk_stop_req      {0};
std::atomic<int>      g_mclk_spp_raw       {0};
std::atomic<int>      g_mclk_spp_seq       {0};

// ---------------------------------------------------------------------------
// Pump state (main thread only).

// Master tempo estimate: clock-count/wall-time samples ~250 ms apart over a
// ~1 s sliding window. Replaces the old per-interval ring that lived on the
// MIDI thread; the window average is immune to single-clock jitter.
#define MCLK_SAMPLES        5
#define MCLK_SAMPLE_MS      250
#define MCLK_DROPOUT_MS     1000
#define MCLK_RETUNE_MS      500   /* estimate must hold out-of-band this long before the nominal moves */
// Position error past this is a discontinuity (a multi-frame stall, not a
// deliberate move) -> snap instead of slew. Set well above the controller's
// normal operating range: a half-beat catch-up and the 0-500 ms Sync Offset
// are deliberate moves the move-tier is meant to slew, so the snap floor sits
// at ~1 s to catch only genuine discontinuities.
#define MCLK_SNAP_MS        1000.0
static int      s_sample_clocks[MCLK_SAMPLES];
static unsigned s_sample_ms[MCLK_SAMPLES];
static int      s_sample_count = 0;     // 0..MCLK_SAMPLES, ring fill
static int      s_sample_head  = 0;
static double   s_tempo_est    = 0.0;   // 0 = no estimate yet
static int      s_last_nominal_bpm = 0;
static unsigned s_retune_since = 0;     // when the estimate left the band; 0 = in band

// Position chase: clock count at the consumed START/CONTINUE.
static int      s_anchor_clocks = 0;
static bool     s_chase_valid   = false;

// User-facing status (F11 lock meter + Lua), refreshed at the end of the pump.
static int      s_state     = ZT_SYNC_OFF;
static double   s_offset_ms = 0.0;          // signed position error, 0 unless chasing

// Transport request baselines + the queued SPP position (mirrors the old
// static queued_row/queued_order in midi-io.cpp: SPP sets it, START zeroes
// it, CONTINUE plays from it, and it persists across CONTINUEs).
static int s_last_start = -1, s_last_cont = -1, s_last_stop = -1;
static int s_last_spp_seq = -1;
static int s_queued_row = 0, s_queued_order = 0;

// keep_tempo: drop the sample window but PRESERVE s_tempo_est, so a brief
// dropout warm-re-locks from the last known tempo instead of cold-starting
// (~1 s of wander). Cold reset (keep_tempo=false) only when sync is disabled.
static void mclk_reset_estimate(bool keep_tempo) {
    s_sample_count = 0;
    s_sample_head  = 0;
    s_retune_since = 0;
    if (!keep_tempo) {
        s_tempo_est        = 0.0;
        s_last_nominal_bpm = 0;
    }
}

static void mclk_update_estimate(unsigned now, int clocks, unsigned edge,
                                 bool clocks_recent) {
    // Dropout: master stopped sending clocks. Drop the stale window but keep
    // the last tempo (warm re-lock); the chase stops separately.
    if (!clocks_recent) {
        mclk_reset_estimate(/*keep_tempo=*/true);
        return;
    }
    // Samples pair the clock count with the clock's ARRIVAL time (edge), not
    // pump time: the pump observes up to one clock period (~20 ms at 120 BPM)
    // after the edge, and that jitter on the window endpoints is worth several
    // BPM of hunting over a 1 s window. On macOS the edge is a hardware
    // timestamp (midi_timestamp.h), so it is jitter-free at the source.
    int head_i = (s_sample_head + MCLK_SAMPLES - 1) % MCLK_SAMPLES;
    if ((s_sample_count == 0 && now - edge <= MCLK_SAMPLE_MS) ||
        (s_sample_count > 0 && clocks != s_sample_clocks[head_i] &&
         edge - s_sample_ms[head_i] >= MCLK_SAMPLE_MS)) {
        s_sample_clocks[s_sample_head] = clocks;
        s_sample_ms[s_sample_head]     = edge;
        s_sample_head = (s_sample_head + 1) % MCLK_SAMPLES;
        if (s_sample_count < MCLK_SAMPLES) s_sample_count++;
    }
    if (s_sample_count < 2) return;
    // Least-squares slope (clocks per ms) over the whole window: a single
    // jitter-delayed edge timestamp only carries 1/n of the weight instead of
    // swinging the estimate as a raw endpoint would.
    int    n      = s_sample_count;
    int    base_i = (s_sample_head + MCLK_SAMPLES - n) % MCLK_SAMPLES;
    int    c0     = s_sample_clocks[base_i];
    unsigned t0   = s_sample_ms[base_i];
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (int k = 0; k < n; k++) {
        int i = (base_i + k) % MCLK_SAMPLES;
        double x = (double)(s_sample_ms[i] - t0);
        double y = (double)(s_sample_clocks[i] - c0);
        sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    double den = (double)n * sxx - sx * sx;
    if (den <= 0.0) return;
    // 24 clocks per beat: BPM = clocks/24 per minute.
    double est = ((double)n * sxy - sx * sy) / den / 24.0 * 60000.0;
    if (est >= 20.0 && est <= 500.0)
        s_tempo_est = est;
}

static void mclk_consume_transport(int clocks_now) {
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
        // The transport gap freezes the clock count while wall time runs: a
        // sample window spanning it would read "fewer clocks over more time"
        // and dip the estimate on restart. Drop the window now (the tempo
        // estimate itself survives for the restart retune).
        mclk_reset_estimate(/*keep_tempo=*/true);
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
        // Same gap hazard as the stop path (a master can restart without ever
        // sending STOP): start the window fresh.
        mclk_reset_estimate(/*keep_tempo=*/true);
        s_anchor_clocks = clocks_now;        // expected position 0 is NOW
        s_chase_valid   = true;
        // play() adopted song->bpm, wiping any chased tempo. Force the next
        // chase pass to re-tune from the retained estimate (nominal == 0).
        s_last_nominal_bpm = 0;
    }
}

static void mclk_chase(unsigned now, int clocks, unsigned last_clock,
                       bool clocks_recent) {
    if (!zt_config_globals.midi_in_sync_chase_tempo) {
        // Transport-only mode: never touch the engine rate (matches the old
        // behavior where the tempo ring was gated on this flag).
        ztPlayer->chase_skew_us = 0;
        s_chase_valid = false;
        s_offset_ms   = 0.0;
        return;
    }
    if (!ztPlayer->playing || s_tempo_est <= 0.0) {
        ztPlayer->chase_skew_us = 0;
        s_offset_ms = 0.0;
        return;
    }
    // Keep the engine nominal on the rounded master tempo (chase_external_tempo
    // never writes song->bpm); the fractional remainder is the chase's rate
    // term, so the nominal only needs to be CLOSE. Require >0.6 BPM of
    // disagreement (a master sitting on a rounding boundary like 124.5 would
    // flap it forever) held for MCLK_RETUNE_MS before moving it; a >3.0 BPM
    // jump is an unmistakable deliberate move and retunes now, dropping the
    // window so the next estimates measure only the new tempo. Runs before the
    // dropout check so a consumed START re-tunes from the retained estimate
    // even before the master's clock stream resumes.
    if (s_last_nominal_bpm == 0) {
        s_last_nominal_bpm = (int)lround(s_tempo_est);
        ztPlayer->chase_external_tempo(s_last_nominal_bpm);
    } else if (fabs(s_tempo_est - (double)s_last_nominal_bpm) > 3.0) {
        s_last_nominal_bpm = (int)lround(s_tempo_est);
        ztPlayer->chase_external_tempo(s_last_nominal_bpm);
        mclk_reset_estimate(/*keep_tempo=*/true);
    } else if (fabs(s_tempo_est - (double)s_last_nominal_bpm) > 0.6) {
        if (s_retune_since == 0) {
            s_retune_since = now;
        } else if (now - s_retune_since >= MCLK_RETUNE_MS) {
            s_last_nominal_bpm = (int)lround(s_tempo_est);
            ztPlayer->chase_external_tempo(s_last_nominal_bpm);
            s_retune_since = 0;
        }
    } else {
        s_retune_since = 0;
    }
    if (!clocks_recent) {
        // Dropout: the position reference is stale. Stop steering and invalidate
        // the anchor so the first chase after clocks resume re-anchors (warm
        // re-lock); the nominal set above keeps the engine free-running at the
        // master's last tempo.
        ztPlayer->chase_skew_us = 0;
        s_chase_valid = false;
        s_offset_ms   = 0.0;
        return;
    }
    if (!s_chase_valid) {
        // Started outside a consumed START (e.g. local F6 while slaved):
        // anchor at the current position so the lock is drift-only.
        s_anchor_clocks = clocks - ztPlayer->played_subticks / 4;
        s_chase_valid   = true;
        s_offset_ms     = 0.0;
        return;
    }
    // Expected position: 4 subticks per clock, plus intra-clock
    // interpolation from wall time so the reference doesn't advance in
    // 4-subtick stairs (~20 ms at 120 BPM) that would kick the controller
    // out of its steady-state deadband.
    double clock_period_ms = 60000.0 / (24.0 * s_tempo_est);
    double frac = (double)(now - last_clock) / clock_period_ms * 4.0;
    if (frac < 0.0) frac = 0.0;
    if (frac > 3.99) frac = 3.99;
    // sync_offset_ms: play early so notes monitored through the master
    // DAW's audio chain sound on the grid -- live-dialable, same as Link.
    double off_subticks = (double)zt_config_globals.sync_offset_ms
                          * s_tempo_est / 60000.0 * 96.0;
    double expected = (double)(clocks - s_anchor_clocks) * 4.0
                      + frac + off_subticks;
    double exact_us   = 60000000.0 / (96.0 * s_tempo_est);
    double nominal_us = (double)(ztPlayer->subtick_len_ms + ztPlayer->subtick_len_mms);
    // Position error (the quantity phase_chase_step feeds back on): engine
    // behind the master grid = positive.
    double err_ms = (expected - (double)ztPlayer->played_subticks) * exact_us / 1000.0;
    if (err_ms > MCLK_SNAP_MS || err_ms < -MCLK_SNAP_MS) {
        // A jump this large isn't drift -- it's a discontinuity (a main-loop
        // stall that skipped frames, or the engine resuming behind). Slewing it
        // back at the controller's bounded authority would take ~1.4 s and
        // audibly rush; instead snap the anchor to the engine's current
        // position and let the fine lock re-establish over the next frames.
        s_anchor_clocks = clocks - ztPlayer->played_subticks / 4;
        ztPlayer->chase_skew_us = 0;
        s_offset_ms = 0.0;
        return;
    }
    ztPlayer->chase_skew_us = phase_chase_step(expected, ztPlayer->played_subticks,
                                               exact_us, nominal_us);
    s_offset_ms = err_ms;
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
        mclk_reset_estimate(/*keep_tempo=*/false);
        s_state     = ZT_SYNC_OFF;
        s_offset_ms = 0.0;
        return;
    }
    unsigned now = mclk_now_ms();
    // One consistent snapshot of the clock counters for this whole frame, so
    // the anchor (consume), the tempo sample (estimate) and the position
    // reference (chase) can't disagree by a clock that lands mid-pump. Read
    // the COUNT before the TIMESTAMP: if a 0xF8 arrives between the two loads
    // we then see (old count, fresh timestamp) -> the interpolation fraction
    // collapses toward 0, which is consistent. The opposite order would briefly
    // add a whole clock (new count) on top of a near-full fraction (~20 ms at
    // 120 BPM) and kick the controller out of its deadband.
    int      clocks      = g_mclk_clocks;
    unsigned last_clock  = g_mclk_last_clock_ms;
    bool clocks_recent   = (last_clock != 0) && ((now - last_clock) <= MCLK_DROPOUT_MS);
    mclk_consume_transport(clocks);
    mclk_update_estimate(now, clocks, last_clock, clocks_recent);
    mclk_chase(now, clocks, last_clock, clocks_recent);

    // Derive the user-facing lock state (F11 meter + Lua) from the snapshot.
    if (!clocks_recent) {
        s_state = (last_clock == 0) ? ZT_SYNC_WAITING : ZT_SYNC_DROPOUT;
    } else if (!zt_config_globals.midi_in_sync_chase_tempo) {
        s_state = ZT_SYNC_TRANSPORT;
    } else if (s_tempo_est <= 0.0 || !ztPlayer->playing) {
        s_state = ZT_SYNC_WAITING;
    } else {
        double err_us = s_offset_ms * 1000.0;
        s_state = (err_us <= PHASE_CHASE_DEADBAND_US && err_us >= -PHASE_CHASE_DEADBAND_US)
                  ? ZT_SYNC_LOCKED : ZT_SYNC_CHASING;
    }
}

void zt_midi_clock_get_status(zt_sync_status *out) {
    if (!out) return;
    out->state      = s_state;
    out->master_bpm = s_tempo_est;
    out->offset_ms  = s_offset_ms;
}

const char *zt_sync_state_name(int state) {
    switch (state) {
        case ZT_SYNC_WAITING:   return "waiting";
        case ZT_SYNC_DROPOUT:   return "dropout";
        case ZT_SYNC_TRANSPORT: return "transport";
        case ZT_SYNC_CHASING:   return "chasing";
        case ZT_SYNC_LOCKED:    return "locked";
        case ZT_SYNC_OFF:
        default:                return "off";
    }
}
