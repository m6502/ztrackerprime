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
#define SX_FILE_W       38
#define SX_RECV_X       (SX_FILE_X + SX_FILE_W + 2)
#define SX_RECV_Y       (SX_BASE_Y + 3)
#define SX_SPACE_BOTTOM 8

// IDs for the page's UI element pool. Only one tab-stop today (the
// file selector) so the layout is simple, but it leaves room to grow.
#define SX_FILELIST_ID  50

static CUI_SysExLibrarian *g_self_for_listbox = NULL;   // OnSelect back-pointer

// ---------------------------------------------------------------------------
// SxFileSelector — real ListBox for the Files pane. Matches the F11
// SkinSelector / Shift+F3 CC Console / F4 / Shift+F4 idiom: anonymous-
// namespace subclass, frame=1, Space-applies override on update(), no
// ListBox::OnSelect call from the subclass (the recurring foot-gun in
// references/foot-guns.md).
//
// Typeahead is intentionally disabled (use_key_select = false) because
// the page already binds letter keys for global commands -- S = send,
// R = rescan, C = clear log. With typeahead on, those keystrokes would
// jump the listbox cursor instead of firing the command.

namespace {
class SxFileSelector : public ListBox {
public:
    SxFileSelector() {
        empty_message       = "(no .syx files yet -- send/recv will populate)";
        is_sorted           = true;
        use_checks          = false;
        use_key_select      = false;
        wrap_focus_at_edges = false;
        frame               = 1;
    }
    void OnChange()       override {}
    void OnSelectChange() override {}
    int update() override {
        // Mirror F4/Shift+F4/Shift+F3: Space and Enter both apply.
        KBKey k = Keys.checkkey();
        if (k == SDLK_SPACE) {
            Keys.getkey();
            LBNode *p = getNode(cur_sel + y_start);
            if (p) OnSelect(p);
            need_refresh++;
            need_redraw++;
            return 0;
        }
        return ListBox::update();
    }
    void OnSelect(LBNode *selected) override {
        if (!selected || !g_self_for_listbox) return;
        g_self_for_listbox->send_selected();
        // NEVER call ListBox::OnSelect from a subclass.
    }
};
} // namespace

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
    g_self_for_listbox = this;
    file_cur = file_top = 0;
    num_files = 0;
    folder[0] = '\0';
    recent_count = 0;
    recv_seq = 0;
    status_line[0] = '\0';

    // File selector tab-stop. ysize = LAST visible row index; the page's
    // update() resizes it per-frame to match the available pane height.
    SxFileSelector *fs = new SxFileSelector;
    fs->x = SX_FILE_X;
    fs->y = SX_FILE_Y;
    fs->xsize = SX_FILE_W;
    fs->ysize = 16;
    UI->add_element(fs, SX_FILELIST_ID);
    file_selector = fs;
}

void CUI_SysExLibrarian::rebuild_file_list_items(void) {
    if (!file_selector) return;
    file_selector->clear();
    for (int i = 0; i < num_files; i++) {
        file_selector->insertItem(files[i]);
    }
    int cursor = (file_cur >= 0 && file_cur < num_files) ? file_cur : 0;
    file_selector->setCursor(cursor);
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
    rebuild_file_list_items();
}

void CUI_SysExLibrarian::send_selected(void) {
    // Sync from listbox first so OnSelect / S-key paths agree on what's
    // selected.
    if (file_selector) {
        file_cur = file_selector->cur_sel + file_selector->y_start;
    }
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

// Thread-safe localtime wrapper. Audit M9: standard `localtime` returns
// a pointer to a shared static struct, technically a portability gotcha
// even though zTracker only calls drain_recv from the UI thread today.
// Falls back to UTC if the platform reentrant variant fails.
static int zt_localtime(time_t t, struct tm *out) {
    if (!out) return 0;
#ifdef _WIN32
    return localtime_s(out, &t) == 0 ? 1 : 0;
#else
    return localtime_r(&t, out) ? 1 : 0;
#endif
}

// Audit L15: cap the number of `recv_*.syx` files in syx_folder.
// Sorts by mtime, deletes oldest until count <= max. max=0 disables.
// Called from drain_recv after each successful save so the folder
// stays bounded regardless of session length.
static void prune_recv_files(const char *folder, int max_files) {
    if (max_files <= 0 || !folder || !*folder) return;
    // Collect names + mtimes.
    struct Entry {
        char name[256];
        time_t mtime;
    };
    Entry entries[1024];
    int n = 0;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\recv_*.syx", folder);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (n >= 1024) break;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        snprintf(entries[n].name, sizeof(entries[n].name), "%s", fd.cFileName);
        ULARGE_INTEGER ull;
        ull.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
        ull.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        entries[n].mtime = (time_t)((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
        n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *dp = opendir(folder);
    if (!dp) return;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (n >= 1024) break;
        const char *fn = de->d_name;
        if (strncmp(fn, "recv_", 5) != 0) continue;
        size_t flen = strlen(fn);
        if (flen < 5 || strcmp(fn + flen - 4, ".syx") != 0) continue;
        char path[1280];
        snprintf(path, sizeof(path), "%s/%s", folder, fn);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        snprintf(entries[n].name, sizeof(entries[n].name), "%s", fn);
        entries[n].mtime = st.st_mtime;
        n++;
    }
    closedir(dp);
#endif
    if (n <= max_files) return;
    // Sort by mtime ascending (oldest first).
    for (int i = 1; i < n; i++) {
        Entry e = entries[i];
        int j = i - 1;
        while (j >= 0 && entries[j].mtime > e.mtime) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = e;
    }
    int to_delete = n - max_files;
    for (int i = 0; i < to_delete; i++) {
        char path[1280];
        snprintf(path, sizeof(path), "%s/%s", folder, entries[i].name);
        unlink(path);
    }
}

void CUI_SysExLibrarian::drain_recv(void) {
    while (zt_sysex_inq_size() > 0) {
        unsigned char buf[ZT_SYSEX_MAX_LEN];
        int n = zt_sysex_inq_pop(buf, sizeof(buf));
        if (n <= 0) break;

        // Auto-save to disk.
        time_t now = time(NULL);
        struct tm tmbuf;
        int has_tm = zt_localtime(now, &tmbuf);
        char tsbuf[32];
        if (has_tm) strftime(tsbuf, sizeof(tsbuf), "%Y%m%d_%H%M%S", &tmbuf);
        else        snprintf(tsbuf, sizeof(tsbuf), "%ld", (long)now);
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
        if (has_tm) strftime(tsdisp, sizeof(tsdisp), "%H:%M:%S", &tmbuf);
        else        snprintf(tsdisp, sizeof(tsdisp), "?:?:?");
        snprintf(r->timestamp, sizeof(r->timestamp), "%s", tsdisp);
        r->length = n;
        int prev_n = n < (int)sizeof(r->preview) ? n : (int)sizeof(r->preview);
        memcpy(r->preview, buf, (size_t)prev_n);
        for (int k = prev_n; k < (int)sizeof(r->preview); k++) r->preview[k] = 0;
        snprintf(r->saved_as, sizeof(r->saved_as), "%s", fname);

        snprintf(status_line, sizeof(status_line),
                 "Received %d bytes. %s -> %s.",
                 n, ok ? "Saved" : "Save FAILED", fname);

        // Cap the recv_*.syx count so syx_folder doesn't grow without
        // bound across long sessions. Audit L15.
        prune_recv_files(folder, zt_config_globals.syx_recv_max_files);

        // Refresh the file list so the new file shows up immediately.
        rescan_folder();
        need_refresh++;
    }
}

void CUI_SysExLibrarian::enter(void) {
    cur_state = STATE_SYSEX_LIB;
    rescan_folder();
    UI->cur_element = SX_FILELIST_ID;
    // Seed recv_seq past any existing `recv_<TS>_NNN.syx` files so a
    // capture in the same wall-clock second after a restart doesn't
    // clobber a previous file. Scans the rescanned `files[]` array,
    // parses the trailing _NNN, takes max+1. Audit M7.
    int max_seq = -1;
    for (int i = 0; i < num_files; i++) {
        const char *fn = files[i];
        if (strncmp(fn, "recv_", 5) != 0) continue;
        // Find the LAST `_NNN.syx`. Walk backwards from the dot.
        size_t flen = strlen(fn);
        if (flen < 9) continue;
        if (strcmp(fn + flen - 4, ".syx") != 0) continue;
        // Locate the underscore before the NNN -- 7 chars before .syx
        // ("_NNN.syx" = 8 chars, but NNN width is %03d so >= 3 digits).
        const char *us = strrchr(fn, '_');
        if (!us) continue;
        int n = atoi(us + 1);
        if (n > max_seq) max_seq = n;
    }
    if (max_seq + 1 > recv_seq) recv_seq = max_seq + 1;
    snprintf(status_line, sizeof(status_line),
             "Up/Dn pick file | Enter or Space or S send | R rescan | C clear log | ESC exit");
    Keys.flush();
    need_refresh++;
    UI->enter();
}

void CUI_SysExLibrarian::leave(void) {
}

void CUI_SysExLibrarian::update(void) {
    int total_rows    = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int file_max_rows = total_rows - SX_FILE_Y - SX_SPACE_BOTTOM - 3;
    if (file_max_rows < 4) file_max_rows = 4;

    // Resize the listbox per frame so window-resize stays correct, and
    // stamp need_redraw=1 so a sibling fill (drain_recv -> recent log
    // repaint) can't blank it. See references/foot-guns.md
    // "Vanishing sibling widgets on `needaclear`".
    if (file_selector) {
        file_selector->ysize = file_max_rows - 1;
        file_selector->need_redraw = 1;
    }

    // Mirror the listbox cursor into file_cur so existing callers
    // (status messages, send_selected) keep working.
    if (file_selector) {
        file_cur = file_selector->cur_sel + file_selector->y_start;
        file_top = file_selector->y_start;
    }

    drain_recv();

    // Up / Down / Enter / Space are routed to the listbox via UI->update().
    UI->update();

    if (!Keys.size()) return;

    KBKey key = Keys.getkey();

    if (key == SDLK_ESCAPE) {
        switch_page(UIP_Patterneditor);
        return;
    }
    if (key == SDLK_S) {
        send_selected();
        need_refresh++;
        return;
    }
    if (key == SDLK_R) {
        rescan_folder();
        snprintf(status_line, sizeof(status_line),
                 "Rescanned: %d file(s) in %s", num_files, folder);
        need_refresh++;
        return;
    }
    if (key == SDLK_C) {
        recent_count = 0;
        snprintf(status_line, sizeof(status_line), "Recent log cleared.");
        need_refresh++;
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
    // The ListBox widget paints its own rows (with the F11/F4-style
    // black-bar highlight) via UI->draw(S) further below. Here we
    // only paint the "Files (.syx)" title above it.
    print(col(SX_FILE_X), row(SX_FILE_Y - 1), "Files (.syx)",
          COLORS.Brighttext, S);

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

    // Paint the listbox + any other UI elements last so they sit on top
    // of the page-level fillRect/title prints.
    UI->draw(S);

    S->unlock();
}
