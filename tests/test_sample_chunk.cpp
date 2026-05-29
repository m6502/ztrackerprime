// SMPL / SMIX chunk round-trip test (PR A: sample foundation).
//
// The SMPL chunk persists one PCM sample from the song's sample pool;
// the SMIX chunk persists per-instrument links into that pool. Both are
// optional, additive .zt chunks (old zTracker skips them via readblock's
// advance-past behavior).
//
// SMPL layout (from src/ztfile-io.cpp build_sample/load_sample):
//   uint16 pool_index
//   uint16 name_len
//   bytes  name        (name_len)
//   uint8  channels    (1 or 2)
//   uint8  root_note
//   uint8  flags
//   int8   finetune
//   uint8  default_vol
//   uint32 rate
//   uint32 loop_start
//   uint32 loop_end
//   uint32 frames
//   int16  pcm[frames * channels]   (interleaved)
//
// SMIX layout:
//   int16 count
//   repeat: uint8 inst_idx, int16 sample_index
//
// SDL-free. Compiles the real src/sample.cpp (zt_sample is SDL-free) and
// uses a tiny in-test Buf that mirrors CDataBuf's push/get methods
// byte-for-byte, exactly as tests/test_ccbn_roundtrip.cpp does for CCBN.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sample.h"

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

// ---- Minimal CDataBuf stand-in (mirrors the real push/get) -------------
struct Buf {
    char data[1 << 20];   // 1 MB is plenty for the tiny synthetic samples here
    int  size;
    int  read_cursor;
    Buf() : size(0), read_cursor(0) {}

    void write(const char *p, int n) { memcpy(data + size, p, n); size += n; }
    void pushuc(unsigned char c)       { write((const char *)&c, 1); }
    void pushc(char c)                 { write(&c, 1); }
    void pushsi(short int v)           { write((const char *)&v, sizeof(short int)); }
    void pushusi(unsigned short int v) { write((const char *)&v, sizeof(unsigned short int)); }
    void pushui(unsigned int v)        { write((const char *)&v, sizeof(unsigned int)); }

    void reset_read() { read_cursor = 0; }
    int  getsize() const { return size; }

    char getch() {
        if (read_cursor >= size) return 0;
        return data[read_cursor++];
    }
    unsigned char getuch() { return (unsigned char)getch(); }
    short int getsi() {
        if (read_cursor + (int)sizeof(short int) > size) return 0;
        short int v; memcpy(&v, data + read_cursor, sizeof(short int));
        read_cursor += sizeof(short int); return v;
    }
    unsigned short int getusi() {
        if (read_cursor + (int)sizeof(unsigned short int) > size) return 0;
        unsigned short int v; memcpy(&v, data + read_cursor, sizeof(unsigned short int));
        read_cursor += sizeof(unsigned short int); return v;
    }
    unsigned int getui() {
        if (read_cursor + (int)sizeof(unsigned int) > size) return 0;
        unsigned int v; memcpy(&v, data + read_cursor, sizeof(unsigned int));
        read_cursor += sizeof(unsigned int); return v;
    }
};

// ---- Mirror of build_sample() ------------------------------------------
static void write_smpl(Buf *buf, const zt_sample *s, int idx) {
    unsigned short len = (unsigned short)strlen(s->name);
    if (len >= ZTM_SAMPLENAME_MAXLEN) len = ZTM_SAMPLENAME_MAXLEN - 1;
    buf->pushusi((unsigned short)idx);
    buf->pushusi(len);
    buf->write(s->name, len);
    buf->pushuc(s->channels);
    buf->pushuc(s->root_note);
    buf->pushuc(s->flags);
    buf->pushc((char)s->finetune);
    buf->pushuc(s->default_vol);
    buf->pushui(s->rate);
    buf->pushui(s->loop_start);
    buf->pushui(s->loop_end);
    buf->pushui(s->frames);
    if (s->pcm && s->frames)
        buf->write((const char *)s->pcm,
                   (int)(s->frames * (unsigned int)s->channels * sizeof(short int)));
}

// ---- Mirror of load_sample() -------------------------------------------
// Returns the parsed pool index, fills *out, sets *truncated.
static int read_smpl(Buf *buf, zt_sample *out, int *truncated) {
    int payload = buf->getsize();
    int trunc   = 0;

    unsigned short idx     = buf->getusi();
    unsigned short namelen = buf->getusi();

    char name[ZTM_SAMPLENAME_MAXLEN];
    unsigned int nstore = 0;
    for (unsigned short b = 0; b < namelen; b++) {
        char c = buf->getch();
        if (nstore < ZTM_SAMPLENAME_MAXLEN - 1) name[nstore++] = c;
    }
    name[nstore] = '\0';

    unsigned char  channels    = buf->getuch();
    unsigned char  root_note   = buf->getuch();
    unsigned char  flags       = buf->getuch();
    signed char    finetune    = (signed char)buf->getch();
    unsigned char  default_vol = buf->getuch();
    unsigned int   rate        = buf->getui();
    unsigned int   loop_start  = buf->getui();
    unsigned int   loop_end    = buf->getui();
    unsigned int   frames      = buf->getui();

    if (channels != 1 && channels != 2) { channels = 1; trunc = 1; }

    int consumed  = 2 + 2 + (int)namelen + 5 + 16;
    int remaining = payload - consumed;
    if (remaining < 0) remaining = 0;
    unsigned int max_frames =
        (unsigned int)remaining / ((unsigned int)channels * (unsigned int)sizeof(short int));
    if (frames > max_frames) { frames = max_frames; trunc = 1; }

    out->clear();
    if (idx >= ZTM_MAX_SAMPLES) { if (truncated) *truncated = trunc; return -1; }

    unsigned int total = frames * (unsigned int)channels;
    short int *pcm = NULL;
    if (total) {
        pcm = new short int[total];
        for (unsigned int i = 0; i < total; i++) pcm[i] = buf->getsi();
    }
    snprintf(out->name, sizeof(out->name), "%s", name);
    out->channels    = channels;
    out->root_note   = root_note;
    out->flags       = flags;
    out->finetune    = finetune;
    out->default_vol = default_vol;
    out->rate        = rate ? rate : 44100;
    out->loop_start  = loop_start;
    out->loop_end    = loop_end;
    if (pcm) { out->set_pcm(pcm, frames, channels); delete[] pcm; }

    if (truncated) *truncated = trunc;
    return (int)idx;
}

// ========================================================================
// zt_sample data-model tests (real src/sample.cpp)
// ========================================================================
static void test_sample_model() {
    zt_sample s;
    CHECK(s.isempty());
    CHECK(s.byte_size() == 0);
    CHECK(s.channels == 1);
    CHECK(s.root_note == ZTM_SAMPLE_DEFAULT_ROOT);

    short int pcm[8] = { 0, 100, -100, 32767, -32768, 1, 2, 3 };
    CHECK(s.set_pcm(pcm, 8, 1) == 1);
    CHECK(!s.isempty());
    CHECK(s.frames == 8);
    CHECK(s.channels == 1);
    CHECK(s.byte_size() == 8 * 1 * sizeof(short int));
    CHECK(s.pcm != pcm);                         // set_pcm copies
    CHECK(memcmp(s.pcm, pcm, sizeof(pcm)) == 0);

    // stereo: 4 frames * 2 ch = 8 values
    CHECK(s.set_pcm(pcm, 4, 2) == 1);
    CHECK(s.frames == 4);
    CHECK(s.channels == 2);
    CHECK(s.byte_size() == 4 * 2 * sizeof(short int));

    // clearing
    s.clear();
    CHECK(s.isempty());
    CHECK(s.pcm == NULL);

    // set_pcm(NULL) is an explicit clear, counts as success
    CHECK(s.set_pcm(NULL, 0, 1) == 1);
    CHECK(s.isempty());
}

// ========================================================================
// SMPL round-trip tests
// ========================================================================
static void fill_sample(zt_sample *s, const char *name, unsigned char ch,
                        unsigned int frames) {
    s->clear();
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->root_note   = 64;
    s->flags       = ZTM_SMPF_LOOP;
    s->finetune    = -7;
    s->default_vol = 100;
    s->rate        = 48000;
    s->loop_start  = 10;
    s->loop_end    = 20;
    unsigned int total = frames * ch;
    short int *pcm = new short int[total];
    for (unsigned int i = 0; i < total; i++) pcm[i] = (short int)(i * 37 - 5000);
    s->set_pcm(pcm, frames, ch);
    delete[] pcm;
}

static void test_smpl_roundtrip_mono() {
    zt_sample in;  fill_sample(&in, "kick.wav", 1, 64);
    Buf buf;       write_smpl(&buf, &in, 7);
    buf.reset_read();
    zt_sample out; int trunc = 0;
    int idx = read_smpl(&buf, &out, &trunc);

    CHECK(idx == 7);
    CHECK(trunc == 0);
    CHECK(strcmp(out.name, "kick.wav") == 0);
    CHECK(out.channels == 1);
    CHECK(out.root_note == 64);
    CHECK(out.flags == ZTM_SMPF_LOOP);
    CHECK(out.finetune == -7);
    CHECK(out.default_vol == 100);
    CHECK(out.rate == 48000);
    CHECK(out.loop_start == 10);
    CHECK(out.loop_end == 20);
    CHECK(out.frames == 64);
    CHECK(out.byte_size() == in.byte_size());
    CHECK(memcmp(out.pcm, in.pcm, in.byte_size()) == 0);
}

static void test_smpl_roundtrip_stereo() {
    zt_sample in;  fill_sample(&in, "loop_st", 2, 128);
    Buf buf;       write_smpl(&buf, &in, 0);
    buf.reset_read();
    zt_sample out; int trunc = 0;
    int idx = read_smpl(&buf, &out, &trunc);

    CHECK(idx == 0);
    CHECK(trunc == 0);
    CHECK(out.channels == 2);
    CHECK(out.frames == 128);
    CHECK(out.byte_size() == in.byte_size());
    CHECK(memcmp(out.pcm, in.pcm, in.byte_size()) == 0);
}

static void test_smpl_empty_name() {
    zt_sample in;  fill_sample(&in, "", 1, 4);
    Buf buf;       write_smpl(&buf, &in, 3);
    buf.reset_read();
    zt_sample out; int trunc = 0;
    int idx = read_smpl(&buf, &out, &trunc);
    CHECK(idx == 3);
    CHECK(out.name[0] == '\0');
    CHECK(out.frames == 4);
}

static void test_smpl_bad_pool_index() {
    zt_sample in;  fill_sample(&in, "x", 1, 4);
    Buf buf;       write_smpl(&buf, &in, ZTM_MAX_SAMPLES + 5);
    buf.reset_read();
    zt_sample out; int trunc = 0;
    int idx = read_smpl(&buf, &out, &trunc);
    CHECK(idx == -1);                  // out-of-range index dropped
    CHECK(out.isempty());
}

static void test_smpl_truncated_frames() {
    // Hand-build a SMPL claiming 1000 frames but with no PCM payload.
    Buf buf;
    buf.pushusi(2);     // idx
    buf.pushusi(0);     // name_len
    buf.pushuc(1);      // channels
    buf.pushuc(60);     // root
    buf.pushuc(0);      // flags
    buf.pushc(0);       // finetune
    buf.pushuc(127);    // vol
    buf.pushui(44100);  // rate
    buf.pushui(0);      // loop_start
    buf.pushui(0);      // loop_end
    buf.pushui(1000);   // frames -- a lie; no PCM follows
    buf.reset_read();

    zt_sample out; int trunc = 0;
    int idx = read_smpl(&buf, &out, &trunc);
    CHECK(idx == 2);
    CHECK(trunc == 1);                 // frames clamped against payload
    CHECK(out.frames == 0);            // no PCM bytes available -> empty
    CHECK(out.isempty());
}

static void test_smpl_long_name_clamped() {
    zt_sample in; in.clear();
    char longn[200]; memset(longn, 'Z', sizeof(longn)); longn[199] = '\0';
    snprintf(in.name, sizeof(in.name), "%s", longn);   // already capped by field
    short int pcm[2] = { 7, 8 };
    in.set_pcm(pcm, 2, 1);
    Buf buf; write_smpl(&buf, &in, 1);
    buf.reset_read();
    zt_sample out; int trunc = 0;
    int idx = read_smpl(&buf, &out, &trunc);
    CHECK(idx == 1);
    CHECK(strlen(out.name) <= ZTM_SAMPLENAME_MAXLEN - 1);
    CHECK(out.name[0] == 'Z');
}

// ========================================================================
// SMIX round-trip
// ========================================================================
struct SmixEntry { unsigned char inst; short int sidx; };

static void write_smix(Buf *buf, const SmixEntry *e, int count) {
    buf->pushsi((short int)count);
    for (int i = 0; i < count; i++) { buf->pushuc(e[i].inst); buf->pushsi(e[i].sidx); }
}

static int read_smix(Buf *buf, SmixEntry *out, int max_out) {
    short int count = buf->getsi();
    if (count < 0 || count > 100 /* ZTM_MAX_INSTS */) return -1;
    int n = 0;
    for (short int k = 0; k < count; k++) {
        unsigned char inst = buf->getuch();
        short int sidx     = buf->getsi();
        if (n < max_out) { out[n].inst = inst; out[n].sidx = sidx; n++; }
    }
    return n;
}

static void test_smix_roundtrip() {
    SmixEntry in[3] = { {0, 5}, {12, 0}, {99, ZTM_MAX_SAMPLES - 1} };
    Buf buf; write_smix(&buf, in, 3);
    buf.reset_read();
    SmixEntry out[8]; int n = read_smix(&buf, out, 8);
    CHECK(n == 3);
    CHECK(out[0].inst == 0  && out[0].sidx == 5);
    CHECK(out[1].inst == 12 && out[1].sidx == 0);
    CHECK(out[2].inst == 99 && out[2].sidx == ZTM_MAX_SAMPLES - 1);
}

static void test_smix_corrupt_count() {
    Buf buf; short int bogus = 9999; buf.pushsi(bogus);
    buf.reset_read();
    SmixEntry out[8]; int n = read_smix(&buf, out, 8);
    CHECK(n == -1);
}

static void test_smix_empty() {
    Buf buf; write_smix(&buf, NULL, 0);
    buf.reset_read();
    SmixEntry out[8]; int n = read_smix(&buf, out, 8);
    CHECK(n == 0);
}

int main(void) {
    test_sample_model();
    test_smpl_roundtrip_mono();
    test_smpl_roundtrip_stereo();
    test_smpl_empty_name();
    test_smpl_bad_pool_index();
    test_smpl_truncated_frames();
    test_smpl_long_name_clamped();
    test_smix_roundtrip();
    test_smix_corrupt_count();
    test_smix_empty();
    printf("sample_chunk: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
