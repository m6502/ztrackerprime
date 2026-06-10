// Unit tests for src/ableton_link.cpp.
//
// Compiled WITHOUT ZT_HAS_ABLETON_LINK and WITH ZT_TEST_NO_SDL, so the stub
// backend compiles against the fake app environment in zt_link_stub.h. Two
// layers are exercised:
//
//   1. The shared contract logic (tempo clamp + rounded dedupe, transport
//      edge detect, prime, quantum clamp) -- one copy serves both the real
//      and stub backends, so a regression here breaks every build.
//   2. The app integration (startup, defer_play, the pump body): quantized
//      arm/fire via the scriptable stub phase, armed-start cancellation on
//      stop_count / song-generation changes, playing-edge sharing, peer
//      transport follow, and bidirectional tempo against a recording fake
//      player.
//
// Because this file #includes ableton_link.cpp, the file-statics
// (g_play_armed, g_stub_phase, g_cached_bpm, ...) are directly readable and
// pokable -- no test-only API needed. link_pump_body() keeps its baselines in
// function-statics that persist across calls, so the app-level tests run in
// a deliberate order and absorb pending edges with an extra pump where noted.
//
// The real Link-backed behaviour (peer discovery, beat clock, transport over
// the network) needs a live LAN session and is covered by the headless
// harness instead.

#include "ableton_link.h"
#include "ableton_link.cpp"

#include <stdio.h>

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

// --- shared contract logic ---------------------------------------------------

static void test_available_is_false_in_stub() {
    CHECK(zt_ableton_link_available() == 0);
}

static void test_init_shutdown_idempotent() {
    link_init();
    link_init();
    be_shutdown();
    be_shutdown();   // shutdown without a live session
    link_init();
    CHECK(zt_ableton_link_available() == 0);
}

static void test_tempo_cache_roundtrip_and_clamp() {
    link_init();
    link_set_tempo(140.0);
    CHECK(zt_ableton_link_get_tempo() == 140.0);
    link_set_tempo(90.0);
    CHECK(zt_ableton_link_get_tempo() == 90.0);
    // Clamp: below floor (20) and above ceiling (999).
    link_set_tempo(5.0);
    CHECK(zt_ableton_link_get_tempo() == 20.0);
    link_set_tempo(5000.0);
    CHECK(zt_ableton_link_get_tempo() == 999.0);
}

static void test_pump_tempo_change_dedupe() {
    link_init();              // resets the "last pumped" sentinel
    be_enable(true);          // pump_tempo reports only while enabled
    link_set_tempo(120.0);
    int bpm = -1;
    // First pump after init reports the current value (sentinel was -1).
    CHECK(link_pump_tempo(&bpm) == true);
    CHECK(bpm == 120);
    // No change -> no report.
    bpm = -1;
    CHECK(link_pump_tempo(&bpm) == false);
    CHECK(bpm == -1);               // left untouched
    // Change -> report once.
    link_set_tempo(123.4);          // rounds to 123
    CHECK(link_pump_tempo(&bpm) == true);
    CHECK(bpm == 123);
    CHECK(link_pump_tempo(&bpm) == false);
    // Disabled -> never reports.
    be_enable(false);
    link_set_tempo(99.0);
    CHECK(link_pump_tempo(&bpm) == false);
}

static void test_transport_cache_and_edge_detect() {
    link_init();              // resets the transport edge sentinel
    be_set_playing(false);
    CHECK(be_playing() == false);

    be_set_playing(true);
    CHECK(be_playing() == true);

    int want = -1;
    // First poll after the start: edge from -1 sentinel to 1.
    CHECK(link_poll_transport(&want) == true);
    CHECK(want == 1);
    // No change -> no edge.
    want = -1;
    CHECK(link_poll_transport(&want) == false);
    CHECK(want == -1);
    // Stop -> edge to 0, reported once.
    be_set_playing(false);
    CHECK(link_poll_transport(&want) == true);
    CHECK(want == 0);
    CHECK(link_poll_transport(&want) == false);
}

static void test_set_quantum_clamps() {
    link_set_quantum(0.0);
    CHECK(g_quantum == 1.0);
    link_set_quantum(100.0);
    CHECK(g_quantum == 16.0);
    link_set_quantum(4.0);
    CHECK(g_quantum == 4.0);
}

static void test_prime_transport_suppresses_spurious_edge() {
    // Enabling start/stop sync must not fire a transport edge just because
    // the sentinel was stale. Priming to the current state suppresses that
    // first spurious edge; genuine later changes still fire.
    link_init();              // g_last_transport sentinel = -1
    be_set_playing(true);
    link_prime_transport();   // baseline to current (playing)
    int want = -1;
    CHECK(link_poll_transport(&want) == false);  // no spurious edge
    CHECK(want == -1);                            // left untouched
    // A real change after priming still edges.
    be_set_playing(false);
    CHECK(link_poll_transport(&want) == true);
    CHECK(want == 0);
}

// --- app integration ---------------------------------------------------------
// These drive link_pump_body() (the pump minus its availability gate, which
// is compile-time 0 in this TU and asserted separately below). Pump baselines
// live in function-statics, so order matters and edges are absorbed with an
// extra pump where noted.

static void test_startup_snaps_conf_flag_in_stub() {
    // The production "stub can't join" guarantee: available()==0 makes
    // startup() clear the conf flag, so the pump never enables.
    zt_config_globals.ableton_link_enable = 1;
    song->bpm = 138;
    zt_ableton_link_startup();
    CHECK(zt_config_globals.ableton_link_enable == 0);
    CHECK(zt_ableton_link_get_tempo() == 138.0);   // seeded from the song
    CHECK(be_enabled() == false);
}

static void test_public_pump_is_inert_in_stub_build() {
    // Even a hand-edited conf flag must not reach be_enable through the
    // PUBLIC pump in a stub build -- the availability gate covers it.
    zt_config_globals.ableton_link_enable = 1;
    zt_ableton_link_pump();
    CHECK(be_enabled() == false);
    zt_config_globals.ableton_link_enable = 0;
}

static void test_defer_play_disabled_passes_through() {
    be_enable(false);
    CHECK(zt_ableton_link_defer_play(3, 9, 2) == 0);
    CHECK(g_play_armed == false);
    CHECK(ztPlayer->play_immediately_calls == 0);   // play() itself would start
}

static void test_pump_enable_transition_seeds_song_tempo() {
    song->bpm    = 138;
    g_cached_bpm = 87.0;   // stale wrapper cache from an earlier session
    zt_config_globals.ableton_link_enable          = 1;
    zt_config_globals.ableton_link_start_stop_sync = 1;
    zt_config_globals.ableton_link_quantum         = 8;
    link_pump_body();
    CHECK(be_enabled() == true);
    CHECK(zt_ableton_link_get_tempo() == 138.0);   // seeded, not the stale 87
    CHECK(g_last_synced_bpm == 138);
    CHECK(g_quantum == 8.0);                       // conf quantum reconciled
    // Stable: a second pump must not move anything.
    link_pump_body();
    CHECK(song->bpm == 138);
    CHECK(ztPlayer->set_speed_calls == 0);
}

static void test_defer_arm_and_quantized_fire() {
    g_stub_phase = 1.0;
    status_change = 0;
    CHECK(zt_ableton_link_defer_play(5, 7, 2) == 1);
    CHECK(g_play_armed == true);
    CHECK(statusmsg != nullptr);          // "waiting for downbeat"
    CHECK(status_change == 1);            // status repaint is gated on this
    CHECK(be_playing() == true);          // peers told at arm time
    // Phase advancing within the bar: no fire.
    g_stub_phase = 2.5;
    link_pump_body();
    CHECK(g_play_armed == true);
    CHECK(ztPlayer->play_immediately_calls == 0);
    // Phase wraps past the downbeat: fire with the armed row/pat/pm.
    g_stub_phase = 0.3;
    link_pump_body();
    CHECK(g_play_armed == false);
    CHECK(ztPlayer->play_immediately_calls == 1);
    CHECK(ztPlayer->last_row == 5);
    CHECK(ztPlayer->last_pat == 7);
    CHECK(ztPlayer->last_pm  == 2);
    CHECK(ztPlayer->playing  == 1);
    CHECK(g_chase_valid == true);             // fire anchored the phase chase
    CHECK(g_chase_anchor == g_stub_beats);
    link_pump_body();   // absorb the started edge (already session-playing)
}

static void test_offset_fires_early() {
    // Link Offset: with 171 ms configured at 120 BPM, the fire must happen
    // when the remaining time to the downbeat drops inside the offset --
    // not at the phase wrap.
    ztPlayer->playing = 0;
    zt_config_globals.ableton_link_quantum   = 4;     // earlier test set 8
    zt_config_globals.ableton_link_offset_ms = 171;
    link_pump_body();                       // absorb stop edge, apply quantum
    g_cached_bpm = 120.0;                   // known tempo for the ms math
    g_stub_phase = 1.0;
    int plays = ztPlayer->play_immediately_calls;
    CHECK(zt_ableton_link_defer_play(0, 0, 1) == 1);
    g_stub_phase = 3.0;                     // 500 ms remain: outside offset
    link_pump_body();
    CHECK(g_play_armed == true);
    CHECK(ztPlayer->play_immediately_calls == plays);
    g_stub_phase = 3.7;                     // 150 ms remain: inside offset
    link_pump_body();
    CHECK(g_play_armed == false);
    CHECK(ztPlayer->play_immediately_calls == plays + 1);
    zt_config_globals.ableton_link_offset_ms = 0;
    link_pump_body();                       // absorb the start edge
}

static void test_stop_count_cancels_arm_and_pushes_stop() {
    // F8 while armed: the player just counts the stop; the pump must both
    // cancel the pending start and tell peers to stop.
    ztPlayer->playing = 0;
    link_pump_body();                       // absorb the stopped edge
    g_stub_phase = 1.0;
    int plays = ztPlayer->play_immediately_calls;
    CHECK(zt_ableton_link_defer_play(0, 0, 1) == 1);
    CHECK(g_play_armed == true);
    CHECK(be_playing() == true);
    ztPlayer->stop();                       // F8 -- plain player call
    link_pump_body();
    CHECK(g_play_armed == false);           // start cancelled
    CHECK(be_playing() == false);           // peers stopped
    CHECK(ztPlayer->play_immediately_calls == plays);   // never fired
    link_pump_body();                       // absorb our own stop edge
}

static void test_song_generation_cancels_arm() {
    g_stub_phase = 1.0;
    int plays = ztPlayer->play_immediately_calls;
    CHECK(zt_ableton_link_defer_play(0, 0, 1) == 1);
    CHECK(g_play_armed == true);
    zt_song_generation++;                   // File > New / song load
    link_pump_body();
    CHECK(g_play_armed == false);
    CHECK(ztPlayer->play_immediately_calls == plays);   // stale start never fired
    be_set_playing(false);                  // tidy session transport
    link_prime_transport();
    link_pump_body();
}

static void test_playing_edges_push_to_session() {
    // A play that did not go through defer (and a stop) must still be shared.
    CHECK(be_playing() == false);
    ztPlayer->playing = 1;                  // direct start
    link_pump_body();
    CHECK(be_playing() == true);            // started edge pushed
    ztPlayer->playing = 0;                  // direct stop (e.g. song end)
    link_pump_body();
    CHECK(be_playing() == false);           // stopped edge pushed
    link_pump_body();                       // absorb the poll edges
}

static void test_tempo_pull_from_session() {
    int speed_calls = ztPlayer->set_speed_calls;
    g_cached_bpm = 87.4;                    // a peer committed 87.4
    link_pump_body();
    CHECK(song->bpm == 87);                 // rounded follow
    CHECK(ztPlayer->set_speed_calls == speed_calls + 1);
    g_cached_bpm = 600.0;                   // beyond zt's ceiling
    link_pump_body();
    CHECK(song->bpm == 500);                // clamped to chase range
    CHECK(ztPlayer->set_speed_calls == speed_calls + 2);
    link_pump_body();                       // stable: no re-apply
    CHECK(ztPlayer->set_speed_calls == speed_calls + 2);
}

static void test_tempo_push_to_session() {
    int speed_calls = ztPlayer->set_speed_calls;
    song->bpm = 150;                        // user edit on F11
    link_pump_body();
    CHECK(zt_ableton_link_get_tempo() == 150.0);   // proposed to peers
    CHECK(g_last_synced_bpm == 150);
    link_pump_body();                       // no ping-pong
    CHECK(song->bpm == 150);
    CHECK(ztPlayer->set_speed_calls == speed_calls);
}

static void test_peer_transport_follow() {
    ztPlayer->playing = 0;
    be_set_playing(false);
    link_prime_transport();
    link_pump_body();                       // settle
    song->orderlist[0] = 0;                 // valid first order
    cur_edit_pattern   = 3;
    g_stub_playing = true;                  // peer pressed play
    link_pump_body();
    CHECK(g_play_armed == true);            // follows as a quantized launch
    CHECK(g_play_pat == 3);
    CHECK(g_play_pm  == 1);
    g_stub_phase = 1.0; link_pump_body();   // wait inside the bar
    g_stub_phase = 0.2; link_pump_body();   // downbeat
    CHECK(ztPlayer->playing == 1);
    int stops = ztPlayer->stop_calls;
    g_stub_playing = false;                 // peer pressed stop
    link_pump_body();
    CHECK(g_play_armed == false);
    CHECK(ztPlayer->stop_calls == stops + 1);
    CHECK(ztPlayer->playing == 0);
    link_pump_body();                       // absorb our stop's edges
}

// --- phase chase ---------------------------------------------------------
// Driven through link_chase_phase() directly with hand-set engine state.
// Stub nominal: 120 BPM -> exact subtick 5208.33 us, engine avg 5208 us,
// so the rate term alone rounds to 0 and the feedback bound is ~15.6 us.

static void chase_reset_engine(double session_bpm) {
    be_enable(true);
    g_cached_bpm = session_bpm;
    zt_config_globals.ableton_link_offset_ms = 0;
    ztPlayer->playing         = 1;
    ztPlayer->bpm             = 120;
    ztPlayer->subtick_len_ms  = 5000;
    ztPlayer->subtick_len_mms = 208;
    ztPlayer->played_subticks = 0;
    ztPlayer->chase_skew_us   = 0;
    g_stub_beats  = 100.0;
    g_chase_valid = false;
}

static void test_chase_anchors_then_locks_at_zero_error() {
    chase_reset_engine(120.0);
    link_chase_phase();                       // first call only anchors
    CHECK(g_chase_valid == true);
    CHECK(ztPlayer->chase_skew_us == 0);
    g_stub_beats = 101.0;                     // one beat of session time...
    ztPlayer->played_subticks = 96;           // ...and the engine kept up
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us == 0);      // rate term 0.33us rounds away
}

static void test_chase_feedback_sign_and_clamp() {
    chase_reset_engine(120.0);
    link_chase_phase();                       // anchor at beats=100
    g_stub_beats = 101.0;
    // Small error (inside 10 ms): steady-state tier, 0.3% bound (~15.6 us).
    ztPlayer->played_subticks = 95;           // 1 subtick (~5 ms) BEHIND
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us < 0);       // shorter subticks to catch up
    CHECK(ztPlayer->chase_skew_us >= -16);
    // Large error (a deliberate move): 12% tier (~625 us).
    ztPlayer->played_subticks = 48;           // half a beat behind
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us < -16);     // beyond the steady-state tier
    CHECK(ztPlayer->chase_skew_us >= -626);   // clamped at 12%
    ztPlayer->played_subticks = 144;          // half a beat AHEAD
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us > 16);      // longer subticks to fall back
    CHECK(ztPlayer->chase_skew_us <= 626);
}

static void test_chase_live_offset_shifts_setpoint() {
    chase_reset_engine(120.0);
    link_chase_phase();                       // anchor at beats=100, offset 0
    g_stub_beats = 101.0;
    ztPlayer->played_subticks = 96;           // locked, zero error
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us == 0);
    // User drags the F11 slider to 100 ms while playing: the setpoint moves
    // 100 ms earlier, the engine is suddenly "behind" and must speed up at
    // the deliberate-move tier.
    zt_config_globals.ableton_link_offset_ms = 100;
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us < -16);
    CHECK(ztPlayer->chase_skew_us >= -626);
    // Catching up restores lock: advance the engine by the 100 ms the new
    // setpoint demands (100 ms at 120 BPM = 0.2 beats = 19.2 subticks).
    ztPlayer->played_subticks = 96 + 19;
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us >= -16);    // back inside steady-state
    zt_config_globals.ableton_link_offset_ms = 0;
}

static void test_chase_fractional_tempo_rate_term() {
    chase_reset_engine(119.6);                // session fractional, zt shows 120
    link_chase_phase();                       // anchor
    g_stub_beats += 1.0;                      // engine tracked the session
    ztPlayer->played_subticks += 96;          // exactly: zero position error
    link_chase_phase();
    // exact subtick at 119.6 = 5225.75us vs engine avg 5208us -> +17.75
    CHECK(ztPlayer->chase_skew_us >= 17 && ztPlayer->chase_skew_us <= 19);
}

static void test_chase_pauses_on_rate_divergence() {
    chase_reset_engine(120.0);
    link_chase_phase();
    CHECK(g_chase_valid == true);
    ztPlayer->bpm = 90;                       // Fxx took the song elsewhere
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us == 0);
    CHECK(g_chase_valid == false);
    ztPlayer->bpm = 120;                      // rates agree again
    link_chase_phase();                       // re-anchors from current pos
    CHECK(g_chase_valid == true);
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us == 0);      // no position jump
}

static void test_chase_zero_skew_when_stopped() {
    chase_reset_engine(120.0);
    ztPlayer->chase_skew_us = 77;
    ztPlayer->playing = 0;
    link_chase_phase();
    CHECK(ztPlayer->chase_skew_us == 0);
}

static void test_peer_start_respects_empty_orderlist() {
    song->orderlist[0] = 0x100;             // empty song: nothing to launch
    g_stub_playing = true;
    link_pump_body();
    CHECK(g_play_armed == false);
    g_stub_playing = false;
    link_pump_body();
}

int main(void) {
    test_available_is_false_in_stub();
    test_init_shutdown_idempotent();
    test_tempo_cache_roundtrip_and_clamp();
    test_pump_tempo_change_dedupe();
    test_transport_cache_and_edge_detect();
    test_set_quantum_clamps();
    test_prime_transport_suppresses_spurious_edge();
    test_startup_snaps_conf_flag_in_stub();
    test_public_pump_is_inert_in_stub_build();
    test_defer_play_disabled_passes_through();
    test_pump_enable_transition_seeds_song_tempo();
    test_defer_arm_and_quantized_fire();
    test_offset_fires_early();
    test_stop_count_cancels_arm_and_pushes_stop();
    test_song_generation_cancels_arm();
    test_playing_edges_push_to_session();
    test_tempo_pull_from_session();
    test_tempo_push_to_session();
    test_peer_transport_follow();
    test_peer_start_respects_empty_orderlist();
    test_chase_anchors_then_locks_at_zero_error();
    test_chase_feedback_sign_and_clamp();
    test_chase_live_offset_shifts_setpoint();
    test_chase_fractional_tempo_rate_term();
    test_chase_pauses_on_rate_divergence();
    test_chase_zero_skew_when_stopped();
    printf("ableton_link (stub): %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
