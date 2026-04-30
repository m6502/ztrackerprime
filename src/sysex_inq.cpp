#include "sysex_inq.h"

#include <mutex>
#include <string.h>

namespace {

struct Slot {
    int len;
    unsigned char data[ZT_SYSEX_MAX_LEN];
};

Slot       g_slots[ZT_SYSEX_QUEUE_SLOTS];
int        g_head  = 0;    // next read slot
int        g_tail  = 0;    // next write slot
int        g_count = 0;
std::mutex g_mu;

} // namespace

extern "C" void zt_sysex_inq_clear(void) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_head = g_tail = g_count = 0;
}

extern "C" int zt_sysex_inq_push(const unsigned char *bytes, int len) {
    if (!bytes || len <= 0 || len > ZT_SYSEX_MAX_LEN) return 0;
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_count == ZT_SYSEX_QUEUE_SLOTS) {
        // Overflow: drop oldest.
        g_head = (g_head + 1) % ZT_SYSEX_QUEUE_SLOTS;
        g_count--;
    }
    Slot *s = &g_slots[g_tail];
    s->len = len;
    memcpy(s->data, bytes, (size_t)len);
    g_tail = (g_tail + 1) % ZT_SYSEX_QUEUE_SLOTS;
    g_count++;
    return 1;
}

extern "C" int zt_sysex_inq_pop(unsigned char *out, int out_cap) {
    if (!out || out_cap <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_count == 0) return 0;
    Slot *s = &g_slots[g_head];
    if (s->len > out_cap) return 0;     // caller's buffer too small; leave in place
    int n = s->len;
    memcpy(out, s->data, (size_t)n);
    g_head = (g_head + 1) % ZT_SYSEX_QUEUE_SLOTS;
    g_count--;
    return n;
}

extern "C" int zt_sysex_inq_size(void) {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_count;
}
