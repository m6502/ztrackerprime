// CCBN chunk roundtrip test (audit P18).
//
// The CCBN chunk persists per-instrument CCizer bank filenames in the
// .zt save format. Format (from src/ztfile-io.cpp):
//   short int  count
//   repeat count times:
//     unsigned char  inst_idx
//     short int      length
//     bytes          path  (length bytes, no null terminator)
//
// The audit (P18) flagged the lack of any test that round-trips this.
// Truncation, oversized-length silently clamping, and corrupt count
// values were the bug classes to catch.
//
// SDL-free. Uses a tiny in-test buffer instead of CDataBuf so the test
// binary doesn't need to link against the SDL stack.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int checks   = 0;

#define CHECK(expr) do {                                                \
    checks++;                                                           \
    if (!(expr)) { failures++;                                          \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
    }                                                                   \
} while(0)

#define CHECK_STR(actual, expected) do {                                \
    checks++;                                                           \
    if (strcmp((actual), (expected)) != 0) { failures++;                \
        fprintf(stderr, "FAIL %s:%d  expected '%s' got '%s'\n",         \
                __FILE__, __LINE__, (expected), (actual));              \
    }                                                                   \
} while(0)

// Minimal stand-in for CDataBuf's write / get* methods. Mirrors the
// real impl byte-for-byte so the test catches any deviation in the
// CCBN reader/writer. (CDataBuf.cpp pulls in SDL.h, hence avoiding
// it here.)
struct Buf {
    char  data[65536];
    int   size;
    int   read_cursor;
    Buf() : size(0), read_cursor(0) {}
    void write(const char *p, int n) { memcpy(data + size, p, n); size += n; }
    void reset_read() { read_cursor = 0; }
    char getch() {
        if (read_cursor >= size) return 0;
        return data[read_cursor++];
    }
    unsigned char getuch() { return (unsigned char)getch(); }
    short int getsi() {
        if (read_cursor + (int)sizeof(short int) > size) return 0;
        short int v;
        memcpy(&v, data + read_cursor, sizeof(short int));
        read_cursor += sizeof(short int);
        return v;
    }
};

#define MAX_INSTS_FOR_TEST 100
#define BANK_BUF_LEN       256

// Mirrors the writer in src/ztfile-io.cpp's `if (count > 0) { ... CCBN }` block.
static void write_ccbn(Buf *buf,
                       const int   *inst_idxs,
                       const char *const *paths,
                       int          count) {
    short int c = (short int)count;
    buf->write((const char *)&c, sizeof(short int));
    for (int i = 0; i < count; i++) {
        unsigned char idx = (unsigned char)inst_idxs[i];
        short int    len  = (short int)strlen(paths[i]);
        buf->write((const char *)&idx, sizeof(unsigned char));
        buf->write((const char *)&len, sizeof(short int));
        buf->write(paths[i], len);
    }
}

struct LoadedEntry {
    unsigned char idx;
    char path[BANK_BUF_LEN];
};

// Mirrors the loader in src/ztfile-io.cpp's `if (cmp_hd(&header[0], "CCBN"))` block.
static int read_ccbn(Buf *buf,
                     LoadedEntry *out,
                     int max_out,
                     int *truncated) {
    if (truncated) *truncated = 0;
    short int count = buf->getsi();
    if (count < 0 || count > max_out || count > MAX_INSTS_FOR_TEST) {
        if (truncated) *truncated = 1;
        return 0;
    }
    int n = 0;
    for (short int k = 0; k < count; k++) {
        unsigned char inst_idx = buf->getuch();
        short int     len      = buf->getsi();
        if (len < 0) {
            if (truncated) *truncated = 1;
            len = 0;
        }
        if ((size_t)len >= sizeof(out[0].path)) {
            if (truncated) *truncated = 1;
            len = (short int)(sizeof(out[0].path) - 1);
        }
        char tmp[sizeof(out[0].path)];
        for (short int b = 0; b < len; b++) tmp[b] = buf->getch();
        tmp[len] = '\0';
        out[n].idx = inst_idx;
        snprintf(out[n].path, sizeof(out[n].path), "%s", tmp);
        n++;
    }
    return n;
}

static void test_basic_roundtrip() {
    Buf buf;
    int   idxs[]  = {0, 5, 99};
    const char *paths[] = {"sc88st.txt", "microfreak.txt", "Prophet6.txt"};
    write_ccbn(&buf, idxs, paths, 3);

    buf.reset_read();
    LoadedEntry out[100];
    int trunc = 0;
    int n = read_ccbn(&buf, out, 100, &trunc);
    CHECK(n == 3);
    CHECK(trunc == 0);
    CHECK(out[0].idx == 0);
    CHECK_STR(out[0].path, "sc88st.txt");
    CHECK(out[1].idx == 5);
    CHECK_STR(out[1].path, "microfreak.txt");
    CHECK(out[2].idx == 99);
    CHECK_STR(out[2].path, "Prophet6.txt");
}

static void test_empty_chunk() {
    Buf buf;
    write_ccbn(&buf, NULL, NULL, 0);
    buf.reset_read();
    LoadedEntry out[100];
    int trunc = 0;
    int n = read_ccbn(&buf, out, 100, &trunc);
    CHECK(n == 0);
    CHECK(trunc == 0);
}

static void test_long_path_clamped() {
    Buf buf;
    char longp[300];
    memset(longp, 'A', sizeof(longp));
    longp[299] = '\0';
    int  idxs[] = {7};
    const char *paths[] = {longp};
    write_ccbn(&buf, idxs, paths, 1);

    buf.reset_read();
    LoadedEntry out[100];
    int trunc = 0;
    int n = read_ccbn(&buf, out, 100, &trunc);
    CHECK(n == 1);
    CHECK(trunc == 1);                                     // length > buffer cap
    CHECK(strlen(out[0].path) == sizeof(out[0].path) - 1);
    CHECK(out[0].path[0] == 'A');
}

static void test_corrupt_count() {
    Buf buf;
    short int bogus = 9999;
    buf.write((const char *)&bogus, sizeof(short int));
    buf.reset_read();
    LoadedEntry out[100];
    int trunc = 0;
    int n = read_ccbn(&buf, out, 100, &trunc);
    CHECK(n == 0);
    CHECK(trunc == 1);
}

static void test_unicode_path() {
    Buf buf;
    int  idxs[] = {3};
    const char *paths[] = {"my\xC3\xB6synth.txt"};   // m-y-ö-s-y-n-t-h
    write_ccbn(&buf, idxs, paths, 1);
    buf.reset_read();
    LoadedEntry out[100];
    int trunc = 0;
    int n = read_ccbn(&buf, out, 100, &trunc);
    CHECK(n == 1);
    CHECK(trunc == 0);
    CHECK_STR(out[0].path, "my\xC3\xB6synth.txt");
}

static void test_max_instruments() {
    // Saturate the chunk with one entry per allowed instrument index.
    Buf buf;
    int idxs[100];
    char paths_storage[100][32];
    const char *paths[100];
    for (int i = 0; i < 100; i++) {
        idxs[i] = i;
        snprintf(paths_storage[i], sizeof(paths_storage[i]), "bank_%02d.txt", i);
        paths[i] = paths_storage[i];
    }
    write_ccbn(&buf, idxs, paths, 100);

    buf.reset_read();
    LoadedEntry out[100];
    int trunc = 0;
    int n = read_ccbn(&buf, out, 100, &trunc);
    CHECK(n == 100);
    CHECK(trunc == 0);
    CHECK_STR(out[0].path, "bank_00.txt");
    CHECK_STR(out[99].path, "bank_99.txt");
}

int main(void) {
    test_basic_roundtrip();
    test_empty_chunk();
    test_long_path_clamped();
    test_corrupt_count();
    test_unicode_path();
    test_max_instruments();
    printf("ccbn_roundtrip: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
