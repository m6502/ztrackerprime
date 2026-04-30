// Unit tests for src/sysex_macro.{h,cpp} -- name predicate + path
// resolution + .syx file reader. SDL-free, conf globals stubbed.
//
// We #include the .cpp directly so the test binary doesn't have to
// pull in the full conf.cpp. The conf global is provided as a stub
// in this translation unit instead.

#include "sysex_macro.h"

// Stub the ZTConf type just enough that sysex_macro.cpp can read
// `zt_config_globals.syx_folder`. The real zTracker links the full
// type via conf.h; here we provide a minimal definition that has
// only the field we need.
#define _CONF_H        // sentinel: skip conf.h's full declaration
#define MAX_PATH 260
struct ZTConf {
    char ccizer_folder[MAX_PATH + 1];
    char syx_folder[MAX_PATH + 1];
};
ZTConf zt_config_globals = {};

#include "sysex_macro.cpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  include <io.h>
#  define unlink _unlink
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

static void write_tmp_bytes(const char *path, const unsigned char *bytes, int len) {
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror("fopen"); exit(1); }
    fwrite(bytes, 1, (size_t)len, fp);
    fclose(fp);
}

static void test_predicate() {
    CHECK(zt_sysex_macro_is_file("patch.syx") == 1);
    CHECK(zt_sysex_macro_is_file("PATCH.SYX") == 1);
    CHECK(zt_sysex_macro_is_file("foo.bar.syx") == 1);
    CHECK(zt_sysex_macro_is_file(".syx") == 0);                 // need at least one char before
    CHECK(zt_sysex_macro_is_file("syx") == 0);
    CHECK(zt_sysex_macro_is_file("sysex") == 0);
    CHECK(zt_sysex_macro_is_file("regular_macro") == 0);
    CHECK(zt_sysex_macro_is_file("") == 0);
    CHECK(zt_sysex_macro_is_file(NULL) == 0);
}

static void test_resolve_path_with_override() {
    snprintf(zt_config_globals.syx_folder, sizeof(zt_config_globals.syx_folder),
             "/tmp/zt_sxm_test");
    char out[1024];
    CHECK(zt_sysex_macro_resolve_path("patch.syx", out, sizeof(out)) == 0);
    CHECK(strcmp(out, "/tmp/zt_sxm_test/patch.syx") == 0);
    zt_config_globals.syx_folder[0] = '\0';
}

static void test_read_file_valid() {
    const char *p = "/tmp/zt_sxm_valid.syx";
    unsigned char data[] = {0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7};
    write_tmp_bytes(p, data, sizeof(data));
    unsigned char *buf = NULL;
    int len = 0;
    CHECK(zt_sysex_macro_read_file(p, &buf, &len, 1024) == 1);
    CHECK(len == (int)sizeof(data));
    CHECK(memcmp(buf, data, sizeof(data)) == 0);
    free(buf);
    unlink(p);
}

static void test_read_file_malformed() {
    const char *p = "/tmp/zt_sxm_bad.syx";
    unsigned char missing_f0[] = {0x12, 0x34, 0xF7};   // no F0 prefix
    write_tmp_bytes(p, missing_f0, sizeof(missing_f0));
    unsigned char *buf = NULL;
    int len = 0;
    CHECK(zt_sysex_macro_read_file(p, &buf, &len, 1024) == 0);
    CHECK(buf == NULL);
    unlink(p);

    unsigned char missing_f7[] = {0xF0, 0x12, 0x34};   // no F7 terminator
    write_tmp_bytes(p, missing_f7, sizeof(missing_f7));
    CHECK(zt_sysex_macro_read_file(p, &buf, &len, 1024) == 0);
    unlink(p);
}

static void test_read_file_too_large() {
    const char *p = "/tmp/zt_sxm_big.syx";
    unsigned char data[100];
    data[0] = 0xF0;
    for (int i = 1; i < 99; i++) data[i] = (unsigned char)(i & 0x7F);
    data[99] = 0xF7;
    write_tmp_bytes(p, data, sizeof(data));
    unsigned char *buf = NULL;
    int len = 0;
    CHECK(zt_sysex_macro_read_file(p, &buf, &len, 50) == 0);   // capped at 50 < 100
    CHECK(zt_sysex_macro_read_file(p, &buf, &len, 200) == 1);  // capped at 200 > 100
    free(buf);
    unlink(p);
}

static void test_read_file_missing() {
    unsigned char *buf = NULL;
    int len = 0;
    CHECK(zt_sysex_macro_read_file("/tmp/does_not_exist_xyz.syx",
                                   &buf, &len, 1024) == 0);
}

int main(void) {
    test_predicate();
    test_resolve_path_with_override();
    test_read_file_valid();
    test_read_file_malformed();
    test_read_file_too_large();
    test_read_file_missing();
    printf("sysex_macro: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
