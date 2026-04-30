#include "sysex_macro.h"
#include "conf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

extern ZTConf zt_config_globals;

extern "C" int zt_sysex_macro_is_file(const char *name) {
    if (!name) return 0;
    size_t len = strlen(name);
    // Require at least one character before the ".syx" extension --
    // a bare ".syx" isn't a usable filename.
    if (len <= 4) return 0;
    return (tolower((unsigned char)name[len-4]) == '.' &&
            tolower((unsigned char)name[len-3]) == 's' &&
            tolower((unsigned char)name[len-2]) == 'y' &&
            tolower((unsigned char)name[len-1]) == 'x') ? 1 : 0;
}

static int dir_exists(const char *p) {
    struct stat st;
    if (!p || !*p || stat(p, &st) != 0) return 0;
    return (st.st_mode & S_IFDIR) ? 1 : 0;
}

extern "C" int zt_sysex_macro_resolve_path(const char *name, char *out, size_t out_cap) {
    if (!name || !out || out_cap < 16) return -1;
    out[0] = '\0';

    char folder[1024] = "";
    if (zt_config_globals.syx_folder[0]) {
        snprintf(folder, sizeof(folder), "%s", zt_config_globals.syx_folder);
    } else {
        // Match CUI_SysExLibrarian::resolve_folder() default cascade.
        if (dir_exists("./syx")) {
            snprintf(folder, sizeof(folder), "./syx");
        } else {
#ifndef _WIN32
            const char *home = getenv("HOME");
            if (home && *home)
                snprintf(folder, sizeof(folder), "%s/.config/zt/syx", home);
            else
                snprintf(folder, sizeof(folder), "./syx");
#else
            snprintf(folder, sizeof(folder), "./syx");
#endif
        }
    }
    snprintf(out, out_cap, "%s/%s", folder, name);
    return 0;
}

extern "C" int zt_sysex_macro_read_file(const char *path,
                                        unsigned char **out_buf,
                                        int *out_len,
                                        int max_len) {
    if (!path || !out_buf || !out_len) return 0;
    *out_buf = NULL;
    *out_len = 0;

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_size <= 0 || (max_len > 0 && (long)st.st_size > (long)max_len)) return 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    int len = (int)st.st_size;
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf) { fclose(fp); return 0; }
    int got = (int)fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    if (got != len) { free(buf); return 0; }
    if (got < 2 || buf[0] != 0xF0 || buf[got-1] != 0xF7) {
        free(buf);
        return 0;       // malformed: not a real SysEx file
    }
    *out_buf = buf;
    *out_len = got;
    return 1;
}
