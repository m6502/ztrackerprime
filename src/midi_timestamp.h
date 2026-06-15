#ifndef ZT_MIDI_TIMESTAMP_H
#define ZT_MIDI_TIMESTAMP_H

// Pure helpers for using a platform MIDI hardware timestamp as the clock
// arrival time, instead of sampling a software clock inside the input
// callback (which adds OS-scheduling jitter on top of the real arrival).
//
// The MIDI-clock pump compares the stored arrival time against SDL_GetTicks(),
// so any hardware stamp MUST be expressed in that same millisecond epoch.
// macOS CoreMIDI packet timestamps are mach_absolute_time ticks (a different
// epoch), so we convert them using a (now_mach, now_sdl_ms) pair sampled at the
// same instant in the callback, plus the mach timebase (ns = ticks*numer/denom).
//
// SDL-free / CoreMIDI-free on purpose: exhaustively unit-tested
// (tests/test_midi_timestamp.cpp) and used from midi-io.cpp under __APPLE__.
//
// These two are designed to be used TOGETHER: convert, then gate. The gate is
// what makes a garbage hardware stamp harmless -- it falls back to the software
// clock rather than feeding a wild arrival time into the phase lock.

#include <stdint.h>

// Convert a mach_absolute_time packet stamp into the SDL_GetTicks() ms epoch.
// `now_mach`/`now_sdl_ms` are sampled at the same instant; the packet is in the
// past, so its arrival = now_sdl_ms - age. A future-dated packet (can happen
// with scheduled delivery) clamps age to 0. denom==0 (no timebase) -> fallback
// to now_sdl_ms.
static inline uint32_t zt_mach_stamp_to_sdl_ms(uint64_t packet_mach,
                                               uint64_t now_mach,
                                               uint32_t now_sdl_ms,
                                               uint32_t numer, uint32_t denom)
{
    if (denom == 0) return now_sdl_ms;
    if (packet_mach >= now_mach) return now_sdl_ms;          // future / same instant
    uint64_t age_ticks = now_mach - packet_mach;
    // Cap before the multiply so garbage (e.g. packet_mach==0) can't overflow;
    // anything this old is rejected by zt_pick_clock_stamp anyway.
    if (age_ticks > (uint64_t)1 << 40) return now_sdl_ms - 0xFFFFFFu; // -> rejected
    uint64_t age_ms = (age_ticks * (uint64_t)numer / (uint64_t)denom) / 1000000ull;
    if (age_ms > 0xFFFFFFu) age_ms = 0xFFFFFFu;
    return now_sdl_ms - (uint32_t)age_ms;
}

// Accept a hardware-derived stamp only if it's plausible relative to the
// software-clock fallback: not stale (older than max_age_ms) and not in the
// future beyond future_tol_ms. Otherwise use `fallback_ms`. Wrap-safe via
// signed difference.
static inline uint32_t zt_pick_clock_stamp(uint32_t candidate_ms,
                                           uint32_t fallback_ms,
                                           uint32_t max_age_ms,
                                           uint32_t future_tol_ms)
{
    int64_t age = (int64_t)fallback_ms - (int64_t)candidate_ms;
    if (age < -(int64_t)future_tol_ms) return fallback_ms;   // too far future
    if (age >  (int64_t)max_age_ms)    return fallback_ms;   // too old / garbage
    return candidate_ms;
}

#endif // ZT_MIDI_TIMESTAMP_H
