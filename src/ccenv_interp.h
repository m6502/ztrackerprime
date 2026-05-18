/* ccenv_interp.h — pure linear-interpolation helper for CC envelopes.
 *
 * SDL-free + module.h-free header so it links into the unit-test
 * harness without dragging in the rest of zTracker. The runtime
 * (playback.cpp) and the editor (CUI_CCEnvelopeEditor.cpp) call into
 * this; the tests verify every edge case here once and the call sites
 * stay trivial.
 *
 * Two flavors:
 *   ccenv_interp_raw() works on a struct-free POD (tick[], value[],
 *   num_nodes). Used by tests with hand-built fixtures and by the
 *   compiled-in runtime via a thin wrapper.
 *
 *   ccenv_interp() takes a ccenvelope* directly (uses module.h types).
 *   Defined inline in module-aware translation units only — we don't
 *   include module.h here so the unit-test harness stays SDL-free.
 */
#ifndef _CCENV_INTERP_H_
#define _CCENV_INTERP_H_

#ifdef __cplusplus
extern "C++" {
#endif

/* Returns interpolated 0..127 value at `pos` (subticks).
 *
 * `tick` is the x-axis (monotonically non-decreasing).
 * `value` is the y-axis (0..127).
 * `num_nodes` >= 1.
 *
 * - pos <= tick[0]              -> value[0]
 * - pos >= tick[num_nodes-1]    -> value[num_nodes-1]
 * - between adjacent nodes      -> linear interpolation
 *
 * Two adjacent nodes with the same tick (vertical step) resolve to
 * the right-hand value; this matches Schism's behaviour and lets the
 * user create instant jumps.
 */
static inline int ccenv_interp_raw(const unsigned short *tick,
                                   const unsigned char  *value,
                                   int num_nodes,
                                   int pos)
{
    if (num_nodes <= 0) return 0;
    if (num_nodes == 1) return value[0];
    /* Strict-less clamp on the left so a vertical step (two adjacent
     * nodes with the same tick at the start) resolves to the right
     * side's value. Mirrored on the right: at the last node the
     * envelope is "done" and just emits the last value. */
    if (pos < (int)tick[0])              return value[0];
    if (pos >= (int)tick[num_nodes - 1]) return value[num_nodes - 1];

    /* Find the segment [i-1, i] that contains pos. Linear scan is
     * fine -- num_nodes <= 32. */
    int i;
    for (i = 1; i < num_nodes; i++) {
        if ((int)tick[i] >= pos) break;
    }
    if (i >= num_nodes) i = num_nodes - 1;

    int x0 = (int)tick[i - 1],  x1 = (int)tick[i];
    int y0 = (int)value[i - 1], y1 = (int)value[i];
    if (x1 <= x0) return y1; /* vertical step */

    int v = y0 + ((pos - x0) * (y1 - y0)) / (x1 - x0);
    if (v < 0)   v = 0;
    if (v > 127) v = 127;
    return v;
}

#ifdef __cplusplus
}
#endif

#endif /* _CCENV_INTERP_H_ */
