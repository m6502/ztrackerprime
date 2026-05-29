/*************************************************************************
 *
 * FILE  sample_voice.h
 *
 * DESCRIPTION
 *   Pure (SDL-free) DSP for the sample voice mixer: pitch-from-note,
 *   fractional-position advance with optional forward loop, and
 *   linear-interpolated PCM read. Kept header-only and dependency-free
 *   so it unit-tests without an audio device (tests/test_sample_voice).
 *
 *   The stateful SampleOutputDevice in OutputDevices.{h,cpp} is a thin
 *   shell around these functions.
 *
 ******/
#ifndef _SAMPLE_VOICE_H
#define _SAMPLE_VOICE_H

#include "sample.h"
#include <math.h>

/* Frames of source PCM to advance per output frame, for `note` played on
 * sample `s` to an output stream running at `out_rate` Hz. A note equal to
 * the sample's root_note plays at the sample's natural rate (resampled to
 * out_rate); each semitone above/below scales by 2^(1/12). `finetune` is
 * interpreted as cents. Returns 0 for invalid input. */
static inline double zt_voice_step(const zt_sample *s, int note, int out_rate)
{
    if (!s || out_rate <= 0 || s->rate == 0) return 0.0;
    double semis = (double)note - (double)s->root_note + (double)s->finetune / 100.0;
    double pitch = pow(2.0, semis / 12.0);
    return ((double)s->rate / (double)out_rate) * pitch;
}

/* Advance `pos` (in frames) by `step`. With a valid forward loop
 * (ZTM_SMPF_LOOP and loop_end > loop_start) the position wraps back into
 * [loop_start, loop_end). Without a loop, reaching the end clears *active.
 * Returns the new position. */
static inline double zt_voice_advance(const zt_sample *s, double pos, double step,
                                      bool *active)
{
    pos += step;
    bool loop = (s->flags & ZTM_SMPF_LOOP) && s->loop_end > s->loop_start;
    if (loop) {
        if (pos >= (double)s->loop_end) {
            double loplen = (double)(s->loop_end - s->loop_start);
            double over   = pos - (double)s->loop_end;
            pos = (double)s->loop_start + fmod(over, loplen);
        }
    } else if (pos >= (double)s->frames) {
        if (active) *active = false;
        pos = (double)s->frames;
    }
    return pos;
}

/* Linear-interpolated read of channel `c` (0 = left, 1 = right) at
 * fractional frame position `pos`. Mono samples return the same value for
 * both channels. Result is in S16 range, as a float for accumulation. */
static inline float zt_voice_read(const zt_sample *s, double pos, int c)
{
    if (!s || !s->pcm || s->frames == 0) return 0.0f;
    unsigned int i0 = (unsigned int)pos;
    if (i0 >= s->frames) i0 = s->frames - 1;
    unsigned int i1 = (i0 + 1 < s->frames) ? i0 + 1 : i0;
    double frac = pos - (double)i0;
    int ch = (s->channels == 2) ? 2 : 1;
    int cc = (ch == 2) ? (c & 1) : 0;
    short v0 = s->pcm[i0 * ch + cc];
    short v1 = s->pcm[i1 * ch + cc];
    return (float)((1.0 - frac) * (double)v0 + frac * (double)v1);
}

#endif /* _SAMPLE_VOICE_H */
/* eof */
