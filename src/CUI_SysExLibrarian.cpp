/*************************************************************************
 *
 * FILE  CUI_SysExLibrarian.cpp
 *
 * DESCRIPTION
 *   SysEx Librarian (Shift+F5).
 *
 *   Workflow:
 *     1. Folder of `.syx` files. The user keeps "request" SysEx files
 *        here (e.g. Universal Device Inquiry, vendor-specific patch-
 *        dump-request) plus any previously captured patches.
 *     2. Pick a file with Up/Dn, press Enter to send it out the current
 *        MIDI Out device. That triggers the synth to respond.
 *     3. The synth's response is auto-captured. Every received SysEx is
 *        written to disk as `recv_<timestamp>.syx` in the same folder
 *        and added to the on-screen "Recent" list. The next folder
 *        rescan picks the new files up so they're immediately
 *        available to re-send.
 *
 *   Storage:
 *     * `syx_folder` zt.conf key. Empty -> defaults to
 *       <ccizer_folder>/syx, auto-created on first use.
 *
 *   Receive parsing:
 *     * macOS via the SysEx accumulator in winmm_compat.h ->
 *       zt_sysex_inq_push() -> drain here on every update().
 *     * Linux ALSA-in is stubbed in the platform layer; this page
 *       still works for SEND. Receive parsing on Linux/Windows is a
 *       follow-up PR.
 *
 ******/
#include "zt.h"
#include "sysex_inq.h"
#include "ccizer.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#  define mkdir_p(p) _mkdir(p)
#else
#  include <dirent.h>
#  include <unistd.h>
#  define mkdir_p(p) mkdir(p, 0755)
#endif

#define SX_BASE_Y       (TRACKS_ROW_Y + 0)
#define SX_FILE_X       2
#define SX_FILE_Y       (SX_BASE_Y + 3)
#define SX_FILE_W       28
#define SX_RECV_X       (SX_FILE_X + SX_FILE_W + 2)
#define SX_RECV_Y       (SX_BASE_Y + 3)
#define SX_SPACE_BOTTOM 8

// --------- Helpers ---------
static int dir_exists(const char *path) {
    if (!path || !*path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFDIR) ? 1 : 0;
}

static void make_dir_recursive(const char *path) {
    if (!path || !*path || dir_exists(path)) return;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            if (!dir_exists(buf)) mkdir_p(buf);
            *p = saved;
        }
    }
    if (!dir_exists(buf)) mkdir_p(buf);
}

static int qsort_cmp_str(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// Scan `dir` for files ending in ".syx".
static int scan_syx_dir(const char *dir, char (*out)[256], int max_results) {
    if (!dir || !*dir || !out || max_results <= 0) return 0;
    int count = 0;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.syx", dir);
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
        if (strcmp(n + len - 4, ".syx") != 0) continue;
        snprintf(out[count++], 256, "%s", n);
    }
    closedir(dp);
#endif
    qsort(out, count, sizeof(out[0]), qsort_cmp_str);
    return count;
}

static long file_size_of(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static int send_file(const char *path, int *out_len) {
    if (out_len) *out_len = 0;
    long len = file_size_of(path);
    if (len <= 0 || len > 1024 * 1024) return 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf) { fclose(fp); return 0; }
    int got = (int)fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    if (got != len) { free(buf); return 0; }
    if (got < 2 || buf[0] != 0xF0 || buf[got - 1] != 0xF7) {
        free(buf);
        return -1;          // malformed: not a properly-framed SysEx
    }

    // Find a usable MIDI Out device. Same fallback logic as the CC Console.
    int dev = -1;
    if (cur_inst >= 0 && cur_inst < MAX_INSTS && song->instruments[cur_inst]
        && song->instruments[cur_inst]->midi_device != 0xff) {
        dev = song->instruments[cur_inst]->midi_device;
    }
    if (dev < 0) {
        for (int i = 0; i < (int)MidiOut->numOuputDevices; i++) {
            if (MidiOut->outputDevices[i] && MidiOut->outputDevices[i]->opened) {
                dev = i; break;
            }
        }
    }
    if (dev < 0) { free(buf); return -2; }

    int rc = MidiOut->sendSysEx((unsigned int)dev, buf, got);
    free(buf);
    if (out_len) *out_len = got;
    return rc ? 1 : 0;
}

static int write_file(const char *path, const unsigned char *bytes, int len) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    int n = (int)fwrite(bytes, 1, (size_t)len, fp);
    fclose(fp);
    return n == len ? 1 : 0;
}

// --------- Lifecycle ---------
CUI_SysExLibrarian::CUI_SysExLibrarian() {
    UI = new UserInterface;
    file_cur = file_top = 0;
    num_files = 0;
    folder[0] = '\0';
    recent_count = 0;
    recv_seq = 0;
    status_line[0] = '\0';
}

void CUI_SysExLibrarian::resolve_folder(void) {
    folder[0] = '\0';
    if (zt_config_globals.syx_folder[0]) {
        snprintf(folder, sizeof(folder), "%s", zt_config_globals.syx_folder);
    } else {
        // Default: ./syx — next to the binary (or in macOS .app's
        // Resources/syx after the chdir). Matches where CMake POST_BUILD
        // copies the bundled `assets/syx` directory so the example
        // request files are visible out of the box.
        struct stat st;
        if (stat("./syx", &st) == 0 && (st.st_mode & S_IFDIR)) {
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
    make_dir_recursive(folder);
}

void CUI_SysExLibrarian::rescan_folder(void) {
    resolve_folder();
    num_files = 0;
    if (folder[0]) {
        num_files = scan_syx_dir(folder, files, 256);
    }
    if (file_cur >= num_files) file_cur = num_files > 0 ? num_files - 1 : 0;
    if (file_top > file_cur)   file_top = file_cur;
}

void CUI_SysExLibrarian::send_selected(void) {
    if (file_cur < 0 || file_cur >= num_files) return;
    char path[1280];
    snprintf(path, sizeof(path), "%s/%s", folder, files[file_cur]);
    int len = 0;
    int rc = send_file(path, &len);
    if (rc == 1) {
        snprintf(status_line, sizeof(status_line),
                 "Sent %s (%d bytes). Listening for response...",
                 files[file_cur], len);
    } else if (rc == -1) {
        snprintf(status_line, sizeof(status_line),
                 "ERROR: %s is not a valid SysEx file (must start F0, end F7).",
                 files[file_cur]);
    } else if (rc == -2) {
        snprintf(status_line, sizeof(status_line),
                 "ERROR: no MIDI Out device open. Open one in F12 first.");
    } else {
        snprintf(status_line, sizeof(status_line),
                 "ERROR sending %s.", files[file_cur]);
    }
}

void CUI_SysExLibrarian::drain_recv(void) {
    while (zt_sysex_inq_size() > 0) {
        unsigned char buf[ZT_SYSEX_MAX_LEN];
        int n = zt_sysex_inq_pop(buf, sizeof(buf));
        if (n <= 0) break;

        // Auto-save to disk.
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char tsbuf[32];
        if (tm) strftime(tsbuf, sizeof(tsbuf), "%Y%m%d_%H%M%S", tm);
        else    snprintf(tsbuf, sizeof(tsbuf), "%ld", (long)now);
        char fname[80];
        snprintf(fname, sizeof(fname), "recv_%s_%03d.syx", tsbuf, recv_seq++);

        char path[1280];
        snprintf(path, sizeof(path), "%s/%s", folder, fname);
        int ok = write_file(path, buf, n);

        // Push to the recent ring.
        if (recent_count < RECENT_LOG_MAX) {
            // Empty slot.
        } else {
            // Slide everything down by one.
            for (int i = 1; i < RECENT_LOG_MAX; i++) recent[i - 1] = recent[i];
            recent_count = RECENT_LOG_MAX - 1;
        }
        RecentLog *r = &recent[recent_count++];
        char tsdisp[16];
        if (tm) strftime(tsdisp, sizeof(tsdisp), "%H:%M:%S", tm);
        else    snprintf(tsdisp, sizeof(tsdisp), "?:?:?");
        snprintf(r->timestamp, sizeof(r->timestamp), "%s", tsdisp);
        r->length = n;
        int prev_n = n < (int)sizeof(r->preview) ? n : (int)sizeof(r->preview);
        memcpy(r->preview, buf, (size_t)prev_n);
        for (int k = prev_n; k < (int)sizeof(r->preview); k++) r->preview[k] = 0;
        snprintf(r->saved_as, sizeof(r->saved_as), "%s", fname);

        snprintf(status_line, sizeof(status_line),
                 "Received %d bytes. %s -> %s.",
                 n, ok ? "Saved" : "Save FAILED", fname);

        // Refresh the file list so the new file shows up immediately.
        rescan_folder();
    }
}

void CUI_SysExLibrarian::enter(void) {
    cur_state = STATE_SYSEX_LIB;
    rescan_folder();
    snprintf(status_line, sizeof(status_line),
             "Up/Dn pick file | Enter or S send | R rescan | C clear log | ESC exit");
    Keys.flush();
}

void CUI_SysExLibrarian::leave(void) {
}

void CUI_SysExLibrarian::update(void) {
    drain_recv();

    if (!Keys.size()) return;

    KBKey key = Keys.getkey();

    if (key == SDLK_ESCAPE) {
        switch_page(UIP_Patterneditor);
        return;
    }
    if (key == SDLK_UP) {
        if (file_cur > 0) file_cur--;
        return;
    }
    if (key == SDLK_DOWN) {
        if (file_cur + 1 < num_files) file_cur++;
        return;
    }
    if (key == SDLK_RETURN || key == SDLK_S) {
        send_selected();
        return;
    }
    if (key == SDLK_R) {
        rescan_folder();
        snprintf(status_line, sizeof(status_line),
                 "Rescanned: %d file(s) in %s", num_files, folder);
        return;
    }
    if (key == SDLK_C) {
        recent_count = 0;
        snprintf(status_line, sizeof(status_line), "Recent log cleared.");
        return;
    }
}

void CUI_SysExLibrarian::draw(Drawable *S) {
    if (S->lock() != 0) return;

    int total_rows    = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int file_max_rows = total_rows - SX_FILE_Y - SX_SPACE_BOTTOM - 3;
    if (file_max_rows < 4) file_max_rows = 4;

    S->fillRect(col(1), row(SX_BASE_Y - 1),
                INTERNAL_RESOLUTION_X - 2,
                row(total_rows - SX_SPACE_BOTTOM),
                COLORS.Background);

    // Out device name for the header. Falls back to "(none)".
    const char *out_name = "(none)";
    {
        int dev = -1;
        if (cur_inst >= 0 && cur_inst < MAX_INSTS &&
            song->instruments[cur_inst] &&
            song->instruments[cur_inst]->midi_device != 0xff) {
            dev = song->instruments[cur_inst]->midi_device;
        }
        if (dev < 0) {
            for (int i = 0; i < (int)MidiOut->numOuputDevices; i++) {
                if (MidiOut->outputDevices[i] && MidiOut->outputDevices[i]->opened) {
                    dev = i; break;
                }
            }
        }
        if (dev >= 0 && MidiOut->outputDevices[dev]) {
            const char *al = MidiOut->get_alias(dev);
            if (al && *al) out_name = al;
        }
    }
    char title[160];
    snprintf(title, sizeof(title),
             "SysEx Librarian (Shift+F5)   Out: %s", out_name);
    printtitle(PAGE_TITLE_ROW_Y, title, COLORS.Text, COLORS.Background, S);

    // Folder line
    {
        char fbuf[160];
        snprintf(fbuf, sizeof(fbuf), "Folder: %s", folder[0] ? folder : "(none)");
        print(col(2), row(SX_BASE_Y + 1), fbuf, COLORS.Lowlight, S);
    }

    // ----- File list pane -----
    print(col(SX_FILE_X), row(SX_FILE_Y - 1), "Files (.syx)",
          COLORS.Brighttext, S);
    if (file_cur < file_top) file_top = file_cur;
    if (file_cur >= file_top + file_max_rows)
        file_top = file_cur - file_max_rows + 1;

    if (num_files == 0) {
        print(col(SX_FILE_X), row(SX_FILE_Y),
              "(no .syx files yet -- send/recv will populate)",
              COLORS.Lowlight, S);
    } else {
        for (int i = 0; i < file_max_rows && file_top + i < num_files; i++) {
            int idx = file_top + i;
            char path[1280];
            snprintf(path, sizeof(path), "%s/%s", folder, files[idx]);
            long sz = file_size_of(path);
            char line[128];
            snprintf(line, sizeof(line), "%c %-22.22s  %6ld B",
                     (idx == file_cur) ? '>' : ' ', files[idx], sz);
            TColor fg = (idx == file_cur) ? COLORS.Brighttext : COLORS.Text;
            TColor bg = (idx == file_cur) ? COLORS.SelectedBGLow : COLORS.Background;
            printBG(col(SX_FILE_X), row(SX_FILE_Y + i), line, fg, bg, S);
        }
    }

    // ----- Recent receive pane -----
    print(col(SX_RECV_X), row(SX_RECV_Y - 1), "Recent receive",
          COLORS.Brighttext, S);
    if (recent_count == 0) {
        print(col(SX_RECV_X), row(SX_RECV_Y),
              "(none yet -- send a request file to trigger a dump)",
              COLORS.Lowlight, S);
    } else {
        for (int i = 0; i < recent_count; i++) {
            const RecentLog *r = &recent[i];
            // Hex preview of the first few bytes.
            char hex[40] = "";
            int  prev_n = r->length < (int)sizeof(r->preview)
                              ? r->length
                              : (int)sizeof(r->preview);
            for (int k = 0; k < prev_n; k++) {
                char tmp[4];
                snprintf(tmp, sizeof(tmp), "%02X ", r->preview[k]);
                strncat(hex, tmp, sizeof(hex) - strlen(hex) - 1);
            }
            char line[200];
            snprintf(line, sizeof(line),
                     "%s  %5d B  %-24s %s",
                     r->timestamp, r->length, hex, r->saved_as);
            print(col(SX_RECV_X), row(SX_RECV_Y + i),
                  line, COLORS.Text, S);
        }
    }

    // ----- Status line -----
    print(col(2), row(total_rows - SX_SPACE_BOTTOM - 1),
          status_line, COLORS.Lowlight, S);

    S->unlock();
}
