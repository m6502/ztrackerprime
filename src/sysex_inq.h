#ifndef _ZT_SYSEX_INQ_H_
#define _ZT_SYSEX_INQ_H_

// Process-wide queue of incoming SysEx messages.
//
// MIDI input callbacks on each platform parse F0..F7 frames out of the
// raw byte stream and push complete messages here. Consumers (e.g. the
// upcoming Send/Recv SysEx page) call zt_sysex_inq_pop() to drain the
// queue.
//
// Storage is bounded -- both the per-message length cap and the total
// queued-frames cap. Overflow drops the oldest message rather than
// growing without bound; this matches the behaviour of midiInQueue,
// which silently overwrites on overflow.
//
// Lock-free single-producer / single-consumer is sufficient today
// (one MIDI-in thread, one consumer per frame on the UI thread) but
// the access functions still take a mutex internally to keep the API
// simple and correct under future producer changes (e.g. multiple
// open MIDI-in devices).

#include <stddef.h>

#define ZT_SYSEX_MAX_LEN     8192   // single message cap (sysex >8KB are rare)
#define ZT_SYSEX_QUEUE_SLOTS 16     // up to 16 messages buffered before overwrite

#ifdef __cplusplus
extern "C" {
#endif

// Reset the queue (drop everything). Safe to call from any thread; cheap.
void zt_sysex_inq_clear(void);

// Push a complete SysEx message (must include 0xF0..0xF7 framing). If
// the queue is full, the oldest message is dropped. Returns 1 on push,
// 0 if `len` is invalid or `bytes` is NULL.
int  zt_sysex_inq_push(const unsigned char *bytes, int len);

// Pop the next queued message into `out`, capacity `out_cap`. Returns
// the number of bytes copied (0 if the queue is empty or `out_cap` was
// too small to hold the next message). On overflow-too-small, the
// message is left in place; the caller should retry with a larger
// buffer or call zt_sysex_inq_clear() to discard it.
int  zt_sysex_inq_pop(unsigned char *out, int out_cap);

// Number of queued messages.
int  zt_sysex_inq_size(void);

#ifdef __cplusplus
}
#endif

#endif // _ZT_SYSEX_INQ_H_
