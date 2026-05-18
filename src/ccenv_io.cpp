/* ccenv_io.cpp — .env text-format reader/writer for inst_envelope. */
#include "ccenv_io.h"
#include "zt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#if defined(_WIN32)
  #include <windows.h>
  #define strcasecmp _stricmp
  #ifndef S_ISDIR
    #define S_ISDIR(m) (((m) & _S_IFDIR) == _S_IFDIR)
  #endif
#else
  #include <dirent.h>
  #include <sys/types.h>
#endif

// ---------------------------------------------------------------------------
// Helpers

static void trim_inplace(char *s) {
    if (!s) return;
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n'))
        s[--n] = 0;
    int lead = 0;
    while (s[lead] == ' ' || s[lead] == '\t') lead++;
    if (lead > 0) memmove(s, s + lead, strlen(s + lead) + 1);
}

static int parse_flags(const char *s) {
    int flags = 0;
    char buf[256]; strncpy(buf, s, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *tok = strtok(buf, ",;|+ \t");
    while (tok) {
        trim_inplace(tok);
        if      (!strcmp(tok, "enabled"))  flags |= ZTM_INSTENVF_ENABLED;
        else if (!strcmp(tok, "loop"))     flags |= ZTM_INSTENVF_LOOP;
        else if (!strcmp(tok, "sustain"))  flags |= ZTM_INSTENVF_SUSTAIN;
        else if (!strcmp(tok, "carry"))    flags |= ZTM_INSTENVF_CARRY;
        tok = strtok(NULL, ",;|+ \t");
    }
    return flags;
}

static void emit_flags(int flags, char out[64]) {
    out[0] = 0;
    int first = 1;
    if (flags & ZTM_INSTENVF_ENABLED) { if (!first) strcat(out, ","); strcat(out, "enabled"); first = 0; }
    if (flags & ZTM_INSTENVF_LOOP)    { if (!first) strcat(out, ","); strcat(out, "loop");    first = 0; }
    if (flags & ZTM_INSTENVF_SUSTAIN) { if (!first) strcat(out, ","); strcat(out, "sustain"); first = 0; }
    if (flags & ZTM_INSTENVF_CARRY)   { if (!first) strcat(out, ","); strcat(out, "carry");   first = 0; }
}

static int parse_range(const char *s, int *lo, int *hi) {
    const char *p = s;
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (!*p) return -1;
    *lo = atoi(p);
    while (*p && (isdigit((unsigned char)*p) || *p == '-')) p++;
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (!*p) { *hi = *lo; return 0; }
    *hi = atoi(p);
    return 0;
}

static int ci_strcmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int ensure_parent_dir(const char *path) {
    const char *slash = strrchr(path, '/');
#if defined(_WIN32)
    const char *bslash = strrchr(path, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    if (!slash || slash == path) return 0;
    char dir[1024];
    int len = (int)(slash - path);
    if (len >= (int)sizeof(dir)) len = sizeof(dir) - 1;
    memcpy(dir, path, len);
    dir[len] = 0;
    struct stat st;
    if (stat(dir, &st) == 0) return 0;
#if defined(_WIN32)
    CreateDirectoryA(dir, NULL);
#else
    mkdir(dir, 0755);
#endif
    return 0;
}

// ---------------------------------------------------------------------------

int ccenv_read_file(const char *path, struct inst_envelope *dst) {
    if (!path || !dst) return -1;
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    memset(dst, 0, sizeof *dst);
    dst->cc        = 1;
    dst->kind      = 0;
    dst->flags     = ZTM_INSTENVF_ENABLED;
    dst->num_nodes = 0;
    dst->speed     = 1;

    char line[512];
    int in_nodes = 0;
    while (fgets(line, sizeof line, fp)) {
        for (char *p = line; *p; p++) {
            if (*p == '#' || *p == ';') { *p = 0; break; }
        }
        trim_inplace(line);
        if (!line[0]) continue;

        if (!in_nodes) {
            if (!strcasecmp(line, "nodes")) { in_nodes = 1; continue; }
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = 0;
            char *key = line; char *val = eq + 1;
            trim_inplace(key); trim_inplace(val);
            for (char *p = key; *p; p++) *p = (char)tolower((unsigned char)*p);

            if (!strcmp(key, "cc")) {
                int v = atoi(val); if (v < 0) v = 0; if (v > 127) v = 127;
                dst->cc = (unsigned char)v;
            } else if (!strcmp(key, "kind")) {
                int v = atoi(val); if (v < 0 || v > 2) v = 0;
                dst->kind = (unsigned char)v;
            } else if (!strcmp(key, "speed")) {
                int v = atoi(val); if (v < 1) v = 1; if (v > 65535) v = 65535;
                dst->speed = (unsigned short)v;
            } else if (!strcmp(key, "flags")) {
                dst->flags = (unsigned char)parse_flags(val);
            } else if (!strcmp(key, "loop")) {
                int lo = 0, hi = 0;
                if (parse_range(val, &lo, &hi) == 0) {
                    dst->loop_start = (unsigned char)lo;
                    dst->loop_end   = (unsigned char)hi;
                }
            } else if (!strcmp(key, "sustain")) {
                int lo = 0, hi = 0;
                if (parse_range(val, &lo, &hi) == 0) {
                    dst->sustain_start = (unsigned char)lo;
                    dst->sustain_end   = (unsigned char)hi;
                }
            }
        } else {
            int t = 0, v = 0;
            if (sscanf(line, "%d %d", &t, &v) != 2 &&
                sscanf(line, "%d,%d", &t, &v) != 2) continue;
            if (dst->num_nodes >= ZTM_INST_ENV_MAX_NODES) continue;
            if (t < 0) t = 0; if (t > 65535) t = 65535;
            if (v < 0) v = 0; if (v > 127) v = 127;
            dst->tick[dst->num_nodes]  = (unsigned short)t;
            dst->value[dst->num_nodes] = (unsigned char)v;
            dst->num_nodes++;
        }
    }
    fclose(fp);

    if (dst->num_nodes > 0) {
        int max_idx = dst->num_nodes - 1;
        if (dst->loop_end      > max_idx) dst->loop_end      = (unsigned char)max_idx;
        if (dst->loop_start    > dst->loop_end)    dst->loop_start    = dst->loop_end;
        if (dst->sustain_end   > max_idx) dst->sustain_end   = (unsigned char)max_idx;
        if (dst->sustain_start > dst->sustain_end) dst->sustain_start = dst->sustain_end;
    }
    return 0;
}

int ccenv_write_file(const char *path, const struct inst_envelope *src) {
    if (!path || !src) return -1;
    ensure_parent_dir(path);
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "# zTracker Prime CC Envelope preset\n");
    fprintf(fp, "cc=%u\n",    src->cc);
    fprintf(fp, "kind=%u\n",  src->kind);
    fprintf(fp, "speed=%u\n", src->speed);

    char flagbuf[64]; emit_flags(src->flags, flagbuf);
    fprintf(fp, "flags=%s\n", flagbuf);

    if (src->flags & ZTM_INSTENVF_LOOP)
        fprintf(fp, "loop=%u..%u\n", src->loop_start, src->loop_end);
    if (src->flags & ZTM_INSTENVF_SUSTAIN)
        fprintf(fp, "sustain=%u..%u\n", src->sustain_start, src->sustain_end);

    fprintf(fp, "nodes\n");
    for (int i = 0; i < src->num_nodes; i++) {
        fprintf(fp, "%u\t%u\n", src->tick[i], src->value[i]);
    }
    fclose(fp);
    return 0;
}

int ccenv_scan_folder(const char *folder,
                      char out_basenames[][256],
                      int max_out)
{
    if (!folder || !*folder || !out_basenames || max_out <= 0) return 0;
    int n = 0;

#if defined(_WIN32)
    char glob[1280];
    snprintf(glob, sizeof glob, "%s\\*.env", folder);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(glob, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.cFileName[0] == '.') continue;
        if (n >= max_out) break;
        strncpy(out_basenames[n], fd.cFileName, 255);
        out_basenames[n][255] = 0;
        n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(folder);
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        const char *dot = strrchr(de->d_name, '.');
        if (!dot || strcasecmp(dot, ".env") != 0) continue;
        if (n >= max_out) break;
        strncpy(out_basenames[n], de->d_name, 255);
        out_basenames[n][255] = 0;
        n++;
    }
    closedir(d);
#endif

    // Insertion sort (small N).
    for (int i = 1; i < n; i++) {
        char key[256];
        memcpy(key, out_basenames[i], 256);
        int j = i - 1;
        while (j >= 0 && ci_strcmp(out_basenames[j], key) > 0) {
            memcpy(out_basenames[j + 1], out_basenames[j], 256);
            j--;
        }
        memcpy(out_basenames[j + 1], key, 256);
    }
    return n;
}

int ccenv_resolve_folder(char *out, int out_size) {
    if (!out || out_size < 2) return -1;
    out[0] = 0;

    if (zt_config_globals.ccenv_folder[0]) {
        strncpy(out, zt_config_globals.ccenv_folder, out_size - 1);
        out[out_size - 1] = 0;
        struct stat st;
        if (stat(out, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
        return -1;
    }
    if (zt_directory[0]) {
        snprintf(out, out_size, "%s/envelopes", zt_directory);
        struct stat st;
        if (stat(out, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    }
    strncpy(out, "./envelopes", out_size - 1);
    out[out_size - 1] = 0;
    struct stat st;
    if (stat(out, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    return -1;
}
