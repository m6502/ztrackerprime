/*************************************************************************
 *
 * FILE  sample_load.cpp
 *
 * DESCRIPTION
 *   WAV loading for the sample pool (see sample_load.h). Decodes via
 *   SDL_LoadWAV / SDL_LoadWAV_IO, converts to interleaved S16 mono/
 *   stereo with SDL_ConvertAudioSamples, and copies the PCM into the
 *   zt_sample (which owns its own buffer).
 *
 ******/
#include "sample_load.h"

#include <string.h>

// Fill out->name from a path or hint: basename, extension stripped,
// truncated to the sample name field.
static void set_name_from_hint(zt_sample *out, const char *hint)
{
    out->name[0] = '\0';
    if (!hint || !hint[0]) return;

    // basename: last '/' or '\\'
    const char *base = hint;
    for (const char *p = hint; *p; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    // copy up to the field cap, stopping at the final '.' (extension)
    const char *dot = NULL;
    for (const char *p = base; *p; ++p)
        if (*p == '.') dot = p;

    unsigned int n = 0;
    for (const char *p = base; *p && (dot ? p < dot : 1); ++p) {
        if (n < ZTM_SAMPLENAME_MAXLEN - 1)
            out->name[n++] = *p;
    }
    out->name[n] = '\0';
}

// Shared tail: take an SDL-decoded buffer (spec/buf/len), convert to
// interleaved S16 mono/stereo, store in `out`. Frees `buf` via SDL_free.
static int finish_from_decoded(const SDL_AudioSpec *spec, Uint8 *buf, Uint32 len,
                               zt_sample *out)
{
    SDL_AudioSpec dst;
    dst.format   = SDL_AUDIO_S16;                       // native-endian S16
    dst.channels = (spec->channels >= 2) ? 2 : 1;       // downmix >2ch to stereo
    dst.freq     = spec->freq;

    Uint8 *conv     = NULL;
    int    conv_len = 0;
    bool   ok = SDL_ConvertAudioSamples(spec, buf, (int)len, &dst, &conv, &conv_len);
    SDL_free(buf);
    if (!ok || !conv || conv_len <= 0) {
        if (conv) SDL_free(conv);
        return 0;
    }

    unsigned int bytes_per_frame = (unsigned int)dst.channels * (unsigned int)sizeof(short int);
    unsigned int frames = (bytes_per_frame > 0) ? ((unsigned int)conv_len / bytes_per_frame) : 0;

    int rc = out->set_pcm((const short int *)conv, frames, (unsigned char)dst.channels);
    SDL_free(conv);
    if (!rc || out->isempty())
        return 0;

    out->rate      = (unsigned int)(spec->freq > 0 ? spec->freq : 44100);
    out->root_note = ZTM_SAMPLE_DEFAULT_ROOT;
    return 1;
}

int zt_sample_load_wav_io(SDL_IOStream *io, int closeio, const char *name_hint,
                          zt_sample *out)
{
    if (!out) {
        if (io && closeio) SDL_CloseIO(io);
        return 0;
    }
    out->clear();
    if (!io) return 0;

    SDL_AudioSpec spec;
    Uint8 *buf = NULL;
    Uint32 len = 0;
    bool ok = SDL_LoadWAV_IO(io, closeio ? true : false, &spec, &buf, &len);
    if (!ok || !buf) {
        if (buf) SDL_free(buf);
        return 0;
    }

    set_name_from_hint(out, name_hint);
    int rc = finish_from_decoded(&spec, buf, len, out);
    if (!rc) out->clear();
    return rc;
}

int zt_sample_load_wav(const char *path, zt_sample *out)
{
    if (!out) return 0;
    out->clear();
    if (!path || !path[0]) return 0;

    SDL_AudioSpec spec;
    Uint8 *buf = NULL;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path, &spec, &buf, &len) || !buf) {
        if (buf) SDL_free(buf);
        return 0;
    }

    set_name_from_hint(out, path);
    int rc = finish_from_decoded(&spec, buf, len, out);
    if (!rc) out->clear();
    return rc;
}
/* eof */
