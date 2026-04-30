// Unit tests for src/sysex_inq.{h,cpp} -- the process-wide SysEx
// receive queue. Pure code, no SDL / MIDI dependency.

#include "sysex_inq.h"
#include "sysex_inq.cpp"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

static void test_empty_queue() {
    zt_sysex_inq_clear();
    CHECK(zt_sysex_inq_size() == 0);
    unsigned char buf[16];
    CHECK(zt_sysex_inq_pop(buf, sizeof(buf)) == 0);
}

static void test_push_pop_roundtrip() {
    zt_sysex_inq_clear();
    unsigned char msg[] = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};
    CHECK(zt_sysex_inq_push(msg, sizeof(msg)) == 1);
    CHECK(zt_sysex_inq_size() == 1);
    unsigned char out[64];
    int n = zt_sysex_inq_pop(out, sizeof(out));
    CHECK(n == (int)sizeof(msg));
    CHECK(memcmp(out, msg, sizeof(msg)) == 0);
    CHECK(zt_sysex_inq_size() == 0);
}

static void test_fifo_order() {
    zt_sysex_inq_clear();
    unsigned char a[] = {0xF0, 0x01, 0xF7};
    unsigned char b[] = {0xF0, 0x02, 0x02, 0xF7};
    unsigned char c[] = {0xF0, 0x03, 0x03, 0x03, 0xF7};
    zt_sysex_inq_push(a, sizeof(a));
    zt_sysex_inq_push(b, sizeof(b));
    zt_sysex_inq_push(c, sizeof(c));
    CHECK(zt_sysex_inq_size() == 3);
    unsigned char out[16];
    int n = zt_sysex_inq_pop(out, sizeof(out));
    CHECK(n == 3 && out[1] == 0x01);
    n = zt_sysex_inq_pop(out, sizeof(out));
    CHECK(n == 4 && out[1] == 0x02);
    n = zt_sysex_inq_pop(out, sizeof(out));
    CHECK(n == 5 && out[1] == 0x03);
}

static void test_overflow_drops_oldest() {
    zt_sysex_inq_clear();
    unsigned char msg[] = {0xF0, 0x00, 0xF7};
    for (int i = 0; i < ZT_SYSEX_QUEUE_SLOTS + 4; i++) {
        msg[1] = (unsigned char)i;
        zt_sysex_inq_push(msg, sizeof(msg));
    }
    CHECK(zt_sysex_inq_size() == ZT_SYSEX_QUEUE_SLOTS);
    // Oldest 4 dropped; first one we pop should be index 4.
    unsigned char out[16];
    int n = zt_sysex_inq_pop(out, sizeof(out));
    CHECK(n == 3);
    CHECK(out[1] == 4);
}

static void test_pop_buffer_too_small() {
    zt_sysex_inq_clear();
    unsigned char msg[] = {0xF0, 0xAA, 0xBB, 0xCC, 0xDD, 0xF7};
    zt_sysex_inq_push(msg, sizeof(msg));
    unsigned char tiny[3];
    CHECK(zt_sysex_inq_pop(tiny, sizeof(tiny)) == 0);   // too small
    CHECK(zt_sysex_inq_size() == 1);                    // message still queued
    unsigned char big[16];
    CHECK(zt_sysex_inq_pop(big, sizeof(big)) == 6);
}

static void test_invalid_inputs() {
    zt_sysex_inq_clear();
    CHECK(zt_sysex_inq_push(NULL, 4) == 0);
    unsigned char buf[4] = {0,0,0,0};
    CHECK(zt_sysex_inq_push(buf, 0) == 0);
    CHECK(zt_sysex_inq_push(buf, -1) == 0);
    CHECK(zt_sysex_inq_push(buf, ZT_SYSEX_MAX_LEN + 1) == 0);
    CHECK(zt_sysex_inq_pop(NULL, 4) == 0);
    CHECK(zt_sysex_inq_pop(buf, 0) == 0);
}

int main(void) {
    test_empty_queue();
    test_push_pop_roundtrip();
    test_fifo_order();
    test_overflow_drops_oldest();
    test_pop_buffer_too_small();
    test_invalid_inputs();
    printf("sysex_inq: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
