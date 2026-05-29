/*************************************************************************
 *
 * FILE  sample.cpp
 *
 * DESCRIPTION
 *   zt_sample lifecycle (see sample.h). SDL-free on purpose -- only
 *   the standard allocator and string/memory routines, so this TU can
 *   be compiled into the SDL-free unit-test harness.
 *
 ******/
#include "sample.h"

#include <stdlib.h>
#include <string.h>
#include <new>

zt_sample::zt_sample(void)
{
    pcm = NULL;
    clear();
}

zt_sample::~zt_sample(void)
{
    if (pcm) {
        delete[] pcm;
        pcm = NULL;
    }
}

void zt_sample::clear(void)
{
    if (pcm) {
        delete[] pcm;
        pcm = NULL;
    }
    name[0]     = '\0';
    frames      = 0;
    channels    = 1;
    root_note   = ZTM_SAMPLE_DEFAULT_ROOT;
    flags       = 0;
    finetune    = 0;
    default_vol = 127;
    rate        = 44100;
    loop_start  = 0;
    loop_end    = 0;
}

int zt_sample::isempty(void) const
{
    return (pcm == NULL || frames == 0) ? 1 : 0;
}

int zt_sample::set_pcm(const short int *src, unsigned int nframes, unsigned char nchannels)
{
    if (pcm) {
        delete[] pcm;
        pcm = NULL;
    }
    frames   = 0;
    channels = (nchannels == 2) ? 2 : 1;

    if (!src || nframes == 0) {
        return 1; /* explicit clear is success */
    }

    unsigned int total = nframes * (unsigned int)channels;
    short int *buf = new (std::nothrow) short int[total];
    if (!buf) {
        channels = 1;
        return 0;
    }
    memcpy(buf, src, (size_t)total * sizeof(short int));
    pcm    = buf;
    frames = nframes;
    return 1;
}

unsigned int zt_sample::byte_size(void) const
{
    if (!pcm || frames == 0) return 0;
    return frames * (unsigned int)channels * (unsigned int)sizeof(short int);
}
/* eof */
