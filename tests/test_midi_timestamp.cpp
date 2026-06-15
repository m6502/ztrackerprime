// Tests for src/midi_timestamp.h -- the pure mach->SDL-ms conversion and the
// plausibility gate that makes a bad hardware MIDI timestamp harmless.

#include "midi_timestamp.h"
#include <stdio.h>

static int failures = 0, checks = 0;
#define CHECK(e) do { checks++; if (!(e)) { failures++; \
    fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #e); } } while (0)

static void test_convert_basic() {
    // numer/denom = 1/1 -> 1 tick == 1 ns. A packet 2,000,000 ns (2 ms) before
    // now maps to now_sdl - 2.
    CHECK(zt_mach_stamp_to_sdl_ms(/*pkt*/8000000, /*now_mach*/10000000,
                                  /*now_sdl*/5000, 1, 1) == 4998);
    // A fresh packet (same instant) maps to now_sdl exactly.
    CHECK(zt_mach_stamp_to_sdl_ms(10000000, 10000000, 5000, 1, 1) == 5000);
}

static void test_convert_timebase() {
    // Apple-Silicon-ish timebase 125/3: 1 tick = 41.667 ns. 720,000 ticks =
    // 30,000,000 ns = 30 ms before now.
    CHECK(zt_mach_stamp_to_sdl_ms(/*pkt*/280000, /*now_mach*/1000000,
                                  /*now_sdl*/1000, 125, 3) == 970);
}

static void test_convert_guards() {
    CHECK(zt_mach_stamp_to_sdl_ms(100, 200, 7777, 1, /*denom=*/0) == 7777); // no timebase
    CHECK(zt_mach_stamp_to_sdl_ms(/*pkt future*/300, 200, 7777, 1, 1) == 7777); // future -> now
}

static void test_pick_gate() {
    // Candidate close to fallback -> use it.
    CHECK(zt_pick_clock_stamp(/*cand*/995, /*fallback*/1000, 2000, 50) == 995);
    // Candidate too old (garbage conversion) -> fallback.
    CHECK(zt_pick_clock_stamp(/*cand*/1000, /*fallback*/5000, 2000, 50) == 5000);
    // Candidate too far in the future -> fallback.
    CHECK(zt_pick_clock_stamp(/*cand*/2000, /*fallback*/1000, 2000, 50) == 1000);
    // Slightly future within tolerance -> keep.
    CHECK(zt_pick_clock_stamp(/*cand*/1020, /*fallback*/1000, 2000, 50) == 1020);
}

static void test_garbage_stamp_is_rejected() {
    // packet_mach = 0 (uninitialized) with a large now_mach -> conversion yields
    // a wildly old value; the gate must reject it and keep the fallback.
    uint32_t cand = zt_mach_stamp_to_sdl_ms(0, 24000000000ull, 100000, 125, 3);
    CHECK(zt_pick_clock_stamp(cand, 100000, 2000, 50) == 100000);
}

int main(void) {
    test_convert_basic();
    test_convert_timebase();
    test_convert_guards();
    test_pick_gate();
    test_garbage_stamp_is_rejected();
    printf("midi_timestamp: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
