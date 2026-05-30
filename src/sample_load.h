/*************************************************************************
 *
 * FILE  sample_load.h
 *
 * DESCRIPTION
 *   WAV file loading for the sample pool. The only sample-subsystem TU
 *   that depends on SDL (it uses SDL_LoadWAV + SDL_ConvertAudioSamples).
 *
 *   Two entry points:
 *     - zt_sample_load_wav(path, out)    -- load from a file path.
 *     - zt_sample_load_wav_io(io, ...)   -- load from an SDL_IOStream.
 *
 *   The IO variant exists so the loader can be exercised against an
 *   in-memory WAV in the unit test (SDL_IOFromMem) without a temp file
 *   or a committed binary fixture.
 *
 *   Both decode to interleaved S16 (mono or stereo; >2 source channels
 *   are downmixed to stereo), which is exactly what the sample pool and
 *   the future voice mixer expect.
 *
 ******/
#ifndef _SAMPLE_LOAD_H
#define _SAMPLE_LOAD_H

#include <SDL.h>
#include "sample.h"

/* Load a WAV file from `path` into `out` (which is cleared first).
 * `out`'s name is set to the file's basename (extension stripped,
 * truncated to fit). Returns 1 on success, 0 on failure (out left empty). */
int zt_sample_load_wav(const char *path, zt_sample *out);

/* Load a WAV from an already-open SDL_IOStream. If `closeio` is true the
 * stream is closed before returning (success or failure). `name_hint`
 * (may be NULL) is copied into out->name. Returns 1 on success, 0 on
 * failure. */
int zt_sample_load_wav_io(SDL_IOStream *io, int closeio, const char *name_hint,
                          zt_sample *out);

#endif /* _SAMPLE_LOAD_H */
/* eof */
