/*************************************************************************
 * midi_clock_sync.h
 *
 * MIDI clock slave sync (midi_in_sync / midi_in_sync_chase_tempo), built
 * on the same observer architecture as the Ableton Link integration: the
 * MIDI-in callback thread only RECORDS what arrived (plain-int counters,
 * same advisory contract as player::stop_count); all decisions -- starting
 * and stopping the player, deriving the master tempo, phase-locking the
 * engine -- happen in zt_midi_clock_pump() on the main thread, once per
 * frame. Drives the engine through the shared phase_chase.h controller, so
 * a fractional-tempo master cannot drift, and honors sync_offset_ms (play
 * early to beat a DAW's monitoring latency) identically to Link.
 *
 * A MIDI clock stream is a position reference, not just a rate: 24 clocks
 * per beat = 4 subticks per clock. Counting clocks since the consumed
 * START gives "expected position" exactly as Link's beatAtTime does.
 *************************************************************************/
#ifndef ZT_MIDI_CLOCK_SYNC_H
#define ZT_MIDI_CLOCK_SYNC_H

#include <atomic>

/* Observer counters, bumped ONLY by the MIDI-in callback thread
 * (midi-io.cpp), read by the pump on the main thread. std::atomic, not plain
 * int: a value written on one thread and read on another is a data race in
 * the C++ memory model -- atomic makes it well-defined (ThreadSanitizer-clean).
 * We rely only on per-counter atomicity, not on any ordering BETWEEN counters:
 * each is an independent monotonic tally and the pump re-reads every frame,
 * tolerating a bump seen a frame late. Cost is nil on the targets we ship --
 * an aligned-word load/store, same as the plain int it replaces. */
extern std::atomic<int>      g_mclk_clocks;        /* 0xF8 count, monotonic           */
extern std::atomic<unsigned> g_mclk_last_clock_ms; /* SDL_GetTicks() at the last 0xF8 */
extern std::atomic<int>      g_mclk_start_req;     /* 0xFA count                      */
extern std::atomic<int>      g_mclk_continue_req;  /* 0xFB count                      */
extern std::atomic<int>      g_mclk_stop_req;      /* 0xFC count                      */
extern std::atomic<int>      g_mclk_spp_raw;       /* last 0xF2 value (14-bit)        */
extern std::atomic<int>      g_mclk_spp_seq;       /* bumped per 0xF2                 */

/* Per-frame pump: consume transport requests (start/continue/stop on the
 * main thread), estimate the master tempo as a double from clock arrival
 * counts, and phase-lock the playing engine to the clock grid. Call once
 * per main loop, next to zt_ableton_link_pump(). */
void zt_midi_clock_pump(void);

#endif /* ZT_MIDI_CLOCK_SYNC_H */
