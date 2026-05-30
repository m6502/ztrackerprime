// WAV loader test (PR A: sample foundation).
//
// Exercises the real src/sample_load.cpp against in-memory WAV files
// built by the test (no temp file, no committed fixture): a canonical
// PCM RIFF/WAVE blob is handed to the loader via SDL_IOFromMem +
// zt_sample_load_wav_io.
//
// Unlike the other suites this one DOES link SDL3, because SDL_LoadWAV /
// SDL_ConvertAudioSamples are the loader's whole job. CI's Linux job has
// SDL3 available.

#include <stdio.h>
#include <string.h>

#include <SDL.h>
#include "sample.h"
#include "sample_load.h"

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

// Build a canonical little-endian PCM WAV into `out`. Returns total bytes.
static int build_wav(unsigned char *out, int channels, int rate, int bits,
                     const void *pcm, int pcm_bytes) {
    unsigned char *p = out;
    auto w32  = [&](unsigned int v){ p[0]=(unsigned char)v; p[1]=(unsigned char)(v>>8);
                                     p[2]=(unsigned char)(v>>16); p[3]=(unsigned char)(v>>24); p+=4; };
    auto w16  = [&](unsigned short v){ p[0]=(unsigned char)v; p[1]=(unsigned char)(v>>8); p+=2; };
    auto wtag = [&](const char *t){ memcpy(p, t, 4); p+=4; };

    int byte_rate   = rate * channels * (bits/8);
    int block_align = channels * (bits/8);

    wtag("RIFF"); w32((unsigned int)(4 + (8+16) + (8+pcm_bytes))); wtag("WAVE");
    wtag("fmt "); w32(16); w16(1); w16((unsigned short)channels);
    w32((unsigned int)rate); w32((unsigned int)byte_rate);
    w16((unsigned short)block_align); w16((unsigned short)bits);
    wtag("data"); w32((unsigned int)pcm_bytes);
    memcpy(p, pcm, pcm_bytes); p += pcm_bytes;
    return (int)(p - out);
}

static void test_16bit_mono() {
    short int pcm[8] = { 0, 1000, -1000, 32767, -32768, 5, 6, 7 };
    unsigned char wav[256];
    int sz = build_wav(wav, 1, 44100, 16, pcm, (int)sizeof(pcm));

    SDL_IOStream *io = SDL_IOFromMem(wav, (size_t)sz);
    CHECK(io != NULL);
    zt_sample s;
    int rc = zt_sample_load_wav_io(io, 1, "kick.wav", &s);
    CHECK(rc == 1);
    CHECK(s.channels == 1);
    CHECK(s.rate == 44100);
    CHECK(s.frames == 8);
    CHECK(!s.isempty());
    CHECK(memcmp(s.pcm, pcm, sizeof(pcm)) == 0);     // S16 in -> S16 out, exact
    CHECK(strcmp(s.name, "kick") == 0);              // basename, extension stripped
}

static void test_16bit_stereo() {
    short int pcm[8] = { 100, -100, 200, -200, 300, -300, 400, -400 };  // 4 frames * 2ch
    unsigned char wav[256];
    int sz = build_wav(wav, 2, 48000, 16, pcm, (int)sizeof(pcm));

    SDL_IOStream *io = SDL_IOFromMem(wav, (size_t)sz);
    zt_sample s;
    int rc = zt_sample_load_wav_io(io, 1, "pad.wav", &s);
    CHECK(rc == 1);
    CHECK(s.channels == 2);
    CHECK(s.rate == 48000);
    CHECK(s.frames == 4);
    CHECK(memcmp(s.pcm, pcm, sizeof(pcm)) == 0);
}

static void test_8bit_converts_to_s16() {
    // 8-bit WAV PCM is unsigned (128 = zero). Exact converted values are
    // SDL's business; we only assert structure + non-empty + conversion ran.
    unsigned char pcm[6] = { 128, 200, 64, 128, 255, 0 };
    unsigned char wav[256];
    int sz = build_wav(wav, 1, 22050, 8, pcm, (int)sizeof(pcm));

    SDL_IOStream *io = SDL_IOFromMem(wav, (size_t)sz);
    zt_sample s;
    int rc = zt_sample_load_wav_io(io, 1, "noise", &s);
    CHECK(rc == 1);
    CHECK(s.channels == 1);
    CHECK(s.rate == 22050);
    CHECK(s.frames == 6);
    CHECK(!s.isempty());
    CHECK(s.byte_size() == 6 * sizeof(short int));   // widened to S16
}

static void test_garbage_rejected() {
    unsigned char junk[64];
    memset(junk, 0xAB, sizeof(junk));
    SDL_IOStream *io = SDL_IOFromMem(junk, sizeof(junk));
    zt_sample s;
    int rc = zt_sample_load_wav_io(io, 1, "bad.wav", &s);
    CHECK(rc == 0);
    CHECK(s.isempty());
}

static void test_null_io() {
    zt_sample s;
    int rc = zt_sample_load_wav_io(NULL, 0, "x", &s);
    CHECK(rc == 0);
    CHECK(s.isempty());
}

int main(void) {
    test_16bit_mono();
    test_16bit_stereo();
    test_8bit_converts_to_s16();
    test_garbage_rejected();
    test_null_io();
    printf("sample_load: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
