/*************************************************************************
 *
 * FILE  CUI_CcConsole.cpp
 *
 * DESCRIPTION
 *   CC Console (Shift+F3). Loads Paketti CCizer-format `.txt` files from
 *   the configured ccizer_folder and lets the user send MIDI Control
 *   Change (and Pitch Bend) messages out to the current MIDI Out device
 *   via on-screen sliders or knobs.
 *
 *   Layout:
 *     +-- Files ----------+  +-- <basename> -------------------------+
 *     | a.txt             |  |  Slot  CC   Name        View   Value |
 *     | b.txt             |  |   1    PB   Pitchbend   slider  64   |
 *     |▶c.txt             |  |   2    1    Mod         knob    23   |
 *     | ...               |  |   3    7    Volume      slider  100  |
 *     +-------------------+  +---------------------------------------+
 *
 *   Keys:
 *     Tab          toggle focus (file list <-> slot grid)
 *     Up/Down      move selection
 *     Enter        load focused file
 *     Left/Right   adjust value (Shift = ×8, Ctrl = ×16)
 *     V            toggle slider <-> knob for focused slot
 *     [ / ]        decrement / increment MIDI channel
 *     Backspace    reset focused slot value to default
 *     Ctrl+S       save .cc-view sidecar
 *     ESC          back to Pattern Editor
 *
 *   The .cc-view sidecar persists slider/knob choices next to the .txt
 *   so the Paketti file itself stays untouched.
 *
 *   Pattern-drawing, per-instrument bank assignment, and CC MIDI Learn
 *   are deferred -- see docs/plans/cc-followup-todo.md.
 *
 ******/
#include "zt.h"
#include "ccizer.h"

#include <stdio.h>
#include <string.h>

// --------- Layout constants ---------
#define CC_BASE_Y       (TRACKS_ROW_Y + 0)
#define CC_FILE_X       2
#define CC_FILE_Y       (CC_BASE_Y + 2)
#define CC_FILE_W       24
#define CC_GRID_X       (CC_FILE_X + CC_FILE_W + 2)
#define CC_GRID_Y       (CC_BASE_Y + 2)
#define CC_GRID_W_COLS  46
#define CC_BAR_W        12
#define CC_SPACE_BOTTOM 8

static ZtCcizerFile g_loaded;

// --------- Helpers ---------
static void resolve_ccizer_folder(char *out, size_t out_sz) {
    out[0] = '\0';
    zt_ccizer_resolve_folder(zt_config_globals.ccizer_folder, ".", out, out_sz);
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
        // Fall back to first opened device.
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

// Build a textual slider bar, e.g. "[####------]"
static void render_bar(char *buf, size_t bsz, int val, int maxv) {
    if (maxv <= 0) maxv = 1;
    int w = CC_BAR_W;
    int filled = (val * w) / maxv;
    if (filled < 0) filled = 0;
    if (filled > w) filled = w;
    if ((int)bsz < w + 3) { buf[0] = '\0'; return; }
    buf[0] = '[';
    for (int i = 0; i < w; i++) buf[1 + i] = (i < filled) ? '#' : '-';
    buf[1 + w] = ']';
    buf[2 + w] = '\0';
}

// Build a 2-char "knob" representation.  Eight detents drawn as one of
// "<.", "<:", "<|", ":|", "|>", "|:", ":>", ".>".
static void render_knob(char *buf, size_t bsz, int val, int maxv) {
    static const char *frames[8] = {
        "<.", "<:", "<|", ":|", "|>", "|:", ":>", ".>"
    };
    if (maxv <= 0) maxv = 1;
    int idx = (val * 8) / (maxv + 1);
    if (idx < 0) idx = 0; if (idx > 7) idx = 7;
    snprintf(buf, bsz, "%s", frames[idx]);
}

// --------- Lifecycle ---------
CUI_CcConsole::CUI_CcConsole() {
    UI = new UserInterface;
    focus = 0;
    file_cur = file_top = 0;
    slot_cur = slot_top = 0;
    channel = 1;
    loaded = 0;
    status_line[0] = '\0';
    folder[0] = '\0';
    num_files = 0;
    memset(&g_loaded, 0, sizeof(g_loaded));
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

void CUI_CcConsole::load_selected(void) {
    if (file_cur < 0 || file_cur >= num_files || folder[0] == '\0') return;
    char path[1280];
    snprintf(path, sizeof(path), "%s/%s", folder, files[file_cur]);
    if (zt_ccizer_load(path, &g_loaded) == 0) {
        loaded = 1;
        slot_cur = slot_top = 0;
        snprintf(status_line, sizeof(status_line),
                 "Loaded %s — %d slot(s).", g_loaded.basename, g_loaded.num_slots);
    } else {
        loaded = 0;
        snprintf(status_line, sizeof(status_line),
                 "Failed to load %s.", files[file_cur]);
    }
}

void CUI_CcConsole::enter(void) {
    cur_state = STATE_CCCONSOLE;
    rescan_folder();
    if (!loaded && num_files > 0) load_selected();
    snprintf(status_line, sizeof(status_line),
             "Tab: focus  Up/Dn: pick  Enter: load  Left/Right: value  V: slider/knob  [/]: ch  Ctrl+S: save  ESC: exit");
    Keys.flush();
}

void CUI_CcConsole::leave(void) {
}

// --------- Update ---------
void CUI_CcConsole::update(void) {
    if (!Keys.size()) return;

    KBMod  kstate = Keys.getstate();
    KBKey  key    = Keys.getkey();

    if (key == SDLK_ESCAPE) {
        switch_page(UIP_Patterneditor);
        return;
    }

    if (key == SDLK_TAB) {
        focus = (focus == 0) ? 1 : 0;
        return;
    }

    if (key == SDLK_LEFTBRACKET) {
        if (channel > 1) channel--;
        snprintf(status_line, sizeof(status_line), "Channel: %d", channel);
        return;
    }
    if (key == SDLK_RIGHTBRACKET) {
        if (channel < 16) channel++;
        snprintf(status_line, sizeof(status_line), "Channel: %d", channel);
        return;
    }

    if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_S) {
        if (loaded) {
            int rc = zt_ccizer_save_view_sidecar(&g_loaded);
            if (rc == 0) {
                snprintf(status_line, sizeof(status_line),
                         "Saved %s.cc-view.", g_loaded.basename);
            } else {
                snprintf(status_line, sizeof(status_line),
                         "Failed to save sidecar (%d).", rc);
            }
        }
        return;
    }

    // ----- File list focus -----
    if (focus == 0) {
        if (key == SDLK_UP) {
            if (file_cur > 0) file_cur--;
            return;
        }
        if (key == SDLK_DOWN) {
            if (file_cur + 1 < num_files) file_cur++;
            return;
        }
        if (key == SDLK_RETURN) {
            load_selected();
            focus = 1;
            return;
        }
        return;
    }

    // ----- Slot grid focus -----
    if (!loaded || g_loaded.num_slots == 0) return;
    ZtCcizerSlot *s = &g_loaded.slots[slot_cur];

    if (key == SDLK_UP) {
        if (slot_cur > 0) slot_cur--;
        return;
    }
    if (key == SDLK_DOWN) {
        if (slot_cur + 1 < g_loaded.num_slots) slot_cur++;
        return;
    }
    if (key == SDLK_LEFT || key == SDLK_RIGHT) {
        int sign  = (key == SDLK_LEFT) ? -1 : 1;
        int step  = 1;
        if (kstate & KS_SHIFT) step = 8;
        if (kstate & KS_CTRL)  step = 16;
        int maxv  = (s->cc == ZT_CCIZER_PB_MARKER) ? 16383 : 127;
        int v     = (int)s->value + sign * step;
        if (v < 0) v = 0; if (v > maxv) v = maxv;
        s->value = (unsigned short)v;
        send_slot(s, channel);
        snprintf(status_line, sizeof(status_line),
                 "%s = %d  (ch %d)", s->name, v, channel);
        return;
    }
    if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
        s->value = (s->cc == ZT_CCIZER_PB_MARKER) ? 8192 : 0;
        send_slot(s, channel);
        snprintf(status_line, sizeof(status_line),
                 "%s reset to %d", s->name, (int)s->value);
        return;
    }
    if (key == SDLK_V) {
        s->view = (s->view == ZT_CCIZER_VIEW_KNOB)
                     ? ZT_CCIZER_VIEW_SLIDER
                     : ZT_CCIZER_VIEW_KNOB;
        snprintf(status_line, sizeof(status_line),
                 "%s view = %s  (Ctrl+S to save)",
                 s->name,
                 s->view == ZT_CCIZER_VIEW_KNOB ? "knob" : "slider");
        return;
    }
    // SPACE re-fires the current value (useful for testing wiring).
    if (key == SDLK_SPACE) {
        send_slot(s, channel);
        snprintf(status_line, sizeof(status_line),
                 "Sent %s = %d (ch %d)", s->name, (int)s->value, channel);
        return;
    }
}

// --------- Draw ---------
void CUI_CcConsole::draw(Drawable *S) {
    if (S->lock() != 0) return;

    int total_rows = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int grid_max_rows = total_rows - CC_GRID_Y - CC_SPACE_BOTTOM - 2;
    if (grid_max_rows < 4) grid_max_rows = 4;
    int file_max_rows = grid_max_rows;

    S->fillRect(col(1), row(CC_BASE_Y - 1),
                INTERNAL_RESOLUTION_X - 2,
                row(CC_GRID_Y + grid_max_rows + 2),
                COLORS.Background);

    char title[128];
    snprintf(title, sizeof(title),
             "CC Console (Shift+F3)  ch %d%s",
             channel, loaded ? "" : " — no file loaded");
    printtitle(PAGE_TITLE_ROW_Y, title, COLORS.Text, COLORS.Background, S);

    // ----- File list pane -----
    print(col(CC_FILE_X), row(CC_FILE_Y - 1),
          "Files", focus == 0 ? COLORS.Brighttext : COLORS.Text, S);
    if (folder[0]) {
        char fbuf[256];
        snprintf(fbuf, sizeof(fbuf), "(%s)", folder);
        print(col(CC_FILE_X), row(CC_FILE_Y + file_max_rows),
              fbuf, COLORS.Lowlight, S);
    } else {
        print(col(CC_FILE_X), row(CC_FILE_Y + file_max_rows),
              "(set ccizer_folder in zt.conf or place files in ./ccizer)",
              COLORS.Lowlight, S);
    }

    if (file_cur < file_top) file_top = file_cur;
    if (file_cur >= file_top + file_max_rows)
        file_top = file_cur - file_max_rows + 1;

    for (int i = 0; i < file_max_rows && file_top + i < num_files; i++) {
        int idx = file_top + i;
        TColor fg = (idx == file_cur)
                        ? (focus == 0 ? COLORS.Brighttext : COLORS.Text)
                        : COLORS.Text;
        TColor bg = (idx == file_cur && focus == 0)
                        ? COLORS.SelectedBGLow
                        : COLORS.Background;
        char line[64];
        snprintf(line, sizeof(line), "%c %-22.22s",
                 (idx == file_cur) ? '>' : ' ', files[idx]);
        printBG(col(CC_FILE_X), row(CC_FILE_Y + i), line, fg, bg, S);
    }
    if (num_files == 0) {
        print(col(CC_FILE_X), row(CC_FILE_Y),
              "(empty folder)", COLORS.Lowlight, S);
    }

    // ----- Slot grid pane -----
    print(col(CC_GRID_X), row(CC_GRID_Y - 1),
          "Slot CC#  Name              View  Value",
          focus == 1 ? COLORS.Brighttext : COLORS.Text, S);

    if (!loaded) {
        print(col(CC_GRID_X), row(CC_GRID_Y),
              "Press Enter on a file to load it.",
              COLORS.Lowlight, S);
    } else if (g_loaded.num_slots == 0) {
        print(col(CC_GRID_X), row(CC_GRID_Y),
              "No slots in this file.",
              COLORS.Lowlight, S);
    } else {
        if (slot_cur < slot_top) slot_top = slot_cur;
        if (slot_cur >= slot_top + grid_max_rows)
            slot_top = slot_cur - grid_max_rows + 1;

        for (int i = 0; i < grid_max_rows && slot_top + i < g_loaded.num_slots; i++) {
            int idx = slot_top + i;
            ZtCcizerSlot *s = &g_loaded.slots[idx];

            char cc_lbl[8];
            if (s->cc == ZT_CCIZER_PB_MARKER) snprintf(cc_lbl, sizeof(cc_lbl), "PB");
            else                              snprintf(cc_lbl, sizeof(cc_lbl), "%d", (int)s->cc);

            char view_str[16];
            int  maxv = (s->cc == ZT_CCIZER_PB_MARKER) ? 16383 : 127;
            if (s->view == ZT_CCIZER_VIEW_KNOB) {
                char knob[8];
                render_knob(knob, sizeof(knob), s->value, maxv);
                snprintf(view_str, sizeof(view_str), "knob %s", knob);
            } else {
                char bar[CC_BAR_W + 4];
                render_bar(bar, sizeof(bar), s->value, maxv);
                snprintf(view_str, sizeof(view_str), "%s", bar);
            }

            char line[160];
            snprintf(line, sizeof(line),
                     "%c %3d  %-3s  %-16.16s %-15s %5d",
                     (idx == slot_cur) ? '>' : ' ',
                     idx + 1, cc_lbl, s->name, view_str, (int)s->value);

            TColor fg = (idx == slot_cur)
                            ? (focus == 1 ? COLORS.Brighttext : COLORS.Text)
                            : COLORS.Text;
            TColor bg = (idx == slot_cur && focus == 1)
                            ? COLORS.SelectedBGLow
                            : COLORS.Background;
            printBG(col(CC_GRID_X), row(CC_GRID_Y + i), line, fg, bg, S);
        }
    }

    // ----- Status line -----
    print(col(2), row(CC_GRID_Y + grid_max_rows + 1),
          status_line, COLORS.Lowlight, S);

    S->unlock();
}
