/*************************************************************************
 *
 * FILE  sample.h
 *
 * DESCRIPTION
 *   PCM sample storage for zTracker's optional sample-playback path.
 *
 *   zTracker is historically MIDI-only. Samples are an additive,
 *   opt-in capability: a song carries a small pool of `zt_sample`
 *   objects, and an instrument MAY link to one via its sample_index.
 *   When no samples are present the song behaves exactly as before and
 *   saves byte-identically (the optional SMPL/SMIX .zt chunks are only
 *   emitted when there is something to write).
 *
 *   This header is deliberately SDL-free so the data model and the
 *   chunk serialise/parse logic can be unit-tested under the SDL-free
 *   test harness. The WAV loader (sample_load.{h,cpp}) is the only
 *   piece that pulls in SDL.
 *
 *   PR A (foundation): storage + .zt persistence + WAV loader. No audio
 *   engine yet -- nothing here makes sound. The SampleOutputDevice voice
 *   mixer that consumes this pool lands in PR B.
 *
 ******/
#ifndef _SAMPLE_H
#define _SAMPLE_H

/* Max samples in a song's pool. Matches Impulse Tracker's 99-sample
 * convention with one slot of headroom; also the upper bound the SMPL
 * loader bounds-checks pool_index against. */
#define ZTM_MAX_SAMPLES        100
#define ZTM_SAMPLENAME_MAXLEN  32

/* zt_sample::flags bits */
#define ZTM_SMPF_LOOP          0x01   /* loop_start..loop_end is an active forward loop */
#define ZTM_SMPF_PINGPONG      0x02   /* reserved for the engine (PR B); ignored in PR A */

/* Default MIDI note at which PCM plays back at its stored `rate`. */
#define ZTM_SAMPLE_DEFAULT_ROOT 60    /* C5 in zTracker's numbering */

/* One PCM sample in a song's pool.
 *
 * PCM is stored as interleaved signed 16-bit frames -- `channels`
 * samples per frame, `frames` frames total. This matches the audio
 * backend's SDL_AUDIO_S16 output format so the future voice mixer reads
 * the pool with no per-sample format conversion.
 *
 * Frame vs sample vs byte (the trio that bit the spike's TestTone):
 *   - a FRAME is one instant in time: `channels` 16-bit values.
 *   - SAMPLE count  = frames * channels.
 *   - BYTE   count  = frames * channels * sizeof(short int).
 * Loop points are expressed in FRAMES.
 */
class zt_sample
{
public:
    char           name[ZTM_SAMPLENAME_MAXLEN];
    short int     *pcm;          /* interleaved S16 frames; NULL when empty */
    unsigned int   frames;       /* number of frames (not samples, not bytes) */
    unsigned char  channels;     /* 1 = mono, 2 = stereo */
    unsigned char  root_note;    /* MIDI note that plays pcm at `rate` */
    unsigned char  flags;        /* ZTM_SMPF_* */
    signed char    finetune;     /* engine-defined fine pitch offset; 0 = none */
    unsigned char  default_vol;  /* 0..127 */
    unsigned int   rate;         /* source sample rate, Hz (e.g. IT C5speed) */
    unsigned int   loop_start;   /* in frames */
    unsigned int   loop_end;     /* in frames; no loop when flag clear / end<=start */

    zt_sample(void);
    ~zt_sample(void);

    /* True when there is no PCM attached (no frames / null buffer). */
    int  isempty(void) const;

    /* Free any PCM and reset every field to the empty default. Safe to
     * call repeatedly. */
    void clear(void);

    /* Replace the PCM with a copy of `src` (nframes * nchannels S16
     * values, interleaved). `src` may be NULL/0 to just clear. Returns
     * 1 on success, 0 on allocation failure (leaves the sample empty). */
    int  set_pcm(const short int *src, unsigned int nframes, unsigned char nchannels);

    /* frames * channels * sizeof(short int). 0 when empty. */
    unsigned int byte_size(void) const;
};

#endif /* _SAMPLE_H */
/* eof */
