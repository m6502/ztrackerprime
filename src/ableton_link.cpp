/*************************************************************************
 * ableton_link.cpp
 *
 * Implementation for Ableton Link support
 *************************************************************************/

#include "ableton_link.h"
#include "phase_chase.h"

#include <cmath>   // lround

#ifdef ZT_HAS_ABLETON_LINK
#include <memory>
// asio (vendored under Link) is noisy under -Wall -Wextra -Wpedantic. We don't
// own it; silence locally so the rest of the tree's build output stays clean.
// This block must precede the zt.h include below: on Windows asio needs
// winsock2.h before anything drags in windows.h.
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
#include <ableton/Link.hpp>
#if defined(__clang__) || defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#endif // ZT_HAS_ABLETON_LINK

#ifdef ZT_TEST_NO_SDL
#include "zt_link_stub.h"   // tests/: fake player, song, conf, globals
#else
#include "zt.h"             // ztPlayer, song, statusmsg, need_refresh,
                            // zt_config_globals, cur_edit_pattern
#endif

// ---------------------------------------------------------------------------
// State shared by both backends. g_cached_bpm survives shutdown — it is the
// caller's tempo source of truth (seeded into a fresh Link on init), not
// Link's. The "last" values drive the change-dedupe / edge-detect below.

static double g_cached_bpm      = 120.0;
static double g_quantum         = 4.0;
static int    g_last_pumped_bpm = -1;   // last int BPM reported by link_pump_tempo
static int    g_last_transport  = -1;   // last transport state seen by link_poll_transport

static double clamp_bpm(double bpm) {
    if (bpm < 20.0)  return 20.0;
    if (bpm > 999.0) return 999.0;
    return bpm;
}

// ---------------------------------------------------------------------------
// Backend primitives: the only real-vs-stub divergence.

#ifdef ZT_HAS_ABLETON_LINK

static std::unique_ptr<ableton::Link> g_link;

static void   be_init(void)            { if (!g_link) g_link.reset(new ableton::Link(clamp_bpm(g_cached_bpm))); }
static void   be_shutdown(void)        { if (g_link) { g_link->enable(false); g_link.reset(); } }
static void   be_enable(bool e)        { if (g_link) g_link->enable(e); }
static bool   be_enabled(void)         { return g_link && g_link->isEnabled(); }
static size_t be_peers(void)           { return g_link ? g_link->numPeers() : 0; }

static double be_tempo(void) {
    if (g_link && g_link->isEnabled())
        return g_link->captureAppSessionState().tempo();
    return g_cached_bpm;
}

static void be_set_tempo(double bpm) {
    if (g_link) {
        auto state = g_link->captureAppSessionState();
        state.setTempo(bpm, g_link->clock().micros());
        g_link->commitAppSessionState(state);
    }
}

// Beat phase within the quantum, in [0, quantum): position inside the current
// bar, 0 = downbeat. -1 when not enabled (no shared grid to align to).
static double be_phase(void) {
    if (!g_link || !g_link->isEnabled()) return -1.0;
    auto state = g_link->captureAppSessionState();
    return state.phaseAtTime(g_link->clock().micros(), g_quantum);
}

// Absolute session beat timeline (monotonic while the session runs). The
// phase chase anchors against this. -1 when not enabled.
static double be_beats(void) {
    if (!g_link || !g_link->isEnabled()) return -1.0;
    auto state = g_link->captureAppSessionState();
    return state.beatAtTime(g_link->clock().micros(), g_quantum);
}

static void be_sss_enable(bool e)      { if (g_link) g_link->enableStartStopSync(e); }
static bool be_sss_enabled(void)       { return g_link && g_link->isStartStopSyncEnabled(); }

static void be_set_playing(bool play) {
    if (!g_link) return;
    auto state = g_link->captureAppSessionState();
    auto now   = g_link->clock().micros();
    if (play) {
        // Pin beat 0 to the next quantum boundary so peers launch bar-aligned.
        state.setIsPlayingAndRequestBeatAtTime(true, now, 0.0, g_quantum);
    } else {
        state.setIsPlaying(false, now);
    }
    g_link->commitAppSessionState(state);
}

static bool be_playing(void) {
    return g_link && g_link->captureAppSessionState().isPlaying();
}

#else  // ---- STUB backend (no ZT_HAS_ABLETON_LINK) -------------------------

// Cached flags stand in for the session. In production stub builds none of
// the enabled paths are ever reached — zt_ableton_link_available() returns 0
// so startup() snaps the conf flag off and the pump never enables — but the
// flags are real enough that the unit test can drive the full pump logic.
static bool   g_stub_enabled = false;
static bool   g_stub_sss     = false;
static bool   g_stub_playing = false;
static double g_stub_phase   = -1.0;   // pokable by the unit test

static double g_stub_beats   = 0.0;    // pokable by the unit test

static void   be_init(void)            {}
static void   be_shutdown(void)        { g_stub_enabled = false; }
static void   be_enable(bool e)        { g_stub_enabled = e; }
static bool   be_enabled(void)         { return g_stub_enabled; }
static size_t be_peers(void)           { return 0; }
static double be_tempo(void)           { return g_cached_bpm; }
static void   be_set_tempo(double)     {}
static double be_phase(void)           { return g_stub_phase; }
static double be_beats(void)           { return g_stub_beats; }
static void   be_sss_enable(bool e)    { g_stub_sss = e; }
static bool   be_sss_enabled(void)     { return g_stub_sss; }
static void   be_set_playing(bool p)   { g_stub_playing = p; }
static bool   be_playing(void)         { return g_stub_playing; }

#endif // ZT_HAS_ABLETON_LINK

// ---------------------------------------------------------------------------
// Shared contract logic — one copy for both backends (the unit test's
// subject). The dedupe/edge "last" sentinels keep callers from re-acting on
// unchanged session state every frame.

static void link_init(void) {
    be_init();
    g_last_pumped_bpm = -1;
    g_last_transport  = -1;
}

// zt -> session: propose a tempo (every peer adopts it). Caches the clamped
// value so it also seeds the next link_init().
static void link_set_tempo(double bpm) {
    bpm = clamp_bpm(bpm);
    g_cached_bpm = bpm;
    be_set_tempo(bpm);
}

// session -> zt: reads the session tempo, rounds to int BPM, and returns true
// (writing *bpm_out) ONLY when the rounded value changed since the last
// successful pump — feedable straight to song->bpm without oscillation.
static bool link_pump_tempo(int *bpm_out) {
    if (!be_enabled()) return false;
    double tempo = be_tempo();
    g_cached_bpm = tempo;
    int rounded = (int)lround(tempo);
    if (rounded != g_last_pumped_bpm) {
        g_last_pumped_bpm = rounded;
        if (bpm_out) *bpm_out = rounded;
        return true;
    }
    return false;
}

static void link_set_quantum(double quantum) {
    if (quantum < 1.0)  quantum = 1.0;
    if (quantum > 16.0) quantum = 16.0;
    g_quantum = quantum;
}

// Edge-detected peer transport follow: true ONLY when the shared transport
// state changed since the last poll (writing *want_play = 1/0), so a peer
// hitting play/stop is mirrored once instead of every frame.
static bool link_poll_transport(int *want_play) {
    int cur = be_playing() ? 1 : 0;
    if (cur != g_last_transport) {
        g_last_transport = cur;
        if (want_play) *want_play = cur;
        return true;
    }
    return false;
}

// Re-baseline the edge detector to the CURRENT session state so the next poll
// reports no edge — call when (re)joining or when sss is freshly enabled,
// otherwise a stale sentinel replays an old peer start/stop as a fresh edge.
static void link_prime_transport(void) {
    g_last_transport = be_playing() ? 1 : 0;
}

// ---------------------------------------------------------------------------
// App integration.

// Quantized-start state: player::play() arms a downbeat-aligned start that
// the pump fires (via play_immediately) when the Link phase wraps past the bar
// boundary.
static bool   g_play_armed = false;
static int    g_play_row = 0, g_play_pat = 0, g_play_pm = 0;
static double g_play_phase = 0.0;
// Last BPM reconciled with the session, so the pump tells a zt-side tempo
// change (push to peers) from a peer-side one (follow).
static int    g_last_synced_bpm = -1;
// Phase chase: the session beat at which the playing engine's subtick 0
// occurred. Anchored at the quantized fire (which bakes the Link Offset
// into the setpoint), re-derived whenever the chase resumes.
static double g_chase_anchor = 0.0;
static bool   g_chase_valid  = false;

static void link_notify_transport(bool playing) {
    if (zt_config_globals.ableton_link_enable &&
        zt_config_globals.ableton_link_start_stop_sync &&
        be_enabled())
        be_set_playing(playing);
}

static void link_arm_play(int row, int pattern, int pm) {
    g_play_row = row; g_play_pat = pattern; g_play_pm = pm;
    g_play_armed = true;
    double ph = be_phase();
    g_play_phase = (ph >= 0.0) ? ph : 0.0;
    statusmsg = "Link: waiting for downbeat...";
    status_change = 1;   // the status-line repaint is gated on this
    need_refresh++;
}

// Fire an armed start when the phase wraps past the downbeat (or immediately
// if Link was disabled while waiting). play_immediately, not play: play() would defer
// right back to us.
//
// sync_offset_ms fires that many ms BEFORE the downbeat: a peer
// monitoring zt through an audio chain (Live's Overall Latency) renders our
// notes that much late, and Link cannot discover a peer's local latency, so
// the user dials the rig's value once and zt's whole timeline runs early by
// exactly that amount -- the notes then SOUND on the grid.
static void link_fire_quantized(void) {
    if (!g_play_armed) return;
    if (!be_enabled()) {
        ztPlayer->play_immediately(g_play_row, g_play_pat, g_play_pm);
        g_play_armed = false; need_refresh++;
        return;
    }
    double ph = be_phase();
    if (ph < 0.0) return;
    bool fire = (ph < g_play_phase);   // wrapped past the downbeat
    int offset_ms = zt_config_globals.sync_offset_ms;
    if (!fire && offset_ms > 0) {
        double tempo = be_tempo();
        if (tempo > 0.0) {
            double remaining_ms = (g_quantum - ph) * 60000.0 / tempo;
            fire = (remaining_ms <= (double)offset_ms);
        }
    }
    if (fire) {
        ztPlayer->play_immediately(g_play_row, g_play_pat, g_play_pm);
        g_play_armed = false; need_refresh++;
        // Anchor the chase on the DOWNBEAT this start aimed at (we are
        // firing offset_ms before it), not on the fire moment itself: the
        // chase re-applies the CURRENT offset every frame, which makes the
        // F11 slider live -- drag it while playing and the loop slews the
        // engine to the new offset.
        double tempo = be_tempo();
        double off_beats = (tempo > 0.0)
            ? (double)zt_config_globals.sync_offset_ms * tempo / 60000.0
            : 0.0;
        g_chase_anchor = be_beats() + off_beats;
        g_chase_valid  = true;
    }
    g_play_phase = ph;
}

// Closed-loop phase lock of the playing engine to Link's beat timeline.
// Rate-following alone (what the MIDI-clock chase does) still drifts:
// fractional session tempos round to int BPM, set_speed() truncates the
// subtick to whole us, and a peer on another machine runs its own crystal.
// Here any residual rate error integrates into POSITION error against
// be_beats(), and the loop cancels it by slewing the subtick length:
//   skew = (exact subtick for the fractional session tempo - the engine's
//           nominal average) - bounded P feedback on the position error.
// The feedback bound (0.3% of a subtick) is far below audibility; the total
// is clamped to 2% so no state can audibly warp playback. While zt's rate
// deliberately diverges from the session (Fxx tempo effect, local edit not
// yet pushed) the chase pauses and re-anchors when the rates agree again --
// re-deriving the anchor from the current position, never jumping it.
static void link_chase_phase(void) {
    if (!ztPlayer) return;
    if (!be_enabled() || !ztPlayer->playing) {
        ztPlayer->chase_skew_us = 0;
        return;
    }
    double tempo = be_tempo();
    bool rates_agree = (tempo >= 20.0 &&
                        tempo - (double)ztPlayer->bpm < 1.0 &&
                        (double)ztPlayer->bpm - tempo < 1.0);
    if (!rates_agree) {
        ztPlayer->chase_skew_us = 0;
        g_chase_valid = false;
        return;
    }
    double beats = be_beats();
    if (beats < 0.0) return;
    // The offset enters the setpoint HERE, from the live conf value, so
    // dragging the F11 slider during playback shifts the target and the
    // loop slews onto it.
    double off_beats = (double)zt_config_globals.sync_offset_ms * tempo / 60000.0;
    int played = ztPlayer->played_subticks;   // one advisory read
    if (!g_chase_valid) {
        g_chase_anchor = beats - played / 96.0 + off_beats;
        g_chase_valid  = true;
        return;
    }
    double exact_us   = 60000000.0 / (96.0 * tempo);
    double expected   = (beats - g_chase_anchor + off_beats) * 96.0;   // subticks
    double nominal_us = (double)(ztPlayer->subtick_len_ms + ztPlayer->subtick_len_mms);
    ztPlayer->chase_skew_us =
        phase_chase_step(expected, played, exact_us, nominal_us);
}

// Does NOT join the session here -- the first zt_ableton_link_pump() does,
// via its enable-reconcile branch. startup() runs inside initSDL(), BEFORE
// the autoload/CLI song load; joining now would seed the session with the
// default song's tempo, and the first pump would then "follow" that stale
// seed and silently revert the loaded song's BPM. The first pump runs after
// the load, so the join always seeds the real song tempo.
void zt_ableton_link_startup(void) {
    link_init();
    link_set_tempo((double)song->bpm);
    if (!zt_ableton_link_available())
        zt_config_globals.ableton_link_enable = 0;   // stub build can't join
}

void zt_ableton_link_teardown(void) { be_shutdown(); }

int zt_ableton_link_defer_play(int row, int pattern, int pm) {
    if (!be_enabled())
        return 0;   // play() proceeds; the pump's start edge tells peers
    link_notify_transport(true);
    link_prime_transport();   // swallow our own start edge
    link_arm_play(row, pattern, pm);
    return 1;
}

int zt_ableton_link_available(void) {
#ifdef ZT_HAS_ABLETON_LINK
    return 1;
#else
    return 0;
#endif
}

size_t zt_ableton_link_num_peers(void) { return be_peers(); }
double zt_ableton_link_get_tempo(void) { return be_tempo(); }

// Everything behind the pump's availability gate. Separate from the public
// entry so the unit test (where available() is compile-time 0) can drive it.
static void link_pump_body(void) {
    // Observe the player and song instead of requiring every call site to
    // tell us about transport changes. player::stop() can run on the counter
    // thread (song end) and inside prepare_play's restart, so the player just
    // counts stops; module::init() bumps the song generation. All Link calls
    // and the armed-start bookkeeping stay here, on the main thread.
    static int prev_playing    = -1;
    static int last_stop_count = -1;
    static int last_song_gen   = -1;
    int  playing_now  = (ztPlayer && ztPlayer->playing) ? 1 : 0;
    int  stop_now     = ztPlayer ? ztPlayer->stop_count : 0;
    bool started      = (prev_playing == 0 && playing_now == 1);
    bool stopped      = (prev_playing == 1 && playing_now == 0);
    bool stop_intent  = (last_stop_count >= 0 && stop_now != last_stop_count);
    bool song_changed = (last_song_gen >= 0 && zt_song_generation != last_song_gen);
    prev_playing    = playing_now;
    last_stop_count = stop_now;
    last_song_gen   = zt_song_generation;
    // A stop from any path, a play that started outside the defer, or a song
    // rebuild (File > New / load) all invalidate a pending quantized start.
    if (stop_intent || song_changed || started)
        g_play_armed = false;
    // A start that didn't come from the quantized fire has no known beat
    // anchor -- the chase re-derives one from the current position.
    if (started)
        g_chase_valid = false;

    // Conf reconcile: quantum, session enable, start/stop sync.
    static int last_quantum = -1;
    if (zt_config_globals.ableton_link_quantum != last_quantum) {
        last_quantum = zt_config_globals.ableton_link_quantum;
        link_set_quantum((double)last_quantum);
    }
    bool want_enabled = zt_config_globals.ableton_link_enable != 0;
    if (want_enabled != be_enabled()) {
        be_enable(want_enabled);
        if (want_enabled) {
            // Re-seed our tempo proposal: a solo session keeps song->bpm
            // (instead of whatever stale value we last cached), and when
            // joining an established session Link's merge still adopts the
            // session's tempo -- the pull below then follows it.
            link_set_tempo((double)song->bpm);
            g_last_synced_bpm = song->bpm;
            // Re-baseline the transport edge detector so the act of joining
            // can't replay a stale peer start/stop as a fresh edge.
            link_prime_transport();
        }
    }
    bool want_sss = zt_config_globals.ableton_link_start_stop_sync != 0;
    if (want_sss != be_sss_enabled()) {
        be_sss_enable(want_sss);
        if (want_sss) link_prime_transport();
    }
    // Bidirectional tempo. PULL: a peer's change follows into song->bpm (even
    // stopped) -- not a file edit. PUSH: zt's own change proposes to peers.
    // g_last_synced_bpm tells them apart; link_pump_tempo's rounded dedupe
    // prevents ping-pong.
    if (be_enabled() && ztPlayer) {
        int link_bpm = 0;
        if (link_pump_tempo(&link_bpm) && link_bpm) {
            if (link_bpm > 500) link_bpm = 500;
            if (link_bpm < 20)  link_bpm = 20;
            if (link_bpm != song->bpm) {
                song->bpm = link_bpm; ztPlayer->set_speed(); need_refresh++;
            }
            g_last_synced_bpm = song->bpm;
        } else if (song->bpm != g_last_synced_bpm) {
            link_set_tempo((double)song->bpm);
            g_last_synced_bpm = song->bpm;
        }
    }
    link_fire_quantized();
    link_chase_phase();
    // Follow a peer's transport: a peer start arms a quantized launch (same as
    // local), a peer stop stops us. link_prime_transport swallows our own
    // edges.
    if (be_enabled() &&
        zt_config_globals.ableton_link_start_stop_sync && ztPlayer) {
        int want_play = -1;
        if (link_poll_transport(&want_play)) {
            if (want_play == 1) {
                if (!ztPlayer->playing && !g_play_armed &&
                    song->orderlist[0] != 0x100)
                    link_arm_play(0, cur_edit_pattern, 1);
            } else if (want_play == 0) {
                g_play_armed = false;
                if (ztPlayer->playing) { ztPlayer->stop(); need_refresh++; }
            }
        }
    }
    // Share transport changes the defer didn't initiate (song-end stop, F8
    // pressed while armed, any direct ztPlayer->play/stop). Defer-initiated
    // changes already notified at arm/follow time, so gate on the session's
    // current state to avoid double commits.
    if (be_enabled() &&
        zt_config_globals.ableton_link_start_stop_sync) {
        if (started && !be_playing())
            be_set_playing(true);
        if ((stopped || (stop_intent && !playing_now)) &&
            be_playing())
            be_set_playing(false);
    }

    // Re-sample after our own actions (the quantized fire, the peer-follow
    // stop) so they never echo back as observed edges on the next pump --
    // only transitions that happen OUTSIDE the pump count. Without this, our
    // own fire's start edge replays one frame later and can override a peer
    // stop that landed in between.
    prev_playing    = (ztPlayer && ztPlayer->playing) ? 1 : 0;
    last_stop_count = ztPlayer ? ztPlayer->stop_count : 0;
}

void zt_ableton_link_pump(void) {
    if (!zt_ableton_link_available()) return;
    link_pump_body();
}
