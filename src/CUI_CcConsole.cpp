/*************************************************************************
 *
 * FILE  CUI_CcConsole.cpp
 *
 * DESCRIPTION
 *   CC Console (Shift+F3). Real ValueSlider widgets per slot — mouse-
 *   clickable, keyboard-adjustable. Tweak fires MIDI CC (or 14-bit
 *   Pitchbend for `PB` slots) on the current channel out the current
 *   MIDI Out device.
 *
 *   Layout (one row per slot):
 *
 *     +-- Files ---------+   Slot CC#  Name              [slider]  val   Learn
 *     | a.txt            |    > 1   PB  Pitchbend        [#####--]  8192 PB ch1
 *     |>b.txt            |      2    1  Mod              [###----]    45 B0 4A
 *     | ...              |      3    7  Volume           [######-]   100 ----
 *     +------------------+    ...
 *
 *   Sliders are widgets in the page's UserInterface. Tab cycling
 *   between them is disabled (no_tab_stop=1) so Tab still toggles
 *   focus between Files pane / slot grid (page-level), and Up/Dn
 *   walk through slot_cur (page-level). The slider widgets are pool-
 *   allocated up to ZT_CCIZER_MAX_SLOTS (128) — positioned per-frame
 *   based on slot_top so vertical scrolling reuses the same widgets.
 *   Off-screen sliders get xsize=0 which disables their draw / mouse
 *   / keyboard paths in ValueSlider::update().
 *
 *   Mouse interaction: click anywhere on a slider's bar to set its
 *   value to that fractional position (ValueSlider's built-in
 *   mouseupdate). Drag to scrub. The page's update() detects each
 *   slider's `changed` flag after UI->update() and calls send_slot
 *   so every motion sends MIDI in real time.
 *
 *   Keys:
 *     Tab          toggle focus (Files <-> slot grid)
 *     Up/Dn        move selection within focused pane
 *     Enter        load focused file (Files pane only)
 *     Lt/Rt        adjust focused slot's value (in slot grid)
 *                    Shift = step×8, Ctrl = step×16
 *     Backspace    reset focused slot's value
 *     L            MIDI Learn for focused slot (next CC/PB binds)
 *     U            unbind learn for focused slot
 *     B            assign loaded file as current instrument's bank
 *     Shift+B      clear current instrument's bank
 *     [ / ]        adjust MIDI channel
 *     Space        re-send focused slot's current value
 *     Ctrl+S       save .cc-view + .cc-midi sidecars
 *     ESC          back to Pattern Editor (cancels Learn first if active)
 *
 *   Every key handler bumps `need_refresh++` because the main loop
 *   gates page draw() on it (see main.cpp `if (need_refresh)`). Forget
 *   the bump and the screen freezes on input.
 *
 ******/
#include "zt.h"
#include "ccizer.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  define cc_mkdir_p(p) _mkdir(p)
#else
#  include <unistd.h>
#  define cc_mkdir_p(p) mkdir(p, 0755)
#endif

// Auto-create the resolved ccizer folder so a fresh install with no
// `./ccizer` directory doesn't silently show "(empty folder)" with no
// hint about why -- mirrors the SysEx Librarian's behavior for the
// same UX consistency reason. Audit M8.
static int cc_dir_exists(const char *path) {
    if (!path || !*path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_mode & S_IFDIR) ? 1 : 0;
}
static void cc_make_dir_recursive(const char *path) {
    if (!path || !*path || cc_dir_exists(path)) return;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            if (!cc_dir_exists(buf)) cc_mkdir_p(buf);
            *p = saved;
        }
    }
    if (!cc_dir_exists(buf)) cc_mkdir_p(buf);
}

// ---------- Layout constants ----------
#define CC_BASE_Y       (TRACKS_ROW_Y + 0)

// File list pane (left)
#define CC_FILE_X       2
#define CC_FILE_Y       (CC_BASE_Y + 2)
#define CC_FILE_W       26

// Slot grid pane (right). Each column owns a fixed character span and
// is drawn with its own print() — never one packed sprintf — so a long
// name can't bleed into the slider widget and a 3-digit CC# can't
// crowd the name. The "marker+slot" field is 5 chars wide (" >123"),
// the CC# field is 4 chars wide (cc_lbl + trailing space), the name
// field is 18 chars (one extra over the 17-char strncpy so the trailing
// space is always there even on the longest names).
#define CC_GRID_X       (CC_FILE_X + CC_FILE_W + 2)   // = 30
#define CC_GRID_Y       (CC_BASE_Y + 2)
#define CC_SLOT_COL     CC_GRID_X                      // " >123" — 5 chars
#define CC_CC_COL       (CC_GRID_X + 5)                // "PB  " / "127 " — 4 chars
#define CC_NAME_COL     (CC_GRID_X + 10)               // 18-char field
#define CC_SLIDER_X     (CC_GRID_X + 28)               // ValueSlider widget x
#define CC_SLIDER_W     10                             // ValueSlider xsize
#define CC_LEARN_COL    (CC_SLIDER_X + CC_SLIDER_W + 5) // "B0 4A" or "PB ch3" or "----"

#define CC_SPACE_BOTTOM 8

// IDs for tagged UI elements. The slider pool starts at CC_SLIDER_ID_BASE
// and runs for ZT_CCIZER_MAX_SLOTS — using a high base avoids collision
// with the (currently empty) tab-stop pool.
#define CC_SLIDER_ID_BASE 100

static ZtCcizerFile g_loaded;

// --------- Helpers ---------
static void resolve_ccizer_folder(char *out, size_t out_sz) {
    out[0] = '\0';
    if (zt_ccizer_resolve_folder(zt_config_globals.ccizer_folder, ".",
                                 out, out_sz) != 0) {
        // None of the cascade entries existed. Fall back to the user
        // override path (or `./ccizer`) and create it so subsequent
        // saves work and the user can see where files should go.
        const char *fallback = (zt_config_globals.ccizer_folder[0])
                                   ? zt_config_globals.ccizer_folder
                                   : "./ccizer";
        snprintf(out, out_sz, "%s", fallback);
    }
    cc_make_dir_recursive(out);
}

static void send_slot(const ZtCcizerSlot *s, int channel) {
    if (!s) return;
    int dev = -1;
    if (cur_inst >= 0 && cur_inst < MAX_INSTS) {
        instrument *inst = song->instruments[cur_inst];
        if (inst && inst->midi_device != 0xff) {
            dev = inst->midi_device;
        }
    }
    if (dev < 0) {
        for (int i = 0; i < (int)MidiOut->numOuputDevices; i++) {
            if (MidiOut->outputDevices[i] && MidiOut->outputDevices[i]->opened) {
                dev = i; break;
            }
        }
    }
    if (dev < 0) return;
    unsigned char ch = (unsigned char)((channel - 1) & 0x0F);

    if (s->cc == ZT_CCIZER_PB_MARKER) {
        MidiOut->pitchWheel((unsigned int)dev, ch, s->value & 0x3FFF);
    } else {
        MidiOut->sendCC((unsigned int)dev, s->cc,
                        (unsigned char)(s->value & 0x7F), ch);
    }
}

static int slot_max(const ZtCcizerSlot *s) {
    return (s->cc == ZT_CCIZER_PB_MARKER) ? 16383 : 127;
}

static int slot_default(const ZtCcizerSlot *s) {
    return (s->cc == ZT_CCIZER_PB_MARKER) ? 8192 : 0;
}

// --------- Lifecycle ---------
CUI_CcConsole::CUI_CcConsole() {
    UI = new UserInterface;
    focus = 0;
    file_cur = file_top = 0;
    slot_cur = slot_top = 0;
    channel = 1;
    loaded = 0;
    learning = 0;
    status_line[0] = '\0';
    folder[0] = '\0';
    num_files = 0;
    last_visible_count = 0;
    memset(&g_loaded, 0, sizeof(g_loaded));
    memset(last_values, 0, sizeof(last_values));

    // Pre-allocate the slider pool. Each is hidden (xsize=0) until a
    // file load + position_sliders() configures it. Tab-stops disabled
    // so Tab/Up/Down keyboard nav stays page-owned.
    for (int i = 0; i < ZT_CCIZER_MAX_SLOTS; i++) {
        ValueSlider *vs = new ValueSlider(0);
        vs->no_tab_stop = 1;
        vs->frame = 0;
        vs->x = CC_SLIDER_X;
        vs->y = 0;
        vs->xsize = 0;          // hidden by default
        vs->ysize = 1;
        vs->min = 0;
        vs->max = 127;
        vs->value = 0;
        UI->add_element(vs, CC_SLIDER_ID_BASE + i);
        sliders[i] = vs;
    }
}

void CUI_CcConsole::rescan_folder(void) {
    num_files = 0;
    folder[0] = '\0';
    resolve_ccizer_folder(folder, sizeof(folder));
    if (folder[0]) {
        num_files = zt_ccizer_list_dir(folder, files, 256);
    }
    if (file_cur >= num_files) file_cur = num_files > 0 ? num_files - 1 : 0;
    if (file_top > file_cur) file_top = file_cur;
}

void CUI_CcConsole::load_by_basename(const char *bn) {
    if (!bn || !*bn) return;
    if (folder[0] == '\0') rescan_folder();
    if (folder[0] == '\0') return;
    if (loaded && strcmp(g_loaded.basename, bn) == 0) return;
    for (int i = 0; i < num_files; i++) {
        if (strcmp(files[i], bn) == 0) {
            file_cur = i;
            load_selected();
            return;
        }
    }
}

void CUI_CcConsole::load_selected(void) {
    if (file_cur < 0 || file_cur >= num_files || folder[0] == '\0') return;
    char path[1280];
    snprintf(path, sizeof(path), "%s/%s", folder, files[file_cur]);
    if (zt_ccizer_load(path, &g_loaded) == 0) {
        loaded = 1;
        slot_cur = slot_top = 0;
        zt_ccizer_set_current_file(&g_loaded);
        // Seed last_values so we don't false-trigger send_slot on the
        // first absorb pass after load.
        for (int i = 0; i < ZT_CCIZER_MAX_SLOTS; i++) {
            last_values[i] = (i < g_loaded.num_slots) ? g_loaded.slots[i].value : 0;
        }
        snprintf(status_line, sizeof(status_line),
                 "Loaded %s — %d slot(s).", g_loaded.basename, g_loaded.num_slots);
    } else {
        loaded = 0;
        zt_ccizer_set_current_file(NULL);
        snprintf(status_line, sizeof(status_line),
                 "Failed to load %s.", files[file_cur]);
    }
    need_refresh++;
}

void CUI_CcConsole::position_sliders(int grid_max_rows) {
    // Hide all sliders by default; activate the visible window.
    for (int i = 0; i < ZT_CCIZER_MAX_SLOTS; i++) {
        sliders[i]->xsize = 0;
    }
    if (!loaded || g_loaded.num_slots == 0) {
        last_visible_count = 0;
        return;
    }
    int n = g_loaded.num_slots;
    if (n > ZT_CCIZER_MAX_SLOTS) n = ZT_CCIZER_MAX_SLOTS;

    // Clamp scroll so slot_cur stays in view.
    if (slot_cur < slot_top) slot_top = slot_cur;
    if (slot_cur >= slot_top + grid_max_rows)
        slot_top = slot_cur - grid_max_rows + 1;
    if (slot_top < 0) slot_top = 0;
    if (slot_top + grid_max_rows > n) slot_top = n - grid_max_rows;
    if (slot_top < 0) slot_top = 0;

    int visible = 0;
    for (int row = 0; row < grid_max_rows; row++) {
        int idx = slot_top + row;
        if (idx >= n) break;
        ZtCcizerSlot *s = &g_loaded.slots[idx];
        ValueSlider *vs = sliders[idx];
        vs->x = CC_SLIDER_X;
        vs->y = CC_GRID_Y + row;
        vs->xsize = CC_SLIDER_W;
        vs->min = 0;
        vs->max = slot_max(s);
        vs->value = s->value;
        last_values[idx] = s->value;
        visible++;
    }
    last_visible_count = visible;
}

void CUI_CcConsole::absorb_slider_changes(void) {
    if (!loaded) return;
    int n = g_loaded.num_slots;
    if (n > ZT_CCIZER_MAX_SLOTS) n = ZT_CCIZER_MAX_SLOTS;
    for (int i = 0; i < n; i++) {
        ValueSlider *vs = sliders[i];
        if (vs->xsize == 0) continue;       // off-screen, skip
        if (vs->value == last_values[i]) continue;
        // User moved this slider (mouse drag or keyboard inside the
        // widget). Copy back to the slot and fire send.
        ZtCcizerSlot *s = &g_loaded.slots[i];
        int v = vs->value;
        if (v < 0) v = 0;
        if (v > slot_max(s)) v = slot_max(s);
        s->value = (unsigned short)v;
        last_values[i] = v;
        slot_cur = i;
        send_slot(s, channel);
        snprintf(status_line, sizeof(status_line),
                 "%s = %d  (ch %d)", s->name, v, channel);
        need_refresh++;
    }
}

void CUI_CcConsole::enter(void) {
    cur_state = STATE_CCCONSOLE;
    learning = 0;          // never enter the page mid-learn
    rescan_folder();
    if (!loaded && num_files > 0) load_selected();
    snprintf(status_line, sizeof(status_line),
             "Tab focus | Up/Dn pick | Click slider | Lt/Rt val | L learn | U unbind | B bank | [/] ch | Ctrl+S | ESC");
    Keys.flush();
    need_refresh++;
    UI->enter();
}

void CUI_CcConsole::leave(void) {
    learning = 0;
}

// --------- Update ---------
void CUI_CcConsole::update(void) {
    int total_rows    = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int grid_max_rows = total_rows - CC_GRID_Y - CC_SPACE_BOTTOM - 2;
    if (grid_max_rows < 4) grid_max_rows = 4;

    // Reposition the slider widget pool every frame so scrolling /
    // file-load / window-resize stay correct without explicit
    // invalidation calls from every key handler.
    position_sliders(grid_max_rows);

    // ----- MIDI-in pump -----
    while (midiInQueue.size() > 0) {
        int dw = midiInQueue.pop();
        unsigned char status = (unsigned char)(dw & 0xFF);
        unsigned char d1     = (unsigned char)((dw >> 8)  & 0x7F);
        unsigned char d2     = (unsigned char)((dw >> 16) & 0x7F);
        unsigned char hi     = status & 0xF0;

        if (learning && (hi == 0xB0 || hi == 0xE0) && loaded &&
            slot_cur >= 0 && slot_cur < g_loaded.num_slots) {
            g_loaded.slots[slot_cur].learn_status = status;
            g_loaded.slots[slot_cur].learn_data1  = (hi == 0xE0) ? 0 : d1;
            learning = 0;
            snprintf(status_line, sizeof(status_line),
                     "Learnt %s slot %d <- status %02X data1 %02X  (Ctrl+S to save)",
                     g_loaded.basename, slot_cur + 1, status, d1);
            need_refresh++;
            continue;
        }

        if (loaded && (hi == 0xB0 || hi == 0xE0)) {
            int match = zt_ccizer_find_learn_match(
                &g_loaded, status, (hi == 0xE0) ? 0 : d1);
            if (match >= 0) {
                ZtCcizerSlot *m = &g_loaded.slots[match];
                if (hi == 0xE0) m->value = (unsigned short)(d1 | (d2 << 7));
                else            m->value = d2;
                slot_cur = match;
                if (match < ZT_CCIZER_MAX_SLOTS) {
                    sliders[match]->value = m->value;
                    last_values[match] = m->value;
                }
                snprintf(status_line, sizeof(status_line),
                         "%s = %d  (learnt slot %d)",
                         m->name, (int)m->value, match + 1);
                need_refresh++;
            }
        }
    }

    // ----- Widget update (mouse drag / keyboard inside slider) -----
    UI->update();
    absorb_slider_changes();

    if (!Keys.size()) return;

    KBMod kstate = Keys.getstate();
    KBKey key   = Keys.getkey();

    if (key == SDLK_ESCAPE) {
        if (learning) {
            learning = 0;
            snprintf(status_line, sizeof(status_line), "Learn cancelled.");
            need_refresh++;
            return;
        }
        switch_page(UIP_Patterneditor);
        return;
    }

    if (key == SDLK_TAB) {
        focus = (focus == 0) ? 1 : 0;
        need_refresh++;
        return;
    }

    if (key == SDLK_LEFTBRACKET) {
        if (channel > 1) channel--;
        snprintf(status_line, sizeof(status_line), "Channel: %d", channel);
        need_refresh++;
        return;
    }
    if (key == SDLK_RIGHTBRACKET) {
        if (channel < 16) channel++;
        snprintf(status_line, sizeof(status_line), "Channel: %d", channel);
        need_refresh++;
        return;
    }

    if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_S) {
        if (loaded) {
            int rc1 = zt_ccizer_save_view_sidecar(&g_loaded);
            int rc2 = zt_ccizer_save_learn_sidecar(&g_loaded);
            if (rc1 == 0 && rc2 == 0) {
                snprintf(status_line, sizeof(status_line),
                         "Saved %s.cc-view + .cc-midi.", g_loaded.basename);
            } else {
                snprintf(status_line, sizeof(status_line),
                         "Failed to save sidecar (view=%d learn=%d).", rc1, rc2);
            }
        }
        need_refresh++;
        return;
    }

    // ----- File list focus -----
    if (focus == 0) {
        if (key == SDLK_UP) {
            if (file_cur > 0) file_cur--;
            need_refresh++;
            return;
        }
        if (key == SDLK_DOWN) {
            if (file_cur + 1 < num_files) file_cur++;
            need_refresh++;
            return;
        }
        if (key == SDLK_RETURN) {
            load_selected();
            focus = 1;
            need_refresh++;
            return;
        }
        return;
    }

    // ----- Slot grid focus -----
    if (!loaded || g_loaded.num_slots == 0) return;
    ZtCcizerSlot *s = &g_loaded.slots[slot_cur];

    if (key == SDLK_UP) {
        if (slot_cur > 0) slot_cur--;
        need_refresh++;
        return;
    }
    if (key == SDLK_DOWN) {
        if (slot_cur + 1 < g_loaded.num_slots) slot_cur++;
        need_refresh++;
        return;
    }
    if (key == SDLK_LEFT || key == SDLK_RIGHT) {
        int sign = (key == SDLK_LEFT) ? -1 : 1;
        int step = 1;
        if (kstate & KS_SHIFT) step = 8;
        if (kstate & KS_CTRL)  step = 16;
        int maxv = slot_max(s);
        int v = (int)s->value + sign * step;
        if (v < 0) v = 0; if (v > maxv) v = maxv;
        s->value = (unsigned short)v;
        if (slot_cur < ZT_CCIZER_MAX_SLOTS) {
            sliders[slot_cur]->value = v;
            last_values[slot_cur] = v;
        }
        send_slot(s, channel);
        snprintf(status_line, sizeof(status_line),
                 "%s = %d  (ch %d)", s->name, v, channel);
        need_refresh++;
        return;
    }
    if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
        s->value = (unsigned short)slot_default(s);
        if (slot_cur < ZT_CCIZER_MAX_SLOTS) {
            sliders[slot_cur]->value = s->value;
            last_values[slot_cur] = s->value;
        }
        send_slot(s, channel);
        snprintf(status_line, sizeof(status_line),
                 "%s reset to %d", s->name, (int)s->value);
        need_refresh++;
        return;
    }
    if (key == SDLK_L) {
        learning = !learning;
        if (learning) {
            midiInQueue.clear();
            snprintf(status_line, sizeof(status_line),
                     "LEARN: send a CC/PB for slot %d (%s)  ESC/L cancel",
                     slot_cur + 1, s->name);
        } else {
            snprintf(status_line, sizeof(status_line), "Learn cancelled.");
        }
        need_refresh++;
        return;
    }
    if (key == SDLK_U) {
        s->learn_status = 0;
        s->learn_data1  = 0;
        snprintf(status_line, sizeof(status_line),
                 "Unbound learn for slot %d  (Ctrl+S to save)", slot_cur + 1);
        need_refresh++;
        return;
    }
    if (key == SDLK_B) {
        if (cur_inst < 0 || cur_inst >= MAX_INSTS || !song->instruments[cur_inst]) {
            snprintf(status_line, sizeof(status_line), "No current instrument.");
            need_refresh++;
            return;
        }
        if (kstate & KS_SHIFT) {
            song->instruments[cur_inst]->ccizer_bank[0] = '\0';
            file_changed++;
            snprintf(status_line, sizeof(status_line),
                     "Cleared CCizer bank for inst %d.", cur_inst);
            need_refresh++;
            return;
        }
        if (loaded) {
            strncpy(song->instruments[cur_inst]->ccizer_bank,
                    g_loaded.basename,
                    sizeof(song->instruments[cur_inst]->ccizer_bank) - 1);
            song->instruments[cur_inst]->ccizer_bank[
                sizeof(song->instruments[cur_inst]->ccizer_bank) - 1] = '\0';
            file_changed++;
            snprintf(status_line, sizeof(status_line),
                     "Assigned %s as bank for inst %d.",
                     g_loaded.basename, cur_inst);
        } else {
            snprintf(status_line, sizeof(status_line),
                     "Load a file first (Enter on Files pane).");
        }
        need_refresh++;
        return;
    }
    if (key == SDLK_SPACE) {
        send_slot(s, channel);
        snprintf(status_line, sizeof(status_line),
                 "Sent %s = %d (ch %d)", s->name, (int)s->value, channel);
        need_refresh++;
        return;
    }
}

// --------- Draw ---------
void CUI_CcConsole::draw(Drawable *S) {
    if (S->lock() != 0) return;

    int total_rows    = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int grid_max_rows = total_rows - CC_GRID_Y - CC_SPACE_BOTTOM - 2;
    if (grid_max_rows < 4) grid_max_rows = 4;

    S->fillRect(col(1), row(CC_BASE_Y - 1),
                INTERNAL_RESOLUTION_X - 2,
                row(CC_GRID_Y + grid_max_rows + 2),
                COLORS.Background);

    char title[160];
    snprintf(title, sizeof(title),
             "CC Console (Shift+F3)  ch %d%s",
             channel, learning ? "  [LEARN]" : "");
    printtitle(PAGE_TITLE_ROW_Y, title, COLORS.Text, COLORS.Background, S);

    // ----- File list pane (left) -----
    print(col(CC_FILE_X), row(CC_FILE_Y - 1),
          "Files", focus == 0 ? COLORS.Brighttext : COLORS.Text, S);
    if (folder[0]) {
        char fbuf[256];
        snprintf(fbuf, sizeof(fbuf), "(%s)", folder);
        print(col(CC_FILE_X), row(CC_FILE_Y + grid_max_rows),
              fbuf, COLORS.Lowlight, S);
    }
    if (file_cur < file_top) file_top = file_cur;
    if (file_cur >= file_top + grid_max_rows)
        file_top = file_cur - grid_max_rows + 1;
    for (int i = 0; i < grid_max_rows && file_top + i < num_files; i++) {
        int idx = file_top + i;
        TColor fg = (idx == file_cur)
                        ? (focus == 0 ? COLORS.Brighttext : COLORS.Text)
                        : COLORS.Text;
        TColor bg = (idx == file_cur && focus == 0)
                        ? COLORS.SelectedBGLow
                        : COLORS.Background;
        char line[64];
        snprintf(line, sizeof(line), "%c %-24.24s",
                 (idx == file_cur) ? '>' : ' ', files[idx]);
        printBG(col(CC_FILE_X), row(CC_FILE_Y + i), line, fg, bg, S);
    }
    if (num_files == 0) {
        print(col(CC_FILE_X), row(CC_FILE_Y),
              "(empty folder)", COLORS.Lowlight, S);
    }

    // ----- Slot grid pane (right) -----
    // Column-aligned headers (one print per column) so they line up
    // with the data rows below. The slider widget shows the live
    // value itself, so the header column is just "Slider" with no
    // trailing "Val" — that text used to bleed into the Learn column.
    TColor hdr_fg = (focus == 1) ? COLORS.Brighttext : COLORS.Text;
    print(col(CC_SLOT_COL), row(CC_GRID_Y - 1), "Slot", hdr_fg, S);
    print(col(CC_CC_COL),   row(CC_GRID_Y - 1), "CC#",  hdr_fg, S);
    print(col(CC_NAME_COL), row(CC_GRID_Y - 1), "Name", hdr_fg, S);
    print(col(CC_SLIDER_X), row(CC_GRID_Y - 1), "Slider", hdr_fg, S);
    print(col(CC_LEARN_COL), row(CC_GRID_Y - 1), "Learn", hdr_fg, S);

    if (!loaded) {
        print(col(CC_GRID_X), row(CC_GRID_Y),
              "Press Enter on a file to load it.",
              COLORS.Lowlight, S);
    } else if (g_loaded.num_slots == 0) {
        print(col(CC_GRID_X), row(CC_GRID_Y),
              "No slots in this file.",
              COLORS.Lowlight, S);
    } else {
        for (int row_i = 0; row_i < grid_max_rows && slot_top + row_i < g_loaded.num_slots; row_i++) {
            int idx = slot_top + row_i;
            ZtCcizerSlot *s = &g_loaded.slots[idx];

            char cc_lbl[8];
            if (s->cc == ZT_CCIZER_PB_MARKER) snprintf(cc_lbl, sizeof(cc_lbl), "PB");
            else                              snprintf(cc_lbl, sizeof(cc_lbl), "%d", (int)s->cc);

            char learn_str[16];
            if (s->learn_status == 0) {
                snprintf(learn_str, sizeof(learn_str), "----");
            } else if ((s->learn_status & 0xF0) == 0xE0) {
                snprintf(learn_str, sizeof(learn_str), "PB ch%d",
                         (s->learn_status & 0x0F) + 1);
            } else {
                snprintf(learn_str, sizeof(learn_str), "%02X %02X",
                         s->learn_status, s->learn_data1);
            }

            TColor fg = (idx == slot_cur)
                            ? (focus == 1 ? COLORS.Brighttext : COLORS.Text)
                            : COLORS.Text;
            TColor bg = (idx == slot_cur && focus == 1)
                            ? COLORS.SelectedBGLow
                            : COLORS.Background;

            // Each column rendered as its own printBG with a fixed
            // width so adjacent columns can't run into each other.
            // Slot field: " >123" — marker + 3-digit slot index +
            // trailing space (5 chars total). CC field: "PB  " /
            // "127 " — left-justified 3-char label + trailing space
            // (4 chars total). Name field: 18 chars; the
            // ZtCcizerSlot::name buffer is 17 long so a 17-char slot
            // name still has one cell of breathing room before the
            // slider widget at CC_SLIDER_X.
            char slot_field[8];
            snprintf(slot_field, sizeof(slot_field), "%c%3d ",
                     (idx == slot_cur) ? '>' : ' ', idx + 1);
            printBG(col(CC_SLOT_COL), row(CC_GRID_Y + row_i),
                    slot_field, fg, bg, S);

            char cc_field[8];
            snprintf(cc_field, sizeof(cc_field), "%-3s ", cc_lbl);
            printBG(col(CC_CC_COL), row(CC_GRID_Y + row_i),
                    cc_field, fg, bg, S);

            char name_buf[20];
            snprintf(name_buf, sizeof(name_buf), "%-18.18s", s->name);
            printBG(col(CC_NAME_COL), row(CC_GRID_Y + row_i),
                    name_buf, fg, bg, S);

            // The ValueSlider widget itself is drawn by UI->draw(S).

            print(col(CC_LEARN_COL), row(CC_GRID_Y + row_i),
                  learn_str, COLORS.Text, S);
        }
    }

    // ----- Status line -----
    print(col(2), row(CC_GRID_Y + grid_max_rows + 1),
          status_line, COLORS.Lowlight, S);

    // Slider widgets paint here.
    UI->draw(S);

    S->unlock();
}
