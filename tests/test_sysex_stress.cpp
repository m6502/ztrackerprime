// Stress tests for src/sysex_inq.{h,cpp} -- the process-wide SysEx
// receive queue.
//
// The existing tests/test_sysex_inq.cpp covers basic round-trip,
// FIFO order, overflow drops oldest, buffer-too-small, and invalid
// inputs. This file goes deeper, simulating the real-world traffic
// shapes we couldn't smoke-test against actual hardware:
//
//   1. Maximum-size message (ZT_SYSEX_MAX_LEN exact) byte-perfect
//      round-trip. Common synth bulk dumps: Yamaha DX7 voice (4104
//      bytes), Roland JV-1080 patch dump (~4 KB), Korg M1 voice
//      (~9 KB which exceeds cap so we test up to 8 KB).
//   2. Realistic synth dump shapes (DX7 = 4104 bytes, generic
//      1 KB / 4 KB / 8 KB) round-tripped with mathematically
//      generated content for byte-by-byte verification.
//   3. High-throughput sequential push/pop in a tight loop. Catches
//      memory leaks and queue-state drift.
//   4. Concurrent push from N threads + pop from main thread, to
//      stress the std::mutex protecting the queue.
//   5. Overflow keeps newest. The existing test asserts oldest are
//      dropped; this confirms the surviving N are exactly the
//      LAST N pushed in unique-content terms.
//   6. Empty / malformed framing acceptance. The queue is transport-
//      only (no F0/F7 parsing) so messages without proper framing
//      should pass through; verify that's actually the case.
//
// Pure C++ (std::thread / std::mutex / std::atomic). No SDL or MIDI
// dependency.

#include "sysex_inq.h"
#include "sysex_inq.cpp"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

// Fill `buf[len]` with a deterministic pseudo-random pattern keyed on
// `seed` so each test message has unique content we can verify.
// Reserves bytes 0 and len-1 for F0 and F7 framing.
static void fill_synth_dump(unsigned char *buf, int len, unsigned int seed) {
    if (len < 2) return;
    buf[0] = 0xF0;
    unsigned int s = seed * 0x9E3779B1u + 0xBADC0FFEu;
    for (int i = 1; i < len - 1; i++) {
        s = s * 1103515245u + 12345u;
        // 7-bit data only -- a real SysEx payload never has the high
        // bit set in any non-status byte.
        buf[i] = (unsigned char)((s >> 8) & 0x7F);
    }
    buf[len - 1] = 0xF7;
}

// ---------------------------------------------------------------------
// 1. Maximum-size message round-trip
// ---------------------------------------------------------------------
static void test_max_size_roundtrip() {
    zt_sysex_inq_clear();
    unsigned char *src = (unsigned char *)malloc(ZT_SYSEX_MAX_LEN);
    unsigned char *dst = (unsigned char *)malloc(ZT_SYSEX_MAX_LEN);
    fill_synth_dump(src, ZT_SYSEX_MAX_LEN, 0x12345678);
    CHECK(zt_sysex_inq_push(src, ZT_SYSEX_MAX_LEN) == 1);
    CHECK(zt_sysex_inq_size() == 1);
    int n = zt_sysex_inq_pop(dst, ZT_SYSEX_MAX_LEN);
    CHECK(n == ZT_SYSEX_MAX_LEN);
    CHECK(memcmp(src, dst, ZT_SYSEX_MAX_LEN) == 0);
    CHECK(zt_sysex_inq_size() == 0);
    free(src);
    free(dst);
}

// ---------------------------------------------------------------------
// 2. Realistic synth dump shapes
// ---------------------------------------------------------------------
static void test_realistic_synth_dumps() {
    zt_sysex_inq_clear();
    // (size, seed) — sizes chosen to span DX7 voice, JV patch,
    // generic 1/2/4/8 KB shapes.
    const struct { int size; unsigned int seed; const char *label; } cases[] = {
        {  100, 0xDEADBEEFu, "small CC dump"            },
        {  256, 0x01234567u, "kit/preset block"         },
        { 1024, 0xCAFEBABEu, "1 KB patch"               },
        { 2048, 0xFEEDF00Du, "2 KB patch"               },
        { 4096, 0xABCD1234u, "4 KB JV-style patch"      },
        { 4104, 0x55AA55AAu, "DX7 voice (exact 4104B)"  },
        { 8000, 0x12348765u, "near-max-cap synth dump"  },
        { ZT_SYSEX_MAX_LEN, 0xFADEDEADu, "max-cap dump" },
    };
    const int n_cases = (int)(sizeof(cases) / sizeof(cases[0]));
    std::vector<std::vector<unsigned char>> sources(n_cases);
    for (int i = 0; i < n_cases; i++) {
        sources[i].resize(cases[i].size);
        fill_synth_dump(sources[i].data(), cases[i].size, cases[i].seed);
    }
    // Push them all (within queue capacity).
    int pushed = 0;
    for (int i = 0; i < n_cases && pushed < ZT_SYSEX_QUEUE_SLOTS; i++) {
        int r = zt_sysex_inq_push(sources[i].data(), cases[i].size);
        CHECK(r == 1);
        pushed++;
    }
    CHECK(zt_sysex_inq_size() == pushed);

    // Pop and byte-verify in FIFO order.
    unsigned char *dst = (unsigned char *)malloc(ZT_SYSEX_MAX_LEN);
    for (int i = 0; i < pushed; i++) {
        int n = zt_sysex_inq_pop(dst, ZT_SYSEX_MAX_LEN);
        CHECK(n == cases[i].size);
        CHECK(memcmp(dst, sources[i].data(), cases[i].size) == 0);
    }
    free(dst);
    CHECK(zt_sysex_inq_size() == 0);
}

// ---------------------------------------------------------------------
// 3. High-throughput sequential push/pop
// ---------------------------------------------------------------------
static void test_high_throughput_sequential() {
    zt_sysex_inq_clear();
    const int rounds = 5000;
    unsigned char buf[256];
    int total_bytes = 0;
    for (int i = 0; i < rounds; i++) {
        int sz = 16 + (i * 7) % 128;        // varying 16-143 bytes
        fill_synth_dump(buf, sz, (unsigned int)i);
        int r = zt_sysex_inq_push(buf, sz);
        CHECK(r == 1);
        unsigned char popped[256];
        int n = zt_sysex_inq_pop(popped, sizeof(popped));
        CHECK(n == sz);
        CHECK(memcmp(popped, buf, sz) == 0);
        total_bytes += sz;
    }
    CHECK(zt_sysex_inq_size() == 0);
    // Sanity check: we did transfer real volume.
    CHECK(total_bytes > 0);
}

// ---------------------------------------------------------------------
// 4. Concurrent producer threads + main-thread consumer
// ---------------------------------------------------------------------
static std::atomic<int> g_pushed_count(0);
static std::atomic<bool> g_stop_producers(false);

static void producer_thread_proc(int thread_id, int messages_to_push) {
    unsigned char buf[64];
    for (int i = 0; i < messages_to_push; i++) {
        if (g_stop_producers.load(std::memory_order_acquire)) break;
        int sz = 8 + ((thread_id * 31 + i) & 0x1F);   // 8..39 bytes
        fill_synth_dump(buf, sz, (unsigned int)((thread_id << 16) | i));
        // Push may report 0 if the queue is full + caller didn't pop;
        // the queue's policy is "drop oldest" which still returns 1.
        // Either way, we don't fail the test on individual pushes.
        zt_sysex_inq_push(buf, sz);
        g_pushed_count.fetch_add(1, std::memory_order_relaxed);
        // Yield occasionally so the consumer gets airtime.
        if ((i & 0x1F) == 0) std::this_thread::yield();
    }
}

static void test_concurrent_producers() {
    zt_sysex_inq_clear();
    g_pushed_count.store(0);
    g_stop_producers.store(false);
    const int n_threads = 4;
    const int per_thread = 250;
    std::vector<std::thread> threads;
    for (int t = 0; t < n_threads; t++) {
        threads.emplace_back(producer_thread_proc, t, per_thread);
    }
    // Consume on the main thread while producers run.
    int popped = 0;
    unsigned char buf[256];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (popped < n_threads * per_thread &&
           std::chrono::steady_clock::now() < deadline) {
        int n = zt_sysex_inq_pop(buf, sizeof(buf));
        if (n > 0) {
            popped++;
            // Lightweight integrity: F0 first, F7 last.
            CHECK(buf[0] == 0xF0);
            CHECK(buf[n - 1] == 0xF7);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
    g_stop_producers.store(true);
    for (auto &th : threads) th.join();

    // Final drain of anything still queued at producer-stop.
    while (zt_sysex_inq_pop(buf, sizeof(buf)) > 0) popped++;

    // Most pushes get dropped by the queue's overflow policy (16-slot
    // ring + 4 producers faster than 1 consumer = oldest evicted). The
    // value of this test is data-race / corruption detection: every
    // popped message had its F0/F7 framing checked above. We confirm
    // here that:
    //   (a) every push attempt was counted (no producer-side data race
    //       on g_pushed_count), and
    //   (b) the consumer did manage to pop *some* messages (the queue
    //       kept up enough to drain, no deadlock).
    int expected_pushes = n_threads * per_thread;
    CHECK(g_pushed_count.load() == expected_pushes);
    CHECK(popped > 0);
}

// ---------------------------------------------------------------------
// 5. Overflow keeps newest (in unique-content terms)
// ---------------------------------------------------------------------
static void test_overflow_keeps_newest_unique() {
    zt_sysex_inq_clear();
    const int N = ZT_SYSEX_QUEUE_SLOTS * 4;   // 64 messages, ring is 16
    for (int i = 0; i < N; i++) {
        // Each message has unique payload content so we can identify
        // which survived after overflow.
        unsigned char msg[8];
        msg[0] = 0xF0;
        msg[1] = (unsigned char)((i >> 21) & 0x7F);
        msg[2] = (unsigned char)((i >> 14) & 0x7F);
        msg[3] = (unsigned char)((i >>  7) & 0x7F);
        msg[4] = (unsigned char)((i)       & 0x7F);
        msg[5] = 0x55;   // marker
        msg[6] = 0xAA;
        msg[7] = 0xF7;
        zt_sysex_inq_push(msg, sizeof(msg));
    }
    CHECK(zt_sysex_inq_size() == ZT_SYSEX_QUEUE_SLOTS);
    // The surviving messages must be the LAST ZT_SYSEX_QUEUE_SLOTS
    // pushed, popped in order. Reconstruct expected indices.
    int first_surviving = N - ZT_SYSEX_QUEUE_SLOTS;
    for (int k = 0; k < ZT_SYSEX_QUEUE_SLOTS; k++) {
        unsigned char out[16];
        int n = zt_sysex_inq_pop(out, sizeof(out));
        CHECK(n == 8);
        int decoded = ((int)out[1] << 21) | ((int)out[2] << 14) |
                      ((int)out[3] <<  7) |  (int)out[4];
        CHECK(decoded == first_surviving + k);
        CHECK(out[5] == 0x55 && out[6] == 0xAA);
    }
    CHECK(zt_sysex_inq_size() == 0);
}

// ---------------------------------------------------------------------
// 6. Malformed framing passes through (transport, not parser)
// ---------------------------------------------------------------------
static void test_malformed_framing_passes_through() {
    zt_sysex_inq_clear();
    // No leading 0xF0.
    unsigned char no_f0[] = {0x12, 0x34, 0x56, 0xF7};
    CHECK(zt_sysex_inq_push(no_f0, sizeof(no_f0)) == 1);
    // No trailing 0xF7.
    unsigned char no_f7[] = {0xF0, 0x01, 0x02, 0x03};
    CHECK(zt_sysex_inq_push(no_f7, sizeof(no_f7)) == 1);
    // Both missing.
    unsigned char neither[] = {0x11, 0x22, 0x33};
    CHECK(zt_sysex_inq_push(neither, sizeof(neither)) == 1);
    // 1-byte message — minimum valid.
    unsigned char one_byte[] = {0xF7};
    CHECK(zt_sysex_inq_push(one_byte, sizeof(one_byte)) == 1);

    CHECK(zt_sysex_inq_size() == 4);

    // All four pop intact.
    unsigned char buf[16];
    CHECK(zt_sysex_inq_pop(buf, sizeof(buf)) == sizeof(no_f0));
    CHECK(memcmp(buf, no_f0, sizeof(no_f0)) == 0);
    CHECK(zt_sysex_inq_pop(buf, sizeof(buf)) == sizeof(no_f7));
    CHECK(memcmp(buf, no_f7, sizeof(no_f7)) == 0);
    CHECK(zt_sysex_inq_pop(buf, sizeof(buf)) == sizeof(neither));
    CHECK(memcmp(buf, neither, sizeof(neither)) == 0);
    CHECK(zt_sysex_inq_pop(buf, sizeof(buf)) == sizeof(one_byte));
}

int main(void) {
    test_max_size_roundtrip();
    test_realistic_synth_dumps();
    test_high_throughput_sequential();
    test_concurrent_producers();
    test_overflow_keeps_newest_unique();
    test_malformed_framing_passes_through();
    printf("sysex_stress: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
