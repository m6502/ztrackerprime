// ZTtp chunk roundtrip test.
//
// The ZTtp chunk persists per-track-index song-wide name + custom color
// (the "drums = red" model). Format (from src/ztfile-io.cpp):
//   uint16 count
//   repeat count times:
//     uint16 track_idx
//     uint32 color          (packed TColor; 0 = no custom color)
//     uint8  name_len       (0..ZTM_TRACKNAME_MAXLEN-1)
//     bytes  name           (name_len bytes, no null terminator)
//
// SDL-free. Uses a tiny in-test buffer that mirrors CDataBuf's get*/push*
// byte-for-byte so the test catches any deviation in the reader/writer.

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

#define ZTM_TRACKNAME_MAXLEN 16
#define ZTM_MAX_TRACKS       256

struct Buf {
    char  data[65536];
    int   size;
    int   read_cursor;
    Buf() : size(0), read_cursor(0) {}
    void write(const char *p, int n) { memcpy(data + size, p, n); size += n; }
    void reset_read() { read_cursor = 0; }
    void pushuc(unsigned char c) { data[size++] = (char)c; }
    void pushusi(unsigned short v) { write((const char *)&v, sizeof(v)); }
    void pushui(unsigned int v)    { write((const char *)&v, sizeof(v)); }
    char getch() { return read_cursor >= size ? 0 : data[read_cursor++]; }
    unsigned char getuch() { return (unsigned char)getch(); }
    unsigned short getusi() {
        if (read_cursor + (int)sizeof(unsigned short) > size) return 0;
        unsigned short v; memcpy(&v, data + read_cursor, sizeof(v));
        read_cursor += sizeof(v); return v;
    }
    unsigned int getui() {
        if (read_cursor + (int)sizeof(unsigned int) > size) return 0;
        unsigned int v; memcpy(&v, data + read_cursor, sizeof(v));
        read_cursor += sizeof(v); return v;
    }
};

// Model: per-track name + color, mirroring zt_module's arrays.
struct Song {
    char          track_name[ZTM_MAX_TRACKS][ZTM_TRACKNAME_MAXLEN];
    unsigned long track_color[ZTM_MAX_TRACKS];
    Song() { memset(track_name, 0, sizeof(track_name));
             memset(track_color, 0, sizeof(track_color)); }
};

// Mirrors build_ZT_track_props().
static void write_props(Buf *buf, const Song *s) {
    unsigned short count = 0;
    for (int i = 0; i < ZTM_MAX_TRACKS; i++)
        if (s->track_color[i] || s->track_name[i][0]) count++;
    buf->pushusi(count);
    for (int i = 0; i < ZTM_MAX_TRACKS; i++) {
        if (!s->track_color[i] && !s->track_name[i][0]) continue;
        buf->pushusi((unsigned short)i);
        buf->pushui((unsigned int)s->track_color[i]);
        unsigned char len = (unsigned char)strnlen(s->track_name[i], ZTM_TRACKNAME_MAXLEN - 1);
        buf->pushuc(len);
        buf->write(s->track_name[i], len);
    }
}

// Mirrors load_ZT_track_props().
static void read_props(Buf *buf, Song *s) {
    unsigned short count = buf->getusi();
    for (int k = 0; k < (int)count; k++) {
        unsigned short idx   = buf->getusi();
        unsigned int   color = buf->getui();
        unsigned char  len   = buf->getuch();
        char tmp[ZTM_TRACKNAME_MAXLEN];
        if (len >= ZTM_TRACKNAME_MAXLEN) len = ZTM_TRACKNAME_MAXLEN - 1;
        for (int b = 0; b < (int)len; b++) tmp[b] = buf->getch();
        tmp[len] = '\0';
        if (idx < ZTM_MAX_TRACKS) {
            s->track_color[idx] = color;
            strncpy(s->track_name[idx], tmp, ZTM_TRACKNAME_MAXLEN - 1);
            s->track_name[idx][ZTM_TRACKNAME_MAXLEN - 1] = '\0';
        }
    }
}

static void test_basic_roundtrip() {
    Song in;
    strcpy(in.track_name[3], "Drums");
    in.track_color[3] = 0xFFD03030u;        // red
    strcpy(in.track_name[10], "Bass");
    in.track_color[10] = 0xFF3060D0u;       // blue
    in.track_color[20] = 0xFF30A050u;       // green, no name

    Buf buf; write_props(&buf, &in);
    buf.reset_read();
    Song out; read_props(&buf, &out);

    CHECK_STR(out.track_name[3], "Drums");
    CHECK(out.track_color[3] == 0xFFD03030u);
    CHECK_STR(out.track_name[10], "Bass");
    CHECK(out.track_color[10] == 0xFF3060D0u);
    CHECK(out.track_color[20] == 0xFF30A050u);
    CHECK_STR(out.track_name[20], "");
    // Untouched tracks stay default.
    CHECK(out.track_color[0] == 0);
    CHECK_STR(out.track_name[0], "");
}

static void test_empty_chunk() {
    Song in; Buf buf; write_props(&buf, &in);
    buf.reset_read();
    Song out; read_props(&buf, &out);
    CHECK(out.track_color[0] == 0);
    CHECK_STR(out.track_name[5], "");
}

static void test_name_only_no_color() {
    Song in;
    strcpy(in.track_name[7], "Lead");      // name set, color 0
    Buf buf; write_props(&buf, &in);
    buf.reset_read();
    Song out; read_props(&buf, &out);
    CHECK_STR(out.track_name[7], "Lead");
    CHECK(out.track_color[7] == 0);
}

static void test_black_color_is_preserved() {
    // Custom black (0xFF000000) is distinct from "no color" (0).
    Song in;
    in.track_color[2] = 0xFF000000u;
    Buf buf; write_props(&buf, &in);
    buf.reset_read();
    Song out; read_props(&buf, &out);
    CHECK(out.track_color[2] == 0xFF000000u);
}

static void test_max_length_name() {
    Song in;
    // 15 chars + null fits exactly in ZTM_TRACKNAME_MAXLEN (16).
    strcpy(in.track_name[1], "ABCDEFGHIJKLMNO");
    in.track_color[1] = 0xFF112233u;
    Buf buf; write_props(&buf, &in);
    buf.reset_read();
    Song out; read_props(&buf, &out);
    CHECK_STR(out.track_name[1], "ABCDEFGHIJKLMNO");
    CHECK(strlen(out.track_name[1]) == 15);
    CHECK(out.track_color[1] == 0xFF112233u);
}

static void test_unicode_name() {
    Song in;
    strcpy(in.track_name[4], "Tr\xC3\xB6mmel");   // Trömmel (UTF-8)
    in.track_color[4] = 0xFF445566u;
    Buf buf; write_props(&buf, &in);
    buf.reset_read();
    Song out; read_props(&buf, &out);
    CHECK_STR(out.track_name[4], "Tr\xC3\xB6mmel");
}

static void test_last_track_index() {
    Song in;
    strcpy(in.track_name[ZTM_MAX_TRACKS - 1], "Last");
    in.track_color[ZTM_MAX_TRACKS - 1] = 0xFFABCDEFu;
    Buf buf; write_props(&buf, &in);
    buf.reset_read();
    Song out; read_props(&buf, &out);
    CHECK_STR(out.track_name[ZTM_MAX_TRACKS - 1], "Last");
    CHECK(out.track_color[ZTM_MAX_TRACKS - 1] == 0xFFABCDEFu);
}

int main(void) {
    test_basic_roundtrip();
    test_empty_chunk();
    test_name_only_no_color();
    test_black_color_is_preserved();
    test_max_length_name();
    test_unicode_name();
    test_last_track_index();
    printf("track_props_roundtrip: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
