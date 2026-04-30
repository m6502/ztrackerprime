// Unit tests for src/ccizer.{h,cpp} -- CCizer parser, sidecar, MIDI byte
// builder, folder resolution. SDL-free.

#include "ccizer.h"
#include "ccizer.cpp"  // pull translation unit in directly so we don't fight CMake

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  include <io.h>
#  define unlink _unlink
#  define rmdir  _rmdir
static int test_mkdir(const char *p) { return _mkdir(p); }
#else
#  include <unistd.h>
static int test_mkdir(const char *p) { return mkdir(p, 0755); }
#endif

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

// ----------------------------------------------------------------------------
static void write_tmp(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (!fp) { perror("fopen"); exit(1); }
    fputs(content, fp);
    fclose(fp);
}

static void test_parser_basic() {
    const char *p = "/tmp/zt_ccizer_basic.txt";
    write_tmp(p,
              "PB Pitchbend\n"
              "1 Mod\n"
              "7 Volume\n"
              "10 Pan\n"
              "74 Cutoff\n");
    ZtCcizerFile f;
    CHECK(zt_ccizer_load(p, &f) == 0);
    CHECK(f.num_slots == 5);
    CHECK(f.slots[0].cc == ZT_CCIZER_PB_MARKER);
    CHECK_STR(f.slots[0].name, "Pitchbend");
    CHECK(f.slots[0].value == 8192);                      // PB centers at 8192
    CHECK(f.slots[1].cc == 1);
    CHECK_STR(f.slots[1].name, "Mod");
    CHECK(f.slots[4].cc == 74);
    CHECK_STR(f.slots[4].name, "Cutoff");
    CHECK_STR(f.basename, "zt_ccizer_basic.txt");
    unlink(p);
}

static void test_parser_comments_and_blanks() {
    const char *p = "/tmp/zt_ccizer_comments.txt";
    write_tmp(p,
              "# header comment\n"
              "\n"
              "1 Mod\n"
              "  \n"
              "# another comment\n"
              "7 Volume\n");
    ZtCcizerFile f;
    CHECK(zt_ccizer_load(p, &f) == 0);
    CHECK(f.num_slots == 2);
    CHECK(f.slots[0].cc == 1);
    CHECK(f.slots[1].cc == 7);
    unlink(p);
}

static void test_parser_rejects_out_of_range() {
    const char *p = "/tmp/zt_ccizer_oor.txt";
    write_tmp(p,
              "200 TooBig\n"
              "-1 Negative\n"
              "garbage line\n"
              "5 OK\n");
    ZtCcizerFile f;
    CHECK(zt_ccizer_load(p, &f) == 0);
    CHECK(f.num_slots == 1);
    CHECK(f.slots[0].cc == 5);
    unlink(p);
}

static void test_parser_pb_case_insensitive() {
    const char *p = "/tmp/zt_ccizer_pbcase.txt";
    write_tmp(p, "pb pitchbend lower\nPB pitchbend upper\n");
    ZtCcizerFile f;
    CHECK(zt_ccizer_load(p, &f) == 0);
    CHECK(f.num_slots == 2);
    CHECK(f.slots[0].cc == ZT_CCIZER_PB_MARKER);
    CHECK(f.slots[1].cc == ZT_CCIZER_PB_MARKER);
    unlink(p);
}

static void test_view_sidecar_roundtrip() {
    const char *p = "/tmp/zt_ccizer_view.txt";
    const char *side = "/tmp/zt_ccizer_view.cc-view";
    write_tmp(p, "1 Mod\n7 Volume\n74 Cutoff\n");

    ZtCcizerFile f;
    CHECK(zt_ccizer_load(p, &f) == 0);
    CHECK(f.slots[0].view == ZT_CCIZER_VIEW_SLIDER);     // default
    CHECK(f.slots[1].view == ZT_CCIZER_VIEW_SLIDER);
    CHECK(f.slots[2].view == ZT_CCIZER_VIEW_SLIDER);

    f.slots[0].view = ZT_CCIZER_VIEW_KNOB;
    f.slots[2].view = ZT_CCIZER_VIEW_KNOB;
    CHECK(zt_ccizer_save_view_sidecar(&f) == 0);

    ZtCcizerFile g;
    CHECK(zt_ccizer_load(p, &g) == 0);
    CHECK(g.slots[0].view == ZT_CCIZER_VIEW_KNOB);
    CHECK(g.slots[1].view == ZT_CCIZER_VIEW_SLIDER);
    CHECK(g.slots[2].view == ZT_CCIZER_VIEW_KNOB);

    unlink(p);
    unlink(side);
}

static void test_midi_builder_cc() {
    ZtCcizerSlot s = {};
    s.cc = 74; s.value = 90;
    unsigned char st = 0, d1 = 0, d2 = 0;
    CHECK(zt_ccizer_build_midi(&s, 1, &st, &d1, &d2) == 1);
    CHECK(st == 0xB0);
    CHECK(d1 == 74);
    CHECK(d2 == 90);

    CHECK(zt_ccizer_build_midi(&s, 16, &st, &d1, &d2) == 1);
    CHECK(st == 0xBF);

    s.cc = 200;
    CHECK(zt_ccizer_build_midi(&s, 1, &st, &d1, &d2) == 0);
}

static void test_midi_builder_pb() {
    ZtCcizerSlot s = {};
    s.cc = ZT_CCIZER_PB_MARKER;
    s.value = 8192;                             // center
    unsigned char st = 0, d1 = 0, d2 = 0;
    CHECK(zt_ccizer_build_midi(&s, 1, &st, &d1, &d2) == 1);
    CHECK(st == 0xE0);
    CHECK(d1 == 0);                             // 8192 & 0x7F == 0
    CHECK(d2 == 64);                            // 8192 >> 7 == 64

    s.value = 16383;
    CHECK(zt_ccizer_build_midi(&s, 5, &st, &d1, &d2) == 1);
    CHECK(st == 0xE4);
    CHECK(d1 == 127);
    CHECK(d2 == 127);
}

static void test_resolve_folder_override() {
    char out[1024];
    char buf[1024];

    // Override that doesn't exist -> falls through.
    int r = zt_ccizer_resolve_folder("/nonexistent/nope", "", out, sizeof(out));
    CHECK(r != 0);

    // Override that exists -> picks it.
    snprintf(buf, sizeof(buf), "/tmp/zt_ccizer_resolve_dir");
    test_mkdir(buf);
    r = zt_ccizer_resolve_folder(buf, "", out, sizeof(out));
    CHECK(r == 0);
    CHECK_STR(out, buf);
    rmdir(buf);
}

static void test_list_dir() {
    const char *dir = "/tmp/zt_ccizer_listdir";
    test_mkdir(dir);
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/a.txt", dir); write_tmp(buf, "1 A\n");
    snprintf(buf, sizeof(buf), "%s/b.txt", dir); write_tmp(buf, "2 B\n");
    snprintf(buf, sizeof(buf), "%s/c.notatxt", dir); write_tmp(buf, "skip\n");

    char names[16][256];
    int n = zt_ccizer_list_dir(dir, names, 16);
    CHECK(n == 2);
    CHECK_STR(names[0], "a.txt");
    CHECK_STR(names[1], "b.txt");

    snprintf(buf, sizeof(buf), "%s/a.txt", dir); unlink(buf);
    snprintf(buf, sizeof(buf), "%s/b.txt", dir); unlink(buf);
    snprintf(buf, sizeof(buf), "%s/c.notatxt", dir); unlink(buf);
    rmdir(dir);
}

int main(void) {
    test_parser_basic();
    test_parser_comments_and_blanks();
    test_parser_rejects_out_of_range();
    test_parser_pb_case_insensitive();
    test_view_sidecar_roundtrip();
    test_midi_builder_cc();
    test_midi_builder_pb();
    test_resolve_folder_override();
    test_list_dir();
    printf("ccizer: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
