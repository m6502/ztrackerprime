/*************************************************************************
 *
 * FILE  CUI_CCEnvelopeEditor.cpp
 *
 * DESCRIPTION
 *   CC Envelope Editor (Shift+F6).
 *
 *   The editor is per-instrument. Each instrument owns up to
 *   ZTM_INST_MAX_ENVELOPES (32) envelope curves. If the instrument
 *   has a CCizer bank loaded (instrument.ccizer_bank), the slot
 *   picker shows the bank's NAMED CC slots (Brightness, Cutoff,
 *   Resonance, ...) and selecting one focuses that slot's curve
 *   for editing. If the instrument has no bank, the slot picker
 *   shows generic "Slot N" rows with a raw CC# field.
 *
 *   Slot picker (left) + big canvas (right). Click in the canvas to
 *   add a node; drag to position; right-click to delete; arrow keys
 *   nav; L/S mark loop/sustain anchors; E toggles enabled; T test-
 *   fires the envelope on cur_inst so you can watch the playhead.
 *
 *   Notes (pattern or keyjazz) auto-arm every enabled envelope on
 *   cur_inst -- the instrument IS the binding.
 *
 ******/
#include "zt.h"

#include "ccenv_io.h"
#include "ccenv_interp.h"
#include "editor_layout.h"
#include "Button.h"
#include "Sliders.h"
#include "ccizer.h"

#if defined(_WIN32) && !defined(strcasecmp)
  #define strcasecmp _stricmp
#endif

extern int cur_inst;

#define BASE_Y          (TRACKS_ROW_Y + 0)
#define SPACE_AT_BOTTOM 8

// Widget IDs.
enum {
    W_SLOT_LIST = 0,
    W_CANVAS,
    W_CC_SLIDER,
    W_KIND_SLIDER,
    W_SPEED_SLIDER,
    W_FLAG_ENABLED,
    W_FLAG_LOOP,
    W_FLAG_SUSTAIN,
    W_FLAG_CARRY,
    W_FILE_LIST,
    W_FILENAME,
    W_BTN_LOAD,
    W_BTN_SAVE
};

static int  ce_slot       = 0;          // which envelope-slot of cur_inst is focused
static int  ce_sel_node   = -1;         // selected node in the focused envelope
static int  ce_last_inst  = -1;         // detect cur_inst changes
static int  ce_dragging   = 0;
static char ce_status[160] = {0};
static char ce_filename_buf[256] = {0};
static char ce_folder[1024] = {0};
static char ce_files[256][256];
static int  ce_num_files = 0;

// Cached CCizer bank for cur_inst (loaded on demand).
static ZtCcizerFile g_inst_bank;
static int          g_inst_bank_valid = 0;
static char         g_inst_bank_for[256] = {0};   // basename it was loaded from

// CheckBox backing ints (CheckBox::value is `int *`).
static int ce_flag_ena   = 0;
static int ce_flag_loop  = 0;
static int ce_flag_sus   = 0;
static int ce_flag_carry = 0;

static UserInterface *ce_page_ui = NULL;

// Forward declarations.
static void ce_pull_to_widgets(UserInterface *UI);
static void ce_load_inst_bank_if_needed(void);

static void ce_set_status(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(ce_status, sizeof ce_status, fmt, ap);
    va_end(ap);
}

static instrument *ce_inst(void) {
    if (cur_inst < 0 || cur_inst >= MAX_INSTS) return NULL;
    return song->instruments[cur_inst];
}

static inst_envelope *ce_env(void) {
    instrument *ii = ce_inst();
    if (!ii) return NULL;
    if (ce_slot < 0) ce_slot = 0;
    if (ce_slot >= ZTM_INST_MAX_ENVELOPES) ce_slot = ZTM_INST_MAX_ENVELOPES - 1;
    return &ii->envelopes[ce_slot];
}

// Force every widget + page refresh, like Cmd-Tab triggers via
// zt_request_ui_full_refresh. Called from every state-mutating
// handler so input always paints same frame.
static void ce_force_refresh(void) {
    if (ce_page_ui) ce_page_ui->full_refresh();
    doredraw++;
    need_refresh++;
    screenmanager.UpdateAll();
}

static void ce_load_inst_bank_if_needed(void) {
    instrument *ii = ce_inst();
    if (!ii) { g_inst_bank_valid = 0; g_inst_bank_for[0] = 0; return; }
    if (g_inst_bank_valid && strcmp(g_inst_bank_for, ii->ccizer_bank) == 0) {
        return;   // already loaded
    }
    g_inst_bank_valid = 0;
    g_inst_bank_for[0] = 0;
    if (!ii->ccizer_bank[0]) return;

    // Resolve <ccizer_folder>/<basename>.
    char folder[1024];
    if (zt_config_globals.ccizer_folder[0]) {
        strncpy(folder, zt_config_globals.ccizer_folder, sizeof folder - 1);
    } else if (zt_directory[0]) {
        snprintf(folder, sizeof folder, "%s/ccizer", zt_directory);
    } else {
        strncpy(folder, "./ccizer", sizeof folder - 1);
    }
    folder[sizeof folder - 1] = 0;
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", folder, ii->ccizer_bank);
    if (zt_ccizer_load(path, &g_inst_bank) == 0) {
        g_inst_bank_valid = 1;
        strncpy(g_inst_bank_for, ii->ccizer_bank, sizeof g_inst_bank_for - 1);
    }
}

static void ce_rescan_folder(void) {
    ce_num_files = 0;
    if (ccenv_resolve_folder(ce_folder, sizeof ce_folder) >= -1) {
        ce_num_files = ccenv_scan_folder(ce_folder, ce_files,
                                         (int)(sizeof ce_files / sizeof ce_files[0]));
    }
}

// ---------------------------------------------------------------------------
// Slot picker: ListBox showing one row per envelope slot of cur_inst.
// If the instrument has a CCizer bank, row label is the named CC
// (e.g. "Brightness CC#74"); otherwise generic "Slot N CC#X".
// `*` marker on rows with num_nodes > 0.

namespace {
class CeSlotList : public ListBox {
public:
    CeSlotList() {
        empty_message = "No slots";
        is_sorted = false;
        use_checks = false;
        use_key_select = true;
    }
    void OnChange() override {
        clear();
        instrument *ii = ce_inst();
        if (!ii) return;
        ce_load_inst_bank_if_needed();
        // Two display modes: CCizer-bank labels OR plain slots.
        int n_show = ZTM_INST_MAX_ENVELOPES;
        if (g_inst_bank_valid && g_inst_bank.num_slots > 0
            && g_inst_bank.num_slots < n_show) {
            // Show only as many envelope rows as the bank has slots.
            n_show = g_inst_bank.num_slots;
        }
        // insertItem prepends in an unsorted list -- walk backwards
        // so visual order matches the array order.
        for (int s = n_show - 1; s >= 0; s--) {
            char line[96];
            const inst_envelope &env = ii->envelopes[s];
            char mark = env.num_nodes > 0 ? '*' : ' ';
            if (g_inst_bank_valid && s < g_inst_bank.num_slots) {
                const ZtCcizerSlot &cs = g_inst_bank.slots[s];
                snprintf(line, sizeof line, "%c %02d %-16.16s CC#%u",
                         mark, s, cs.name, (unsigned)cs.cc);
            } else {
                snprintf(line, sizeof line, "%c Slot %02d  CC#%u",
                         mark, s,
                         env.cc == ZTM_INSTENV_CC_NONE ? 0u : (unsigned)env.cc);
            }
            insertItem(line);
        }
    }
    void OnSelectChange() override {
        int idx = cur_sel + y_start;
        // Items were inserted in reverse so index N visually == array index N.
        if (idx < 0 || idx >= ZTM_INST_MAX_ENVELOPES) return;
        if (idx != ce_slot) {
            ce_slot = idx;
            ce_sel_node = -1;
            if (ce_page_ui) ce_pull_to_widgets(ce_page_ui);
            ce_force_refresh();
            ce_set_status("Focused slot %d", ce_slot);
        }
    }
    void OnSelect(LBNode *p) override {
        (void)p;
        OnSelectChange();
    }
};

// ---------------------------------------------------------------------------
// The big drawing canvas.

class EnvCanvas : public UserInterfaceElement {
public:
    int max_tick_view() const {
        inst_envelope *e = ce_env();
        if (!e || e->num_nodes == 0) return 64;
        int t = (int)e->tick[e->num_nodes - 1];
        int v = t + (t / 4);
        if (v < 64) v = 64;
        if (v > 65535) v = 65535;
        return v;
    }
    int pix_to_tick(int px) const {
        int rel = px - col(this->x);
        int span = col(this->xsize);
        if (span <= 0) return 0;
        int mt = max_tick_view();
        int t = (rel * mt) / span;
        if (t < 0) t = 0; if (t > 65535) t = 65535;
        return t;
    }
    int pix_to_value(int py) const {
        int rel = py - row(this->y);
        int span = row(this->ysize);
        if (span <= 0) return 0;
        int v = 127 - (rel * 127) / span;
        if (v < 0) v = 0; if (v > 127) v = 127;
        return v;
    }
    int node_x(int n) const {
        inst_envelope *e = ce_env();
        if (!e) return col(this->x);
        int mt = max_tick_view();
        int span = col(this->xsize);
        return col(this->x) + ((int)e->tick[n] * span) / (mt > 0 ? mt : 1);
    }
    int node_y(int n) const {
        inst_envelope *e = ce_env();
        if (!e) return row(this->y);
        int span = row(this->ysize);
        return row(this->y) + ((127 - (int)e->value[n]) * span) / 127;
    }
    int find_node_near(int px, int py) const {
        inst_envelope *e = ce_env();
        if (!e || e->num_nodes == 0) return -1;
        int best = -1, best_dsq = 6 * 6;
        for (int i = 0; i < e->num_nodes; i++) {
            int dx = px - node_x(i);
            int dy = py - node_y(i);
            int dsq = dx*dx + dy*dy;
            if (dsq < best_dsq) { best = i; best_dsq = dsq; }
        }
        return best;
    }
    int hit_canvas(int px, int py) const {
        return px >= col(this->x) && px < col(this->x + this->xsize)
            && py >= row(this->y) && py < row(this->y + this->ysize);
    }

    static int sort_after_move(inst_envelope *e, int idx) {
        if (!e || idx < 0 || idx >= e->num_nodes) return idx;
        while (idx > 0 && e->tick[idx - 1] > e->tick[idx]) {
            unsigned short t = e->tick[idx]; e->tick[idx] = e->tick[idx-1]; e->tick[idx-1] = t;
            unsigned char  v = e->value[idx]; e->value[idx] = e->value[idx-1]; e->value[idx-1] = v;
            idx--;
        }
        while (idx + 1 < e->num_nodes && e->tick[idx] > e->tick[idx + 1]) {
            unsigned short t = e->tick[idx]; e->tick[idx] = e->tick[idx+1]; e->tick[idx+1] = t;
            unsigned char  v = e->value[idx]; e->value[idx] = e->value[idx+1]; e->value[idx+1] = v;
            idx++;
        }
        return idx;
    }

    int mouseupdate(int cur_element) override {
        KBKey key = Keys.checkkey();
        inst_envelope *e = ce_env();
        if (!e) return cur_element;

        if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT))) {
            if (hit_canvas(MousePressX, MousePressY)) {
                int hit = find_node_near(MousePressX, MousePressY);
                if (hit >= 0) {
                    ce_sel_node = hit;
                    ce_dragging = 1;
                    ce_set_status("Selected node %d", hit);
                } else if (e->num_nodes < ZTM_INST_ENV_MAX_NODES) {
                    int t = pix_to_tick(MousePressX);
                    int v = pix_to_value(MousePressY);
                    e->tick[e->num_nodes]  = (unsigned short)t;
                    e->value[e->num_nodes] = (unsigned char)v;
                    e->num_nodes++;
                    ce_sel_node = sort_after_move(e, e->num_nodes - 1);
                    ce_dragging = 1;        // create-and-drag in one motion
                    if (!(e->flags & ZTM_INSTENVF_ENABLED)) {
                        e->flags |= ZTM_INSTENVF_ENABLED;
                        ce_flag_ena = 1;
                    }
                    file_changed++;
                    ce_set_status("Added node %d at tick=%d value=%d (drag to move)",
                                  ce_sel_node, t, v);
                }
                Keys.getkey();
                this->need_redraw++;
                ce_force_refresh();
                return this->ID;
            }
        }
        if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT))) {
            if (hit_canvas(MousePressX, MousePressY)) {
                int hit = find_node_near(MousePressX, MousePressY);
                if (hit >= 0) {
                    for (int k = hit; k + 1 < e->num_nodes; k++) {
                        e->tick[k]  = e->tick[k+1];
                        e->value[k] = e->value[k+1];
                    }
                    e->num_nodes--;
                    if (ce_sel_node >= e->num_nodes) ce_sel_node = e->num_nodes - 1;
                    file_changed++;
                    ce_set_status("Deleted node %d", hit);
                }
                Keys.getkey();
                this->need_redraw++;
                ce_force_refresh();
                return this->ID;
            }
        }
        if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT))) {
            // Consume ONLY if the press was inside us OR we were
            // dragging -- never steal another widget's button-up.
            if (ce_dragging || hit_canvas(MousePressX, MousePressY)) {
                Keys.getkey();
                if (ce_dragging) {
                    ce_dragging = 0;
                    this->need_redraw++;
                    ce_force_refresh();
                    return this->ID;
                }
            }
        }
        if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_RIGHT))) {
            if (hit_canvas(MousePressX, MousePressY)) Keys.getkey();
        }
        // Live drag.
        if (ce_dragging && ce_sel_node >= 0 && ce_sel_node < e->num_nodes
            && hit_canvas(LastX, LastY)) {
            int new_t = pix_to_tick(LastX);
            int new_v = pix_to_value(LastY);
            if (new_t != e->tick[ce_sel_node] || new_v != e->value[ce_sel_node]) {
                e->tick[ce_sel_node]  = (unsigned short)new_t;
                e->value[ce_sel_node] = (unsigned char)new_v;
                ce_sel_node = sort_after_move(e, ce_sel_node);
                file_changed++;
                this->need_redraw++;
                ce_force_refresh();
            }
        }
        return cur_element;
    }

    int update() override {
        KBKey k = Keys.checkkey();
        KBMod ks = Keys.cur_state;
        if (!k) return 0;
        inst_envelope *e = ce_env();
        if (!e) return 0;
        int nn = e->num_nodes;
        int maxidx = nn > 0 ? nn - 1 : 0;
        int consumed = 0;

        if (k == SDLK_LEFT) {
            if (ks & KS_SHIFT) {
                if (ce_sel_node >= 0 && ce_sel_node < nn && e->tick[ce_sel_node] > 0) {
                    e->tick[ce_sel_node]--;
                    ce_sel_node = sort_after_move(e, ce_sel_node);
                    file_changed++;
                }
            } else if (ce_sel_node > 0) ce_sel_node--;
            else if (nn > 0) ce_sel_node = 0;
            consumed = 1;
        } else if (k == SDLK_RIGHT) {
            if (ks & KS_SHIFT) {
                if (ce_sel_node >= 0 && ce_sel_node < nn && e->tick[ce_sel_node] < 65535) {
                    e->tick[ce_sel_node]++;
                    ce_sel_node = sort_after_move(e, ce_sel_node);
                    file_changed++;
                }
            } else if (ce_sel_node < maxidx) ce_sel_node++;
            else if (nn > 0) ce_sel_node = maxidx;
            consumed = 1;
        } else if (k == SDLK_UP) {
            if (ce_sel_node >= 0 && ce_sel_node < nn) {
                int delta = (ks & KS_SHIFT) ? 8 : 1;
                int v = (int)e->value[ce_sel_node] + delta;
                if (v > 127) v = 127;
                e->value[ce_sel_node] = (unsigned char)v;
                file_changed++;
            }
            consumed = 1;
        } else if (k == SDLK_DOWN) {
            if (ce_sel_node >= 0 && ce_sel_node < nn) {
                int delta = (ks & KS_SHIFT) ? 8 : 1;
                int v = (int)e->value[ce_sel_node] - delta;
                if (v < 0) v = 0;
                e->value[ce_sel_node] = (unsigned char)v;
                file_changed++;
            }
            consumed = 1;
        } else if (k == SDLK_DELETE || k == SDLK_BACKSPACE) {
            if (ce_sel_node >= 0 && ce_sel_node < nn) {
                for (int j = ce_sel_node; j + 1 < nn; j++) {
                    e->tick[j]  = e->tick[j+1];
                    e->value[j] = e->value[j+1];
                }
                e->num_nodes--;
                if (ce_sel_node >= e->num_nodes) ce_sel_node = e->num_nodes - 1;
                file_changed++;
                ce_set_status("Deleted node");
            }
            consumed = 1;
        } else if (k == SDLK_SPACE) {
            // Insert mid-segment after selected.
            if (nn > 0 && nn < ZTM_INST_ENV_MAX_NODES && ce_sel_node >= 0) {
                int ins = ce_sel_node + 1;
                if (ins > nn) ins = nn;
                for (int j = nn; j > ins; j--) {
                    e->tick[j]  = e->tick[j-1];
                    e->value[j] = e->value[j-1];
                }
                int t0 = ins > 0 ? (int)e->tick[ins-1] : 0;
                int t1 = ins < nn ? (int)e->tick[ins] : t0 + 16;
                int v0 = ins > 0 ? (int)e->value[ins-1] : 0;
                int v1 = ins < nn ? (int)e->value[ins] : v0;
                e->tick[ins]  = (unsigned short)((t0 + t1) / 2);
                e->value[ins] = (unsigned char)((v0 + v1) / 2);
                e->num_nodes++;
                ce_sel_node = ins;
                file_changed++;
                ce_set_status("Inserted node at %d", ins);
            } else if (nn == 0) {
                e->tick[0] = 0; e->value[0] = 64;
                e->num_nodes = 1;
                ce_sel_node = 0;
                file_changed++;
                ce_set_status("First node added (tick 0, value 64)");
            }
            consumed = 1;
        } else if (k == SDLK_L && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            if (ce_sel_node >= 0 && ce_sel_node < nn) {
                if (e->loop_start == ce_sel_node && e->loop_end != ce_sel_node) {
                    e->loop_end = (unsigned char)ce_sel_node;
                } else {
                    e->loop_start = (unsigned char)ce_sel_node;
                    if (e->loop_end < e->loop_start) e->loop_end = (unsigned char)ce_sel_node;
                }
                e->flags |= ZTM_INSTENVF_LOOP;
                ce_flag_loop = 1;
                file_changed++;
                ce_set_status("Loop set: %u..%u", e->loop_start, e->loop_end);
            }
            consumed = 1;
        } else if (k == SDLK_S && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            if (ce_sel_node >= 0 && ce_sel_node < nn) {
                if (e->sustain_start == ce_sel_node && e->sustain_end != ce_sel_node) {
                    e->sustain_end = (unsigned char)ce_sel_node;
                } else {
                    e->sustain_start = (unsigned char)ce_sel_node;
                    if (e->sustain_end < e->sustain_start)
                        e->sustain_end = (unsigned char)ce_sel_node;
                }
                e->flags |= ZTM_INSTENVF_SUSTAIN;
                ce_flag_sus = 1;
                file_changed++;
                ce_set_status("Sustain set: %u..%u", e->sustain_start, e->sustain_end);
            }
            consumed = 1;
        } else if (k == SDLK_E && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            e->flags ^= ZTM_INSTENVF_ENABLED;
            ce_flag_ena = (e->flags & ZTM_INSTENVF_ENABLED) ? 1 : 0;
            file_changed++;
            ce_set_status("Enabled: %s", ce_flag_ena ? "on" : "off");
            consumed = 1;
        } else if (k == SDLK_T && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            zt_audition_env_arm_one(cur_inst, ce_slot, 0);
            ce_set_status("Test-fired envelope on inst %d slot %d", cur_inst, ce_slot);
            consumed = 1;
        }
        if (consumed) {
            Keys.getkey();
            this->need_redraw++;
            ce_force_refresh();
        }
        return 0;
    }

    static void draw_line_1px(Drawable *S, int ax, int ay, int bx, int by, TColor c) {
        int dx = bx - ax, dy = by - ay;
        int adx = dx < 0 ? -dx : dx;
        int ady = dy < 0 ? -dy : dy;
        int steps = adx > ady ? adx : ady;
        if (steps <= 0) { S->fillRect(ax, ay, ax, ay, c); return; }
        for (int s = 0; s <= steps; s++) {
            int x = ax + (dx * s) / steps;
            int y = ay + (dy * s) / steps;
            S->fillRect(x, y, x, y, c);
        }
    }

    void draw(Drawable *S, int active) override {
        inst_envelope *e = ce_env();
        int x0 = col(this->x);
        int y0 = row(this->y);
        int wpx = col(this->xsize);
        int hpx = row(this->ysize);
        int x1 = x0 + wpx;
        int y1 = y0 + hpx;

        // Background.
        S->fillRect(x0, y0, x1 - 1, y1 - 1, COLORS.EditBG);

        // Loop / sustain bands.
        if (e && e->num_nodes > 1 && (e->flags & ZTM_INSTENVF_LOOP)) {
            int lx0 = node_x(e->loop_start);
            int lx1 = node_x(e->loop_end);
            if (lx1 > lx0)
                S->fillRect(lx0, y0 + 1, lx1 - 1, y1 - 2, COLORS.EditBGhigh);
        }
        if (e && e->num_nodes > 1 && (e->flags & ZTM_INSTENVF_SUSTAIN)) {
            int sx0 = node_x(e->sustain_start);
            int sx1 = node_x(e->sustain_end);
            if (sx1 > sx0)
                S->fillRect(sx0, y0 + 1, sx1 - 1, y1 - 2, COLORS.EditBGlow);
        }

        // Fine dotted grid (8x8 pitch).
        for (int gy = y0 + 8; gy < y1 - 4; gy += 8) {
            for (int gx = x0 + 8; gx < x1 - 4; gx += 8) {
                S->fillRect(gx, gy, gx, gy, COLORS.EditBGlow);
            }
        }

        // Frame.
        TColor frame = active ? COLORS.Highlight : COLORS.EditText;
        S->fillRect(x0,     y0,     x1 - 1, y0,     frame);
        S->fillRect(x0,     y1 - 1, x1 - 1, y1 - 1, frame);
        S->fillRect(x0,     y0,     x0,     y1 - 1, frame);
        S->fillRect(x1 - 1, y0,     x1 - 1, y1 - 1, frame);

        if (!e || e->num_nodes == 0) {
            print(x0 + 8, y0 + (hpx / 2) - 4,
                  "Empty -- click in the canvas to add the first node.",
                  COLORS.EditText, S);
            screenmanager.UpdateWH(x0, y0, wpx, hpx);
            return;
        }

        TColor curve = COLORS.Highlight;
        // Flat tail left of first node.
        {
            int fx = node_x(0), fy = node_y(0);
            if (fx > x0) S->fillRect(x0, fy, fx, fy, curve);
        }
        // Diagonals between nodes.
        for (int i = 0; i + 1 < e->num_nodes; i++) {
            draw_line_1px(S, node_x(i), node_y(i), node_x(i+1), node_y(i+1), curve);
        }
        // Flat tail right of last node.
        {
            int lx = node_x(e->num_nodes - 1), ly = node_y(e->num_nodes - 1);
            if (lx < x1 - 1) S->fillRect(lx, ly, x1 - 1, ly, curve);
        }

        // Loop / sustain boundary vertical lines.
        if (e->num_nodes > 0 && (e->flags & ZTM_INSTENVF_LOOP)) {
            int lx0 = node_x(e->loop_start);
            int lx1 = node_x(e->loop_end);
            S->fillRect(lx0, y0 + 1, lx0, y1 - 2, COLORS.EditText);
            S->fillRect(lx1, y0 + 1, lx1, y1 - 2, COLORS.EditText);
        }
        if (e->num_nodes > 0 && (e->flags & ZTM_INSTENVF_SUSTAIN)) {
            int sx0 = node_x(e->sustain_start);
            int sx1 = node_x(e->sustain_end);
            S->fillRect(sx0, y0 + 1, sx0, y1 - 2, COLORS.Highlight);
            S->fillRect(sx1, y0 + 1, sx1, y1 - 2, COLORS.Highlight);
        }

        // Nodes.
        for (int i = 0; i < e->num_nodes; i++) {
            int nx = node_x(i), ny = node_y(i);
            if (i == ce_sel_node) {
                S->fillRect(nx - 2, ny - 2, nx + 2, ny + 2, COLORS.EditBG);
                S->fillRect(nx - 1, ny - 1, nx + 1, ny + 1, COLORS.EditText);
            } else {
                S->fillRect(nx - 1, ny - 1, nx + 1, ny + 1, COLORS.Highlight);
            }
        }

        // Live playhead.
        int lp = -1, lv = -1;
        if (zt_envelope_live_position(cur_inst, ce_slot, &lp, &lv) && lp >= 0) {
            int mt = max_tick_view();
            int px = x0 + (lp * wpx) / (mt > 0 ? mt : 1);
            if (px > x0 + 1 && px < x1 - 1) {
                S->fillRect(px, y0 + 1, px, y1 - 2, COLORS.EditText);
            }
            int v = ccenv_interp_raw(e->tick, e->value, e->num_nodes, lp);
            int cy = y0 + ((127 - v) * (hpx - 2)) / 127 + 1;
            S->fillRect(px - 1, cy - 1, px + 1, cy + 1, COLORS.EditText);
        }
        screenmanager.UpdateWH(x0, y0, wpx, hpx);
    }
};
} // anon namespace

// ---------------------------------------------------------------------------
// File picker (right pane).

namespace {
class CeFileList : public ListBox {
public:
    CeFileList() {
        empty_message = "No .env files";
        is_sorted = false;
        use_checks = false;
        use_key_select = true;
    }
    void OnChange() override {
        clear();
        for (int i = ce_num_files - 1; i >= 0; i--) insertItem(ce_files[i]);
    }
    void OnSelectChange() override {
        LBNode *p = getNode(cur_sel + y_start);
        if (p && p->caption) {
            strncpy(ce_filename_buf, p->caption, sizeof ce_filename_buf - 1);
            ce_filename_buf[sizeof ce_filename_buf - 1] = 0;
        }
    }
    void OnSelect(LBNode *p) override { OnSelectChange(); (void)p; }
};
} // anon namespace

// ---------------------------------------------------------------------------
// Sync widget state <-> envelope struct.

static void ce_pull_to_widgets(UserInterface *UI) {
    inst_envelope *e = ce_env();
    int cc_val   = e ? e->cc       : 0;
    if (cc_val == ZTM_INSTENV_CC_NONE) cc_val = 0;
    int kind_val = e ? e->kind     : 0;
    int speed    = e ? e->speed    : 1;
    ((ValueSlider *)UI->get_element(W_CC_SLIDER))->value    = cc_val;
    ((ValueSlider *)UI->get_element(W_KIND_SLIDER))->value  = kind_val;
    ((ValueSlider *)UI->get_element(W_SPEED_SLIDER))->value = speed;
    ce_flag_ena   = e ? !!(e->flags & ZTM_INSTENVF_ENABLED) : 0;
    ce_flag_loop  = e ? !!(e->flags & ZTM_INSTENVF_LOOP)    : 0;
    ce_flag_sus   = e ? !!(e->flags & ZTM_INSTENVF_SUSTAIN) : 0;
    ce_flag_carry = e ? !!(e->flags & ZTM_INSTENVF_CARRY)   : 0;
}

static void ce_push_from_widgets(UserInterface *UI) {
    inst_envelope *e = ce_env();
    if (!e) return;
    int v_cc    = ((ValueSlider *)UI->get_element(W_CC_SLIDER))->value;
    int v_kind  = ((ValueSlider *)UI->get_element(W_KIND_SLIDER))->value;
    int v_speed = ((ValueSlider *)UI->get_element(W_SPEED_SLIDER))->value;
    int v_flags = (ce_flag_ena   ? ZTM_INSTENVF_ENABLED : 0)
                | (ce_flag_loop  ? ZTM_INSTENVF_LOOP    : 0)
                | (ce_flag_sus   ? ZTM_INSTENVF_SUSTAIN : 0)
                | (ce_flag_carry ? ZTM_INSTENVF_CARRY   : 0);
    if ((int)e->cc != v_cc || (int)e->kind != v_kind
        || (int)e->speed != v_speed || (int)e->flags != v_flags) {
        e->cc    = (unsigned char)v_cc;
        e->kind  = (unsigned char)v_kind;
        e->speed = (unsigned short)v_speed;
        e->flags = (unsigned char)v_flags;
        file_changed++;
    }
}

// ---------------------------------------------------------------------------
// Page lifecycle

CUI_CCEnvelopeEditor::CUI_CCEnvelopeEditor(void) {
    UI = new UserInterface;
    ce_page_ui = UI;

    // Layout regions:
    //   Slot list:  cols  2..28  rows BASE_Y+1..BASE_Y+22 (22 rows visible)
    //   Top strip:  cols 30..78  rows BASE_Y+1..BASE_Y+4 (meta + flags)
    //   Canvas:     cols 30..68  rows BASE_Y+6..BASE_Y+22 (16 rows tall)
    //   File pane:  cols 70..78  rows BASE_Y+6..BASE_Y+22

    CeSlotList *sl = new CeSlotList;
    UI->add_element(sl, W_SLOT_LIST);
    sl->x = 2; sl->y = BASE_Y + 1; sl->xsize = 26; sl->ysize = 22;

    EnvCanvas *cv = new EnvCanvas;
    UI->add_element(cv, W_CANVAS);
    cv->x = 30; cv->y = BASE_Y + 6;
    cv->xsize = 38; cv->ysize = 17;

    ValueSlider *vs;
    CheckBox    *cb;
    Button      *bt;
    TextInput   *ti;

    // CC# slider (top of right pane).
    vs = new ValueSlider;
    UI->add_element(vs, W_CC_SLIDER);
    vs->x = 38; vs->y = BASE_Y + 1; vs->xsize = 14; vs->ysize = 1;
    vs->min = 0; vs->max = 127; vs->value = 1;

    // Kind slider.
    vs = new ValueSlider;
    UI->add_element(vs, W_KIND_SLIDER);
    vs->x = 38; vs->y = BASE_Y + 2; vs->xsize = 6; vs->ysize = 1;
    vs->min = 0; vs->max = 2; vs->value = 0;

    // Speed slider.
    vs = new ValueSlider;
    UI->add_element(vs, W_SPEED_SLIDER);
    vs->x = 38; vs->y = BASE_Y + 3; vs->xsize = 10; vs->ysize = 1;
    vs->min = 1; vs->max = 255; vs->value = 1;

    // Flag checkboxes -- one per row with full-word labels drawn by draw().
    cb = new CheckBox;
    UI->add_element(cb, W_FLAG_ENABLED);
    cb->x = 30; cb->y = BASE_Y + 4; cb->xsize = 3; cb->frame = 1;
    cb->value = &ce_flag_ena;

    cb = new CheckBox;
    UI->add_element(cb, W_FLAG_LOOP);
    cb->x = 42; cb->y = BASE_Y + 4; cb->xsize = 3; cb->frame = 1;
    cb->value = &ce_flag_loop;

    cb = new CheckBox;
    UI->add_element(cb, W_FLAG_SUSTAIN);
    cb->x = 51; cb->y = BASE_Y + 4; cb->xsize = 3; cb->frame = 1;
    cb->value = &ce_flag_sus;

    cb = new CheckBox;
    UI->add_element(cb, W_FLAG_CARRY);
    cb->x = 63; cb->y = BASE_Y + 4; cb->xsize = 3; cb->frame = 1;
    cb->value = &ce_flag_carry;

    // File picker on far right.
    CeFileList *fl = new CeFileList;
    UI->add_element(fl, W_FILE_LIST);
    fl->x = 70; fl->y = BASE_Y + 6; fl->xsize = 9; fl->ysize = 12;

    ti = new TextInput;
    UI->add_element(ti, W_FILENAME);
    ti->frame = 1;
    ti->x = 70; ti->y = BASE_Y + 19;
    ti->xsize = 9; ti->length = sizeof ce_filename_buf - 1;
    ti->str = (unsigned char *)ce_filename_buf;

    bt = new Button;
    UI->add_element(bt, W_BTN_LOAD);
    bt->x = 70; bt->y = BASE_Y + 21; bt->xsize = 9; bt->ysize = 1;
    bt->caption = " Load";

    bt = new Button;
    UI->add_element(bt, W_BTN_SAVE);
    bt->x = 70; bt->y = BASE_Y + 22; bt->xsize = 9; bt->ysize = 1;
    bt->caption = " Save";
}

void CUI_CCEnvelopeEditor::enter(void) {
    need_refresh++;
    cur_state = STATE_CCENVELOPE;
    ce_rescan_folder();
    ce_load_inst_bank_if_needed();
    ((CeFileList *)UI->get_element(W_FILE_LIST))->OnChange();
    ((CeSlotList *)UI->get_element(W_SLOT_LIST))->OnChange();
    ce_last_inst = cur_inst;
    if (ce_slot < 0 || ce_slot >= ZTM_INST_MAX_ENVELOPES) ce_slot = 0;
    ce_pull_to_widgets(UI);
    UI->set_focus(W_CANVAS);
    Keys.flush();
    ce_set_status("Inst %d: pick a slot on the left, click in canvas to draw.",
                  cur_inst);
}

void CUI_CCEnvelopeEditor::leave(void) {
}

void CUI_CCEnvelopeEditor::update(void) {
    // Stale-event flush for the first 3 frames after entry, like
    // zt_request_ui_full_refresh's Keys.flush does on Cmd-Tab.
    static int frames_since_enter = 999;
    if (cur_state == STATE_CCENVELOPE && frames_since_enter < 3) {
        Keys.flush();
        frames_since_enter++;
    } else if (cur_state != STATE_CCENVELOPE) {
        frames_since_enter = 0;
    }
    if (frames_since_enter == 999) frames_since_enter = 0;   // first-ever entry

    // Navigation keys (peek BEFORE UI->update so widgets can't eat them).
    if (Keys.size()) {
        KBKey nk = Keys.checkkey();
        KBMod nks = Keys.cur_state;
        if (nk == SDLK_ESCAPE) {
            Keys.getkey();
            switch_page(UIP_Patterneditor);
            need_refresh++;
            return;
        }
        if ((nk == SDLK_F1 || nk == SDLK_F2 || nk == SDLK_F3 ||
             nk == SDLK_F4 || nk == SDLK_F5 || nk == SDLK_F6 ||
             nk == SDLK_F7 || nk == SDLK_F8 || nk == SDLK_F9 ||
             nk == SDLK_F10 || nk == SDLK_F11 || nk == SDLK_F12)
            && !(nks & KS_CTRL)) {
            need_refresh++;
            return;
        }
    }

    UI->update();

    // cur_inst changed (user Tab'd back to F3 and picked another)
    // -- rebuild the slot list since the bank may differ.
    if (cur_inst != ce_last_inst) {
        ce_last_inst = cur_inst;
        ce_slot = 0; ce_sel_node = -1;
        ce_load_inst_bank_if_needed();
        ((CeSlotList *)UI->get_element(W_SLOT_LIST))->OnChange();
        ce_pull_to_widgets(UI);
        ce_force_refresh();
    }

    // Load button.
    Button *b = (Button *)UI->get_element(W_BTN_LOAD);
    if (b && b->changed) {
        if (ce_filename_buf[0]) {
            char path[2048];
            snprintf(path, sizeof path, "%s/%s", ce_folder, ce_filename_buf);
            inst_envelope *e = ce_env();
            if (e && ccenv_read_file(path, e) == 0) {
                ce_set_status("Loaded %s into slot %d", ce_filename_buf, ce_slot);
                file_changed++;
                ce_sel_node = e->num_nodes > 0 ? 0 : -1;
                ce_pull_to_widgets(UI);
            } else {
                ce_set_status("Failed to load %s", ce_filename_buf);
            }
        } else {
            ce_set_status("Pick a file from the list first.");
        }
        b->changed = 0;
        ce_force_refresh();
    }
    b = (Button *)UI->get_element(W_BTN_SAVE);
    if (b && b->changed) {
        if (ce_filename_buf[0] && ce_folder[0]) {
            inst_envelope *e = ce_env();
            if (e && e->num_nodes > 0) {
                char path[2048];
                snprintf(path, sizeof path, "%s/%s", ce_folder, ce_filename_buf);
                int n = (int)strlen(path);
                if (n < 4 || strcasecmp(path + n - 4, ".env") != 0) {
                    if (n + 4 < (int)sizeof path) strcat(path, ".env");
                }
                if (ccenv_write_file(path, e) == 0) {
                    ce_set_status("Saved %s", path);
                    ce_rescan_folder();
                    ((CeFileList *)UI->get_element(W_FILE_LIST))->OnChange();
                } else {
                    ce_set_status("Save failed: %s", path);
                }
            } else {
                ce_set_status("Slot is empty -- nothing to save.");
            }
        } else {
            ce_set_status("Filename or folder missing.");
        }
        b->changed = 0;
        ce_force_refresh();
    }

    // Sync metadata sliders back to the envelope.
    ce_push_from_widgets(UI);

    // Live playhead: keep refreshing while any voice is active.
    int lp = -1, lv = -1;
    if (zt_envelope_live_position(cur_inst, ce_slot, &lp, &lv)) {
        need_refresh++;
        ce_page_ui->full_refresh();
        if (lv >= 0) {
            inst_envelope *e = ce_env();
            int cc = e ? (int)e->cc : -1;
            ce_set_status("LIVE  pos=%d  CC#%d -> %d", lp, cc, lv);
        }
    }
}

void CUI_CCEnvelopeEditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->full_refresh();
    UI->draw(S);
    draw_status(S);
    printtitle(PAGE_TITLE_ROW_Y, "CC Envelope Editor (Shift+F6)",
               COLORS.Text, COLORS.Background, S);

    // Slot-list header
    print(col(2), row(BASE_Y + 0), "Envelope slots:", COLORS.Text, S);

    // Right-pane metadata labels.
    {
        instrument *ii = ce_inst();
        char buf[160];
        memset(buf, ' ', sizeof buf); buf[sizeof buf - 1] = 0;
        if (ii) {
            const char *bank = ii->ccizer_bank[0] ? ii->ccizer_bank : "(no CCizer bank)";
            snprintf(buf, sizeof buf, "Inst %02d  Bank: %.40s", cur_inst, bank);
        }
        buf[40] = 0;
        printBG(col(30), row(BASE_Y + 0), buf, COLORS.Text, COLORS.Background, S);
    }
    print(col(30), row(BASE_Y + 1), "CC#",   COLORS.Text, S);
    print(col(30), row(BASE_Y + 2), "Kind",  COLORS.Text, S);
    {
        inst_envelope *e = ce_env();
        const char *knd = "CC";
        if (e) {
            if      (e->kind == 1) knd = "Pitchbend";
            else if (e->kind == 2) knd = "ChanPress";
        }
        char buf[24]; snprintf(buf, sizeof buf, "(%s)", knd);
        print(col(46), row(BASE_Y + 2), buf, COLORS.Text, S);
    }
    print(col(30), row(BASE_Y + 3), "Speed", COLORS.Text, S);

    // Flag row labels -- FULL WORDS, right of each checkbox.
    print(col(34), row(BASE_Y + 4), "Enabled", COLORS.Text, S);
    print(col(46), row(BASE_Y + 4), "Loop",    COLORS.Text, S);
    print(col(55), row(BASE_Y + 4), "Sustain", COLORS.Text, S);
    print(col(67), row(BASE_Y + 4), "Carry",   COLORS.Text, S);

    // Focused-slot legend just above the canvas.
    {
        inst_envelope *e = ce_env();
        char buf[160];
        memset(buf, ' ', sizeof buf);
        const char *slot_name = "(no slot)";
        char slot_label[80];
        if (g_inst_bank_valid && ce_slot < g_inst_bank.num_slots) {
            snprintf(slot_label, sizeof slot_label, "%s",
                     g_inst_bank.slots[ce_slot].name);
            slot_name = slot_label;
        } else {
            snprintf(slot_label, sizeof slot_label, "Slot %02d", ce_slot);
            slot_name = slot_label;
        }
        int nn = e ? e->num_nodes : 0;
        if (ce_sel_node >= 0 && nn > 0) {
            snprintf(buf, sizeof buf, "%-20.20s  nodes=%d  sel=%d t=%u v=%u",
                     slot_name, nn, ce_sel_node,
                     (unsigned)e->tick[ce_sel_node],
                     (unsigned)e->value[ce_sel_node]);
        } else {
            snprintf(buf, sizeof buf, "%-20.20s  nodes=%d  (no selection)",
                     slot_name, nn);
        }
        buf[48] = 0;
        printBG(col(30), row(BASE_Y + 5), buf,
                COLORS.Text, COLORS.Background, S);
    }

    // Right pane: filename label
    print(col(70), row(BASE_Y + 18), "File:", COLORS.Text, S);

    // Folder + status + hint at the bottom of the page.
    {
        char buf[200];
        memset(buf, ' ', sizeof buf);
        snprintf(buf, sizeof buf, "Folder: %s", ce_folder[0] ? ce_folder : "(unresolved)");
        buf[100] = 0;
        printBG(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 3), buf,
                COLORS.Text, COLORS.Background, S);
    }
    {
        char buf[200];
        memset(buf, ' ', sizeof buf);
        memcpy(buf, ce_status, strlen(ce_status));
        buf[120] = 0;
        printBG(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 2), buf,
                COLORS.Text, COLORS.Background, S);
    }
    print(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 1),
          "Click=add/select  Drag=move  Right-click=delete  "
          "Arrows=nav  Sh-Arr=tick  Space=insert  Del=remove  "
          "L=loop  S=sustain  E=enable  T=test  ESC=back",
          COLORS.Text, S);

    screenmanager.UpdateAll();

    need_refresh = 0;
    updated = 2;
    ztPlayer->num_orders();
    S->unlock();
}
