/*************************************************************************
 * ableton_link.h
 *
 * zt <-> Ableton Link integration.
 *
 * Ableton Link (https://github.com/Ableton/link) keeps musical tempo — and
 * optionally transport (play/stop) — in sync across applications on a local
 * network (Ableton Live, and most modern DAWs / mobile music apps speak it).
 * zTracker uses it to follow / drive a shared session tempo so it can jam
 * alongside Live without a MIDI-clock cable.
 *
 * DESIGN: the integration OBSERVES the app instead of being notified by it.
 * The per-frame pump watches the player (playing, stop_count) and the song
 * generation, so plain ztPlayer->play()/stop() calls anywhere are shared
 * with peers and a pending quantized start is cancelled when the context
 * changes — call sites need no Link code. The one exception is
 * zt_ableton_link_defer_play(), called by player::play() itself to arm a
 * downbeat-quantized start when Link is enabled.
 *
 * THREADING: every function here must be called from the main/UI thread
 * (the per-frame loop + UI handlers). The player's stop_count exists
 * precisely so other threads never need to call in.
 *************************************************************************/

#ifndef ZT_ABLETON_LINK_H

#define ZT_ABLETON_LINK_H

#include <stddef.h>

/* Call on zt startup */
void   zt_ableton_link_startup(void);

/* Leave the session and tear down. */
void   zt_ableton_link_teardown(void);

/* Call once per main loop. */
void   zt_ableton_link_pump(void);

/* Called by player::play() only: when Link is enabled, arm a start quantized
 * to the next downbeat and return nonzero (play() returns; the pump fires
 * play_immediately() at the downbeat). Returns 0 when Link is off: play() proceeds. */
int    zt_ableton_link_defer_play(int row, int pattern, int pm);

/* 1 when this build has Link compiled in, 0 for the stub. Lets the UI show
 * "(not compiled in)" instead of a dead toggle. */
int    zt_ableton_link_available(void);

/* Remote peers currently in the session (excludes self). For the F11 status
 * line. 0 in the stub / when not enabled. */
size_t zt_ableton_link_num_peers(void);

/* Current session tempo (cached value when not enabled). For the F11 status
 * line. */
double zt_ableton_link_get_tempo(void);

#endif /* ZT_ABLETON_LINK_H */
