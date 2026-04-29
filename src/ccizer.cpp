#include "ccizer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dirent.h>
#  include <unistd.h>
#endif

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static const char *skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static void rstrip(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' ||
                     s[n - 1] == ' '  || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
}

static void path_basename(const char *path, char *out, size_t out_sz) {
    const char *p = path;
    const char *last = path;
    while (*p) {
        if (*p == '/' || *p == '\\') last = p + 1;
        p++;
    }
    snprintf(out, out_sz, "%s", last);
}

static void path_swap_extension(const char *path, const char *new_ext,
                                char *out, size_t out_sz) {
    snprintf(out, out_sz, "%s", path);
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
    size_t len = strlen(out);
    snprintf(out + len, out_sz - len, "%s", new_ext);
}

// ----------------------------------------------------------------------------
// Parser
// ----------------------------------------------------------------------------

int zt_ccizer_load(const char *path, ZtCcizerFile *out) {
    if (!path || !out) return -1;
    memset(out, 0, sizeof(*out));
    snprintf(out->path, sizeof(out->path), "%s", path);
    path_basename(path, out->basename, sizeof(out->basename));

    FILE *fp = fopen(path, "r");
    if (!fp) return -2;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        rstrip(line);
        const char *p = skip_ws(line);
        if (*p == '\0' || *p == '#') continue;
        if (out->num_slots >= ZT_CCIZER_MAX_SLOTS) break;

        // First token: "PB" or decimal CC number.
        unsigned char cc = 0;
        if ((p[0] == 'P' || p[0] == 'p') && (p[1] == 'B' || p[1] == 'b') &&
            (p[2] == ' ' || p[2] == '\t' || p[2] == '\0')) {
            cc = ZT_CCIZER_PB_MARKER;
            p += 2;
        } else if (isdigit((unsigned char)*p)) {
            char *end = NULL;
            long v = strtol(p, &end, 10);
            if (!end || end == p || v < 0 || v > 127) continue;
            cc = (unsigned char)v;
            p = end;
        } else {
            continue; // malformed
        }
        p = skip_ws(p);

        ZtCcizerSlot *slot = &out->slots[out->num_slots++];
        slot->cc = cc;
        slot->view = ZT_CCIZER_VIEW_SLIDER;
        slot->value = (cc == ZT_CCIZER_PB_MARKER) ? 8192 : 0;
        snprintf(slot->name, sizeof(slot->name), "%s", *p ? p : "(unnamed)");
    }
    fclose(fp);

    // Read sidecar if present.
    char sidecar[1280];
    path_swap_extension(path, ".cc-view", sidecar, sizeof(sidecar));
    FILE *vfp = fopen(sidecar, "r");
    if (vfp) {
        while (fgets(line, sizeof(line), vfp)) {
            rstrip(line);
            int slot_idx = 0;
            char view_word[32] = {0};
            if (sscanf(line, "%d %31s", &slot_idx, view_word) == 2 &&
                slot_idx >= 1 && slot_idx <= out->num_slots) {
                ZtCcizerSlot *s = &out->slots[slot_idx - 1];
                if (strcmp(view_word, "knob") == 0)
                    s->view = ZT_CCIZER_VIEW_KNOB;
                else
                    s->view = ZT_CCIZER_VIEW_SLIDER;
            }
        }
        fclose(vfp);
    }

    return 0;
}

int zt_ccizer_save_view_sidecar(const ZtCcizerFile *f) {
    if (!f || !f->path[0]) return -1;
    char sidecar[1280];
    path_swap_extension(f->path, ".cc-view", sidecar, sizeof(sidecar));
    FILE *fp = fopen(sidecar, "w");
    if (!fp) return -2;
    fprintf(fp, "# zTracker CC Console view hints — slot index, slider|knob\n");
    for (int i = 0; i < f->num_slots; i++) {
        fprintf(fp, "%d %s\n", i + 1,
                f->slots[i].view == ZT_CCIZER_VIEW_KNOB ? "knob" : "slider");
    }
    fclose(fp);
    return 0;
}

// ----------------------------------------------------------------------------
// Folder scan
// ----------------------------------------------------------------------------

static int qsort_cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int zt_ccizer_list_dir(const char *dir, char (*out)[256], int max_results) {
    if (!dir || !out || max_results <= 0) return 0;
    int count = 0;

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.txt", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (count >= max_results) break;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        snprintf(out[count++], 256, "%s", fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *dp = opendir(dir);
    if (!dp) return 0;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (count >= max_results) break;
        const char *n = de->d_name;
        size_t len = strlen(n);
        if (len < 5) continue;
        if (strcmp(n + len - 4, ".txt") != 0) continue;
        snprintf(out[count++], 256, "%s", n);
    }
    closedir(dp);
#endif

    qsort(out, count, sizeof(out[0]), qsort_cmp_str);
    return count;
}

// ----------------------------------------------------------------------------
// MIDI byte builder
// ----------------------------------------------------------------------------

int zt_ccizer_build_midi(const ZtCcizerSlot *s, int channel,
                         unsigned char *status,
                         unsigned char *data1,
                         unsigned char *data2) {
    if (!s || !status || !data1 || !data2) return 0;
    if (channel < 1) channel = 1;
    if (channel > 16) channel = 16;
    unsigned char ch = (unsigned char)(channel - 1);

    if (s->cc == ZT_CCIZER_PB_MARKER) {
        unsigned short v = s->value & 0x3FFF;
        *status = 0xE0 | ch;
        *data1  = (unsigned char)(v & 0x7F);
        *data2  = (unsigned char)((v >> 7) & 0x7F);
        return 1;
    }
    if (s->cc > 127) return 0;
    *status = 0xB0 | ch;
    *data1  = s->cc;
    *data2  = (unsigned char)(s->value & 0x7F);
    return 1;
}

// ----------------------------------------------------------------------------
// Folder resolution
// ----------------------------------------------------------------------------

static int dir_exists(const char *path) {
    if (!path || !*path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFDIR) ? 1 : 0;
}

int zt_ccizer_resolve_folder(const char *override_path,
                             const char *exe_dir,
                             char *out, size_t out_sz) {
    if (!out || out_sz < 2) return -1;
    out[0] = '\0';

    if (override_path && *override_path && dir_exists(override_path)) {
        snprintf(out, out_sz, "%s", override_path);
        return 0;
    }

    if (exe_dir && *exe_dir) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/ccizer", exe_dir);
        if (dir_exists(buf)) { snprintf(out, out_sz, "%s", buf); return 0; }
        snprintf(buf, sizeof(buf), "%s/../Resources/ccizer", exe_dir);
        if (dir_exists(buf)) { snprintf(out, out_sz, "%s", buf); return 0; }
        snprintf(buf, sizeof(buf), "%s/../share/zt/ccizer", exe_dir);
        if (dir_exists(buf)) { snprintf(out, out_sz, "%s", buf); return 0; }
    }

#ifndef _WIN32
    const char *home = getenv("HOME");
    if (home && *home) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/.config/zt/ccizer", home);
        if (dir_exists(buf)) { snprintf(out, out_sz, "%s", buf); return 0; }
    }
#endif

    return -2;
}
