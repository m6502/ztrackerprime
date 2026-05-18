/* ccenv_advance.h — pure per-subtick advance logic for CC envelopes.
 *
 * Mirrors Schism's _process_envelope() (schismtracker/player/sndmix.c
 * ~line 470). One call = one subtick advance. The caller owns the
 * voice state; this function reads the envelope struct (passed as
 * loose POD pointers to stay SDL-free + module.h-free for tests) and
 * mutates the position + done flag.
 *
 * Sustain / loop semantics:
 *
 *   key_off == 0:  if SUSTAIN flag set, position wraps between
 *                  tick[sustain_start] and tick[sustain_end] when it
 *                  walks past sustain_end. Otherwise falls through
 *                  to the LOOP / terminate branch.
 *
 *   key_off != 0:  sustain region is skipped. If LOOP flag set,
 *                  position wraps between tick[loop_start] and
 *                  tick[loop_end] when it walks past loop_end.
 *                  Otherwise, when position reaches the final
 *                  tick, `done` is set and the caller deactivates
 *                  the voice (after emitting the last value).
 */
#ifndef _CCENV_ADVANCE_H_
#define _CCENV_ADVANCE_H_

#include "ccenv_interp.h"

#ifdef __cplusplus
extern "C++" {
#endif

/* Flag masks must match the ZTM_CCENVF_* in module.h. Duplicated
 * here so the header stays module.h-free for tests. A compile-time
 * static_assert in module.cpp guards against drift. */
#define CCENV_F_ENABLED  0x01
#define CCENV_F_LOOP     0x02
#define CCENV_F_SUSTAIN  0x04
#define CCENV_F_CARRY    0x08

struct ccenv_voice_state {
    int  position;     /* current subtick within envelope timeline */
    int  done;         /* 1 = envelope finished (caller should release voice) */
    int  key_off;      /* 1 = note-off received; sustain region abandoned */
};

/* Advance the voice by one subtick. Returns the interpolated value
 * (0..127) at the *post-advance* position so the caller can emit it
 * immediately. */
static inline int ccenv_step(struct ccenv_voice_state *vs,
                             const unsigned short *tick,
                             const unsigned char  *value,
                             int num_nodes,
                             unsigned char flags,
                             int loop_start, int loop_end,
                             int sustain_start, int sustain_end)
{
    if (!vs || num_nodes <= 0 || !(flags & CCENV_F_ENABLED)) {
        if (vs) vs->done = 1;
        return 0;
    }
    if (vs->done) {
        return ccenv_interp_raw(tick, value, num_nodes,
                                tick[num_nodes - 1]);
    }

    vs->position++;

    /* Sustain region (only while key held). The region is
     * inclusive: [tick[sustain_start], tick[sustain_end]]. */
    if (!vs->key_off
        && (flags & CCENV_F_SUSTAIN)
        && sustain_start >= 0 && sustain_end >= sustain_start
        && sustain_end < num_nodes) {
        int s0 = (int)tick[sustain_start];
        int s1 = (int)tick[sustain_end];
        if (vs->position > s1) {
            /* Snap back to s0. Degenerate s0==s1 (single-node sustain)
             * latches at s0. */
            vs->position = (s1 > s0) ? s0 : s0;
        }
        return ccenv_interp_raw(tick, value, num_nodes, vs->position);
    }

    /* Regular loop region (key held + no sustain, OR after note-off). */
    if ((flags & CCENV_F_LOOP)
        && loop_start >= 0 && loop_end >= loop_start
        && loop_end < num_nodes) {
        int l0 = (int)tick[loop_start];
        int l1 = (int)tick[loop_end];
        if (vs->position > l1) {
            vs->position = (l1 > l0) ? l0 : l0;
        }
        return ccenv_interp_raw(tick, value, num_nodes, vs->position);
    }

    /* No loop, no sustain (or both inactive). Terminate at last node. */
    int last = (int)tick[num_nodes - 1];
    if (vs->position >= last) {
        vs->position = last;
        vs->done = 1;
    }
    return ccenv_interp_raw(tick, value, num_nodes, vs->position);
}

#ifdef __cplusplus
}
#endif

#endif /* _CCENV_ADVANCE_H_ */
