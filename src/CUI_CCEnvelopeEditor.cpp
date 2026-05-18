/*************************************************************************
 *
 * FILE  CUI_CCEnvelopeEditor.cpp
 *
 * DESCRIPTION
 *   The CC Envelope editor (Shift+F6).
 *
 *   Schism-style graphical envelope authoring: a 2D canvas where the
 *   user clicks to add nodes, drags to move them, right-clicks to
 *   delete, and keyboard hotkeys mark loop / sustain anchors at the
 *   selected node. The interpolated curve is rendered live on the
 *   canvas with the same math the playback thread uses
 *   (ccenv_interp.h), so what you see on-screen is what you'll hear.
 *
 *   Top strip carries the metadata (slot, name, cc#, kind, flag
 *   checkboxes) + a .env file picker. The canvas occupies the rest
 *   of the page.
 *
 *   Runtime triggering: per-instrument auto-arm OR Vxxyy pattern
 *   effect. See doc/design/cc-envelopes.md for the wider model.
 *
 ******/
#include "zt.h"

#include "ccenv_io.h"
#include "ccenv_interp.h"
#include "editor_layout.h"
#include "Button.h"
#include "Sliders.h"

#if defined(_WIN32) && !defined(strcasecmp)
  #define strcasecmp _stricmp
#endif

// Forward-decl at file scope so the anonymous-namespace EnvCanvas
// sees the GLOBAL cur_inst (not an internal-linkage clone).
extern int cur_inst;

#define BASE_Y          (TRACKS_ROW_Y + 0)
#define SPACE_AT_BOTTOM 8

// Widget IDs. The canvas tab-stop is FIRST so Tab from outside lands
// straight on the drawing surface -- that's the page's primary
// interaction. Metadata follows; everything else trails.
enum {
    W_CANVAS = 0,
    W_SLOT,
    W_NAME,
    W_CC,
    W_KIND,
    W_FLAG_ENA,
    W_FLAG_LOOP,
    W_FLAG_SUS,
    W_FLAG_CARRY,
    W_FILE_LIST,
    W_FILENAME,
    W_BTN_LOAD,
    W_BTN_SAVE
};

static int  ce_slot     = 0;
static int  ce_selected = -1;   // selected node index on the canvas (-1 = none)
static char ce_name_buf[ZTM_CCENVNAME_MAXLEN] = {0};
static char ce_filename_buf[256] = {0};
static char ce_folder[1024]      = {0};
static char ce_files[256][256];
static int  ce_num_files         = 0;
static char ce_status[160]       = {0};

// CheckBox::value is `int *` (a pointer to a backing int the page owns).
static int ce_flag_ena   = 0;
static int ce_flag_loop  = 0;
static int ce_flag_sus   = 0;
static int ce_flag_carry = 0;

// Canvas geometry (character units). The metadata strip occupies
// rows BASE_Y+0..BASE_Y+8; legend sits at row CANVAS_Y-1; canvas
// itself runs from CANVAS_Y down CANVAS_H rows.
static int CANVAS_X  = 2;
static int CANVAS_Y  = 22;
static int CANVAS_W  = 70;
static int CANVAS_H  = 14;

// ---------------------------------------------------------------------------
// Helpers

static ccenvelope *ce_ensure(int slot) {
    if (slot < 0 || slot >= ZTM_MAX_CCENVELOPES) return NULL;
    if (!song->ccenvelopes[slot]) {
        song->ccenvelopes[slot] = new ccenvelope;
        // Sensible default: enabled, empty node list. User clicks to
        // populate.
        song->ccenvelopes[slot]->flags = ZTM_CCENVF_ENABLED;
    }
    return song->ccenvelopes[slot];
}

static void ce_load_name(int slot) {
    ccenvelope *e = song->ccenvelopes[slot];
    if (e) {
        memcpy(ce_name_buf, e->name, ZTM_CCENVNAME_MAXLEN);
        ce_name_buf[ZTM_CCENVNAME_MAXLEN - 1] = 0;
    } else {
        ce_name_buf[0] = 0;
    }
}

static void ce_store_name(int slot) {
    bool any_text = (ce_name_buf[0] != 0);
    ccenvelope *cur = song->ccenvelopes[slot];
    if (!any_text && (!cur || cur->isempty())) return;
    ccenvelope *e = ce_ensure(slot);
    if (!e) return;
    if (memcmp(e->name, ce_name_buf, ZTM_CCENVNAME_MAXLEN) != 0) {
        memcpy(e->name, ce_name_buf, ZTM_CCENVNAME_MAXLEN);
        e->name[ZTM_CCENVNAME_MAXLEN - 1] = 0;
        file_changed++;
    }
}

static void ce_set_status(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(ce_status, sizeof ce_status, fmt, ap);
    va_end(ap);
}

static void ce_rescan_folder(void) {
    ce_num_files = 0;
    if (ccenv_resolve_folder(ce_folder, sizeof ce_folder) == 0) {
        ce_num_files = ccenv_scan_folder(ce_folder, ce_files,
                                         (int)(sizeof ce_files / sizeof ce_files[0]));
    }
}

// Best displayed x-tick range. Auto-fits to ~120% of the last node
// or 64 ticks, whichever is larger. Caps at 65535 so we don't get
// nonsense at the upper bound.
static int ce_max_tick_view(const ccenvelope *e) {
    if (!e || e->num_nodes == 0) return 64;
    int t = (int)e->tick[e->num_nodes - 1];
    int v = t + (t / 4);   // 25% headroom
    if (v < 64)    v = 64;
    if (v > 65535) v = 65535;
    return v;
}

// Sort nodes by tick after a move (keeps the array monotonic so
// ccenv_interp_raw works correctly). Returns the new index of the
// node that was at `moved_idx`.
static int ce_sort_after_move(ccenvelope *e, int moved_idx) {
    if (!e || moved_idx < 0 || moved_idx >= e->num_nodes) return moved_idx;
    // Bubble the moved node into position.
    int idx = moved_idx;
    while (idx > 0 && e->tick[idx - 1] > e->tick[idx]) {
        unsigned short t = e->tick[idx]; e->tick[idx] = e->tick[idx - 1]; e->tick[idx - 1] = t;
        unsigned char  v = e->value[idx]; e->value[idx] = e->value[idx - 1]; e->value[idx - 1] = v;
        idx--;
    }
    while (idx + 1 < e->num_nodes && e->tick[idx] > e->tick[idx + 1]) {
        unsigned short t = e->tick[idx]; e->tick[idx] = e->tick[idx + 1]; e->tick[idx + 1] = t;
        unsigned char  v = e->value[idx]; e->value[idx] = e->value[idx + 1]; e->value[idx + 1] = v;
        idx++;
    }
    return idx;
}

// ---------------------------------------------------------------------------
// File-picker listbox

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
        for (int i = ce_num_files - 1; i >= 0; --i) insertItem(ce_files[i]);
    }
    void OnSelectChange() override {
        LBNode *p = getNode(cur_sel + y_start);
        if (p && p->caption) {
            strncpy(ce_filename_buf, p->caption, sizeof ce_filename_buf - 1);
            ce_filename_buf[sizeof ce_filename_buf - 1] = 0;
        }
    }
    void OnSelect(LBNode *p) override {
        if (p && p->caption) {
            strncpy(ce_filename_buf, p->caption, sizeof ce_filename_buf - 1);
            ce_filename_buf[sizeof ce_filename_buf - 1] = 0;
        }
        ListBox::OnSelect(p);
    }
};

// ---------------------------------------------------------------------------
// The drawing canvas. This is where the user lives.
//
// Mouse:
//   Left-click on empty space   -> add a node at the clicked position,
//                                  select it.
//   Left-click on existing node -> select it; drag to move.
//   Right-click on node         -> delete it.
//
// Keyboard (when canvas has focus -- the default tab stop):
//   Left  / Right       : select previous / next node
//   Up    / Down        : selected node's value +/-
//   Shift-Left / Right  : selected node's tick  -/+ (also re-sorts)
//   Delete / Backspace  : delete selected node
//   L                   : assign loop-start at selected node;
//                         second press assigns loop-end (toggles)
//   S                   : same for sustain
//   E                   : toggle Enabled flag
//   Space               : add a new node halfway between selected
//                         and the next one
//
// The widget interprets MousePressX/Y (last-down location) for click
// hit-testing and LastX/Y (current position) for drag motion. The
// dragging flag keeps motion local until mouse-up.
class EnvCanvas : public UserInterfaceElement {
public:
    int dragging;

    EnvCanvas() {
        no_tab_stop = false;
        dragging = 0;
        // x/y/xsize/ysize set by the page constructor based on
        // CANVAS_X/Y/W/H constants.
    }

    int pix_to_tick(int px, const ccenvelope *e) const {
        int rel = px - col(this->x);
        int span = col(this->xsize);
        if (span <= 0) return 0;
        int mt = ce_max_tick_view(e);
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
    int node_pix_x(int n_idx, const ccenvelope *e) const {
        int mt = ce_max_tick_view(e);
        int span = col(this->xsize);
        return col(this->x) + ((int)e->tick[n_idx] * span) / (mt > 0 ? mt : 1);
    }
    int node_pix_y(int n_idx, const ccenvelope *e) const {
        int span = row(this->ysize);
        return row(this->y) + ((127 - (int)e->value[n_idx]) * span) / 127;
    }

    // Find the nearest node to (px, py) within a small radius.
    // Returns -1 if none. 5px is the visual radius of the selected
    // node marker -- anything farther counts as empty space so the
    // click adds a new node rather than reselecting the old one.
    int find_node_near(int px, int py, const ccenvelope *e) const {
        if (!e || e->num_nodes == 0) return -1;
        int best = -1;
        int best_dsq = 5 * 5;
        for (int i = 0; i < e->num_nodes; i++) {
            int dx = px - node_pix_x(i, e);
            int dy = py - node_pix_y(i, e);
            int dsq = dx * dx + dy * dy;
            if (dsq < best_dsq) { best = i; best_dsq = dsq; }
        }
        return best;
    }

    int hit_canvas(int px, int py) const {
        return px >= col(this->x) && px < col(this->x + this->xsize)
            && py >= row(this->y) && py < row(this->y + this->ysize);
    }

    int mouseupdate(int cur_element) override {
        KBKey key = Keys.checkkey();
        ccenvelope *e = ce_ensure(ce_slot);

        // Left button down: select existing node or add a new one.
        if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT))) {
            if (hit_canvas(MousePressX, MousePressY)) {
                int hit = find_node_near(MousePressX, MousePressY, e);
                if (hit >= 0) {
                    ce_selected = hit;
                    dragging = 1;
                    int nn_sel = e ? e->num_nodes : 0;
                    ce_set_status("Selected node %d/%d (click elsewhere "
                                  "to add a new one; drag to move)",
                                  hit, nn_sel);
                } else if (e && e->num_nodes < ZTM_CCENV_MAX_NODES) {
                    // Add a node at the clicked position.
                    int t = pix_to_tick(MousePressX, e);
                    int v = pix_to_value(MousePressY);
                    e->tick[e->num_nodes]  = (unsigned short)t;
                    e->value[e->num_nodes] = (unsigned char)v;
                    e->num_nodes++;
                    ce_selected = ce_sort_after_move(e, e->num_nodes - 1);
                    file_changed++;
                    ce_set_status("Added node %d/%d at tick=%d value=%d",
                                  ce_selected, e->num_nodes, t, v);
                }
                Keys.getkey();
                need_refresh++;
                need_redraw++;
                return this->ID;
            }
        }
        // Right button down on a node: delete it.
        if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT))) {
            if (hit_canvas(MousePressX, MousePressY)) {
                int hit = find_node_near(MousePressX, MousePressY, e);
                if (hit >= 0 && e) {
                    for (int k = hit; k + 1 < e->num_nodes; k++) {
                        e->tick[k]  = e->tick[k + 1];
                        e->value[k] = e->value[k + 1];
                    }
                    e->num_nodes--;
                    if (ce_selected >= e->num_nodes) ce_selected = e->num_nodes - 1;
                    file_changed++;
                    ce_set_status("Deleted node %d", hit);
                }
                Keys.getkey();
                need_refresh++;
                need_redraw++;
                return this->ID;
            }
        }
        // Left button up: stop dragging.
        if (key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT))) {
            if (dragging) {
                dragging = 0;
                Keys.getkey();
                need_refresh++;
                need_redraw++;
                return this->ID;
            }
        }
        // Live drag: move the selected node to follow the cursor.
        if (dragging && e && ce_selected >= 0 && ce_selected < e->num_nodes) {
            if (hit_canvas(LastX, LastY)) {
                int new_t = pix_to_tick(LastX, e);
                int new_v = pix_to_value(LastY);
                if (new_t != e->tick[ce_selected] || new_v != e->value[ce_selected]) {
                    e->tick[ce_selected]  = (unsigned short)new_t;
                    e->value[ce_selected] = (unsigned char)new_v;
                    ce_selected = ce_sort_after_move(e, ce_selected);
                    file_changed++;
                    need_refresh++;
                    need_redraw++;
                }
            }
        }
        return cur_element;
    }

    int update() override {
        KBKey k = Keys.checkkey();
        KBMod ks = Keys.cur_state;
        if (!k) return 0;
        ccenvelope *e = song->ccenvelopes[ce_slot];
        if (!e) return 0;
        int nn = e->num_nodes;
        int max_idx = nn > 0 ? nn - 1 : 0;

        bool consumed = false;

        if (k == SDLK_LEFT) {
            if (ks & KS_SHIFT) {
                if (ce_selected >= 0 && ce_selected < nn && e->tick[ce_selected] > 0) {
                    int t = e->tick[ce_selected] - 1;
                    if (t < 0) t = 0;
                    e->tick[ce_selected] = (unsigned short)t;
                    ce_selected = ce_sort_after_move(e, ce_selected);
                    file_changed++;
                }
            } else {
                if (ce_selected > 0)        ce_selected--;
                else if (nn > 0)            ce_selected = 0;
            }
            consumed = true;
        } else if (k == SDLK_RIGHT) {
            if (ks & KS_SHIFT) {
                if (ce_selected >= 0 && ce_selected < nn && e->tick[ce_selected] < 65535) {
                    int t = e->tick[ce_selected] + 1;
                    if (t > 65535) t = 65535;
                    e->tick[ce_selected] = (unsigned short)t;
                    ce_selected = ce_sort_after_move(e, ce_selected);
                    file_changed++;
                }
            } else {
                if (ce_selected < max_idx)  ce_selected++;
                else if (nn > 0)            ce_selected = max_idx;
            }
            consumed = true;
        } else if (k == SDLK_UP) {
            if (ce_selected >= 0 && ce_selected < nn && e->value[ce_selected] < 127) {
                int delta = (ks & KS_SHIFT) ? 8 : 1;
                int v = (int)e->value[ce_selected] + delta;
                if (v > 127) v = 127;
                e->value[ce_selected] = (unsigned char)v;
                file_changed++;
            }
            consumed = true;
        } else if (k == SDLK_DOWN) {
            if (ce_selected >= 0 && ce_selected < nn && e->value[ce_selected] > 0) {
                int delta = (ks & KS_SHIFT) ? 8 : 1;
                int v = (int)e->value[ce_selected] - delta;
                if (v < 0) v = 0;
                e->value[ce_selected] = (unsigned char)v;
                file_changed++;
            }
            consumed = true;
        } else if (k == SDLK_DELETE || k == SDLK_BACKSPACE) {
            if (ce_selected >= 0 && ce_selected < nn) {
                for (int j = ce_selected; j + 1 < nn; j++) {
                    e->tick[j]  = e->tick[j + 1];
                    e->value[j] = e->value[j + 1];
                }
                e->num_nodes--;
                if (ce_selected >= e->num_nodes) ce_selected = e->num_nodes - 1;
                file_changed++;
                ce_set_status("Deleted node");
            }
            consumed = true;
        } else if (k == SDLK_SPACE) {
            // Insert a node halfway between selected and the next.
            if (nn > 0 && nn < ZTM_CCENV_MAX_NODES && ce_selected >= 0) {
                int ins = ce_selected + 1;
                if (ins > nn) ins = nn;
                for (int j = nn; j > ins; j--) {
                    e->tick[j]  = e->tick[j - 1];
                    e->value[j] = e->value[j - 1];
                }
                int t0 = (ins > 0)       ? (int)e->tick[ins - 1]  : 0;
                int t1 = (ins < nn)      ? (int)e->tick[ins]      : t0 + 16;
                int v0 = (ins > 0)       ? (int)e->value[ins - 1] : 0;
                int v1 = (ins < nn)      ? (int)e->value[ins]     : v0;
                e->tick[ins]  = (unsigned short)((t0 + t1) / 2);
                e->value[ins] = (unsigned char)((v0 + v1) / 2);
                e->num_nodes++;
                ce_selected = ins;
                file_changed++;
                ce_set_status("Inserted node at %d", ins);
            } else if (nn == 0) {
                // Empty envelope: a single first node at tick 0, value 64.
                e->tick[0] = 0; e->value[0] = 64;
                e->num_nodes = 1;
                ce_selected = 0;
                file_changed++;
                ce_set_status("First node added (tick 0, value 64)");
            }
            consumed = true;
        } else if (k == SDLK_L && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            // Cycle loop_start / loop_end onto the selected node, and
            // make sure the LOOP flag is set. Pressing L the first time
            // sets loop_start. Pressing it again (with the same node)
            // sets loop_end. After that, pressing it AGAIN moves
            // loop_start back to the selected node and so on.
            if (ce_selected >= 0 && ce_selected < nn) {
                if (e->loop_start == ce_selected && e->loop_end != ce_selected) {
                    e->loop_end = (unsigned char)ce_selected;
                } else {
                    e->loop_start = (unsigned char)ce_selected;
                    if (e->loop_end < e->loop_start)
                        e->loop_end = (unsigned char)ce_selected;
                }
                e->flags |= ZTM_CCENVF_LOOP;
                ce_flag_loop = 1;
                file_changed++;
                ce_set_status("Loop set: %u..%u", e->loop_start, e->loop_end);
            }
            consumed = true;
        } else if (k == SDLK_S && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            if (ce_selected >= 0 && ce_selected < nn) {
                if (e->sustain_start == ce_selected && e->sustain_end != ce_selected) {
                    e->sustain_end = (unsigned char)ce_selected;
                } else {
                    e->sustain_start = (unsigned char)ce_selected;
                    if (e->sustain_end < e->sustain_start)
                        e->sustain_end = (unsigned char)ce_selected;
                }
                e->flags |= ZTM_CCENVF_SUSTAIN;
                ce_flag_sus = 1;
                file_changed++;
                ce_set_status("Sustain set: %u..%u", e->sustain_start, e->sustain_end);
            }
            consumed = true;
        } else if (k == SDLK_E && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            e->flags ^= ZTM_CCENVF_ENABLED;
            ce_flag_ena = (e->flags & ZTM_CCENVF_ENABLED) ? 1 : 0;
            file_changed++;
            ce_set_status("Enabled: %s", ce_flag_ena ? "on" : "off");
            consumed = true;
        } else if (k == SDLK_T && !(ks & (KS_CTRL | KS_ALT | KS_META))) {
            // Test-fire: arm this envelope on cur_inst right here so
            // the user can watch the playhead animate without leaving
            // the editor. Sentinel key 0 lets us release it on Esc /
            // page-leave but won't collide with real keyjazz keys.
            zt_audition_env_arm_envelope(ce_slot, cur_inst, 0);
            ce_set_status("Test-fired envelope %d on inst %d",
                          ce_slot, cur_inst);
            consumed = true;
        }

        if (consumed) {
            Keys.getkey();
            need_refresh++;
            need_redraw++;
        }
        return 0;
    }

    void draw(Drawable *S, int active) override {
        ccenvelope *e = song->ccenvelopes[ce_slot];

        int x0 = col(this->x);
        int y0 = row(this->y);
        int wpx = col(this->xsize);
        int hpx = row(this->ysize);
        int x1 = x0 + wpx;
        int y1 = y0 + hpx;

        // fillRect signature: (x1, y1, x2, y2) -- two corners.

        // (1) Background.
        S->fillRect(x0, y0, x1, y1, COLORS.EditBG);

        // (2) Loop / sustain colored bands BEFORE the curve so the
        // curve draws on top. Schism uses transparent shading; we
        // approximate with EditBGhigh (loop) and EditBGlow (sustain).
        if (e && e->num_nodes > 1 && (e->flags & ZTM_CCENVF_LOOP)) {
            int lx0 = node_pix_x(e->loop_start, e);
            int lx1 = node_pix_x(e->loop_end, e);
            if (lx1 > lx0)
                S->fillRect(lx0, y0 + 1, lx1, y1 - 1, COLORS.EditBGhigh);
        }
        if (e && e->num_nodes > 1 && (e->flags & ZTM_CCENVF_SUSTAIN)) {
            int sx0 = node_pix_x(e->sustain_start, e);
            int sx1 = node_pix_x(e->sustain_end, e);
            if (sx1 > sx0)
                S->fillRect(sx0, y0 + 1, sx1, y1 - 1, COLORS.EditBGlow);
        }

        // (3) Cartesian grid. Quarter-height horizontals at 25/50/75%
        // and value 127 reference line at the top.
        for (int q = 1; q <= 3; q++) {
            int gy = y0 + (q * hpx) / 4;
            for (int xx = x0 + 2; xx < x1 - 2; xx += 6) {
                S->fillRect(xx, gy, xx + 2, gy + 1, COLORS.EditBGlow);
            }
        }
        // Eighth-width vertical gridlines.
        for (int q = 1; q <= 7; q++) {
            int gx = x0 + (q * wpx) / 8;
            for (int yy = y0 + 2; yy < y1 - 2; yy += 6) {
                S->fillRect(gx, yy, gx + 1, yy + 2, COLORS.EditBGlow);
            }
        }

        // (4) Border.
        TColor frame_color = active ? COLORS.Highlight : COLORS.Text;
        S->fillRect(x0,     y0,     x1,     y0 + 1, frame_color);
        S->fillRect(x0,     y1 - 1, x1,     y1,     frame_color);
        S->fillRect(x0,     y0,     x0 + 1, y1,     frame_color);
        S->fillRect(x1 - 1, y0,     x1,     y1,     frame_color);

        if (!e || e->num_nodes == 0) {
            print(x0 + 6, y0 + (hpx / 2) - 4,
                  "Click in this box to add the first node.",
                  COLORS.Text, S);
            mark_dirty(x0, y0, x1, y1);
            return;
        }

        int mt = ce_max_tick_view(e);

        // (5) Curve as connected line segments between adjacent nodes
        // (Bresenham-ish). 3px thick so it reads cleanly on retina.
        TColor curve_color = COLORS.Highlight;
        for (int i = 0; i + 1 < e->num_nodes; i++) {
            int ax = node_pix_x(i, e);
            int ay = node_pix_y(i, e);
            int bx = node_pix_x(i + 1, e);
            int by = node_pix_y(i + 1, e);
            draw_line_3px(S, ax, ay, bx, by, curve_color);
        }
        // Single-node case: a horizontal stub at the node's value.
        if (e->num_nodes == 1) {
            int ny = node_pix_y(0, e);
            S->fillRect(x0 + 1, ny - 1, x1 - 1, ny + 2, curve_color);
        }

        // (6) Loop / sustain start+end vertical edge markers (sharp lines
        // at the boundaries of the colored bands).
        if (e->num_nodes > 0 && (e->flags & ZTM_CCENVF_LOOP)) {
            int lx0 = node_pix_x(e->loop_start, e);
            int lx1 = node_pix_x(e->loop_end, e);
            S->fillRect(lx0,     y0 + 1, lx0 + 1, y1 - 1, COLORS.Text);
            S->fillRect(lx1 - 1, y0 + 1, lx1,     y1 - 1, COLORS.Text);
        }
        if (e->num_nodes > 0 && (e->flags & ZTM_CCENVF_SUSTAIN)) {
            int sx0 = node_pix_x(e->sustain_start, e);
            int sx1 = node_pix_x(e->sustain_end, e);
            S->fillRect(sx0,     y0 + 1, sx0 + 1, y1 - 1, COLORS.Highlight);
            S->fillRect(sx1 - 1, y0 + 1, sx1,     y1 - 1, COLORS.Highlight);
        }

        // (7) Nodes. Big, with an outline that distinguishes selected
        // (filled bright) from unselected (filled highlight + 1px text-color outline).
        for (int i = 0; i < e->num_nodes; i++) {
            int nx = node_pix_x(i, e);
            int ny = node_pix_y(i, e);
            if (i == ce_selected) {
                // Selected: 8px filled bright square + 1px outline ring.
                S->fillRect(nx - 5, ny - 5, nx + 6, ny + 6, COLORS.Text);
                S->fillRect(nx - 4, ny - 4, nx + 5, ny + 5, COLORS.EditBG);
                S->fillRect(nx - 3, ny - 3, nx + 4, ny + 4, COLORS.Text);
            } else {
                // Unselected: 6px filled highlight square.
                S->fillRect(nx - 3, ny - 3, nx + 4, ny + 4, COLORS.Highlight);
            }
        }

        // (8) Live playhead.
        int live_pos = -1, live_val = -1;
        if (zt_envelope_live_position(ce_slot, &live_pos, &live_val)
            && live_pos >= 0) {
            int px = x0 + (live_pos * wpx) / (mt > 0 ? mt : 1);
            if (px > x0 + 1 && px < x1 - 1) {
                // Vertical playhead line.
                S->fillRect(px - 1, y0 + 1, px + 2, y1 - 1, COLORS.Text);
            }
            int v = ccenv_interp_raw(e->tick, e->value, e->num_nodes, live_pos);
            int cy = y0 + ((127 - v) * (hpx - 2)) / 127 + 1;
            S->fillRect(px - 4, cy - 4, px + 5, cy + 5, COLORS.Text);
            S->fillRect(px - 3, cy - 3, px + 4, cy + 4, COLORS.Highlight);
        }

        // Mark the entire canvas region dirty so screenmanager pushes
        // it to the window on the next Refresh(). Without this the
        // changes only become visible after Alt-Tab.
        mark_dirty(x0, y0, x1, y1);
    }

private:
    // Mark a screen-space rectangle dirty so the SDL backend pushes
    // it on the next present.
    void mark_dirty(int x0, int y0, int x1, int y1) const {
        if (x1 > x0 && y1 > y0)
            screenmanager.UpdateWH(x0, y0, x1 - x0, y1 - y0);
    }

    // Thick (3px) Bresenham line for the curve. Simple stepping
    // algorithm -- envelope curves never get long enough to matter
    // performance-wise.
    static void draw_line_3px(Drawable *S, int x0, int y0, int x1, int y1,
                              TColor c) {
        int dx = x1 - x0;
        int dy = y1 - y0;
        int adx = dx < 0 ? -dx : dx;
        int ady = dy < 0 ? -dy : dy;
        int steps = adx > ady ? adx : ady;
        if (steps <= 0) {
            S->fillRect(x0 - 1, y0 - 1, x0 + 2, y0 + 2, c);
            return;
        }
        for (int s = 0; s <= steps; s++) {
            int x = x0 + (dx * s) / steps;
            int y = y0 + (dy * s) / steps;
            S->fillRect(x - 1, y - 1, x + 2, y + 2, c);
        }
    }
};
} // namespace

// ---------------------------------------------------------------------------
// Sync widgets <-> envelope struct (the canvas already mutates the
// struct directly; this only covers the metadata controls).

static void ce_pull_to_widgets(UserInterface *UI) {
    ccenvelope *e = song->ccenvelopes[ce_slot];
    ((ValueSlider *)UI->get_element(W_SLOT))->value = ce_slot;
    ((ValueSlider *)UI->get_element(W_CC))->value   = e ? e->cc   : 1;
    ((ValueSlider *)UI->get_element(W_KIND))->value = e ? e->kind : 0;
    ce_flag_ena   = e ? !!(e->flags & ZTM_CCENVF_ENABLED) : 1;  // default enabled
    ce_flag_loop  = e ? !!(e->flags & ZTM_CCENVF_LOOP)    : 0;
    ce_flag_sus   = e ? !!(e->flags & ZTM_CCENVF_SUSTAIN) : 0;
    ce_flag_carry = e ? !!(e->flags & ZTM_CCENVF_CARRY)   : 0;
    ce_load_name(ce_slot);
}

static void ce_push_from_widgets(UserInterface *UI) {
    int v_cc       = ((ValueSlider *)UI->get_element(W_CC))->value;
    int v_kind     = ((ValueSlider *)UI->get_element(W_KIND))->value;
    int desired_flags = (ce_flag_ena ? ZTM_CCENVF_ENABLED : 0)
                      | (ce_flag_loop ? ZTM_CCENVF_LOOP : 0)
                      | (ce_flag_sus  ? ZTM_CCENVF_SUSTAIN : 0)
                      | (ce_flag_carry ? ZTM_CCENVF_CARRY : 0);
    ccenvelope *e = song->ccenvelopes[ce_slot];
    int e_cc    = e ? e->cc    : 1;
    int e_kind  = e ? e->kind  : 0;
    int e_flags = e ? e->flags : 0;
    if (!e || e_cc != v_cc || e_kind != v_kind || e_flags != desired_flags) {
        if (e_cc == v_cc && e_kind == v_kind && e_flags == desired_flags) return;
        e = ce_ensure(ce_slot);
        if (!e) return;
        e->cc    = (unsigned char)v_cc;
        e->kind  = (unsigned char)v_kind;
        e->flags = (unsigned char)desired_flags;
        file_changed++;
    }
}

// ---------------------------------------------------------------------------
// Page lifecycle

CUI_CCEnvelopeEditor::CUI_CCEnvelopeEditor(void) {
    UI = new UserInterface;

    ValueSlider *vs;
    TextInput   *ti;
    CheckBox    *cb;
    Button      *bt;

    const int INP_X   = 12;
    const int INP_W   = 14;
    const int RIGHT_X = 50;
    const int RIGHT_W = 28;

    // The canvas is the centerpiece. Position it BELOW the metadata
    // strip (rows BASE_Y+0..BASE_Y+9). 14 rows tall, 70 cols wide.
    EnvCanvas *cv = new EnvCanvas;
    UI->add_element(cv, W_CANVAS);
    cv->x = CANVAS_X; cv->y = CANVAS_Y;
    cv->xsize = CANVAS_W; cv->ysize = CANVAS_H;

    // Row 0: Slot
    vs = new ValueSlider;
    UI->add_element(vs, W_SLOT);
    vs->x = INP_X; vs->y = BASE_Y + 0; vs->xsize = INP_W; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_MAX_CCENVELOPES - 1; vs->value = 0;

    // Row 2: Name
    ti = new TextInput;
    UI->add_element(ti, W_NAME);
    ti->frame = 1;
    ti->x = INP_X; ti->y = BASE_Y + 2;
    ti->xsize = 28; ti->length = ZTM_CCENVNAME_MAXLEN - 1;
    ti->str = (unsigned char *)ce_name_buf;

    // Row 4: CC#
    vs = new ValueSlider;
    UI->add_element(vs, W_CC);
    vs->x = INP_X; vs->y = BASE_Y + 4; vs->xsize = INP_W; vs->ysize = 1;
    vs->min = 0; vs->max = 127; vs->value = 1;

    // Row 6: Kind on its own row (the "(CC/PB/CP)" decoration is
    // drawn by draw() right of the slider value display).
    vs = new ValueSlider;
    UI->add_element(vs, W_KIND);
    vs->x = INP_X; vs->y = BASE_Y + 6; vs->xsize = 6; vs->ysize = 1;
    vs->min = 0; vs->max = 2; vs->value = 0;

    // Row 8: 4 flag checkboxes side by side. Each checkbox is 3 chars
    // (frame + Off/On text); labels go to the right of each box.
    cb = new CheckBox; UI->add_element(cb, W_FLAG_ENA);
    cb->x = INP_X;      cb->y = BASE_Y + 8; cb->xsize = 3;
    cb->frame = 1; cb->value = &ce_flag_ena;
    cb = new CheckBox; UI->add_element(cb, W_FLAG_LOOP);
    cb->x = INP_X + 10; cb->y = BASE_Y + 8; cb->xsize = 3;
    cb->frame = 1; cb->value = &ce_flag_loop;
    cb = new CheckBox; UI->add_element(cb, W_FLAG_SUS);
    cb->x = INP_X + 20; cb->y = BASE_Y + 8; cb->xsize = 3;
    cb->frame = 1; cb->value = &ce_flag_sus;
    cb = new CheckBox; UI->add_element(cb, W_FLAG_CARRY);
    cb->x = INP_X + 30; cb->y = BASE_Y + 8; cb->xsize = 3;
    cb->frame = 1; cb->value = &ce_flag_carry;

    // Right pane (rows 1..9): file picker + filename + buttons. All
    // ABOVE the canvas (which starts at row CANVAS_Y=22). No content
    // in the right pane overflows into the canvas's row range.
    CeFileList *fl = new CeFileList;
    UI->add_element(fl, W_FILE_LIST);
    fl->x = RIGHT_X; fl->y = BASE_Y + 1;
    fl->xsize = RIGHT_W; fl->ysize = 6;

    ti = new TextInput; UI->add_element(ti, W_FILENAME);
    ti->frame = 1; ti->x = RIGHT_X; ti->y = BASE_Y + 8;
    ti->xsize = RIGHT_W; ti->length = sizeof ce_filename_buf - 1;
    ti->str = (unsigned char *)ce_filename_buf;

    bt = new Button; UI->add_element(bt, W_BTN_LOAD);
    bt->x = RIGHT_X     ; bt->y = BASE_Y + 9; bt->xsize = 12; bt->ysize = 1;
    bt->caption = "Load";
    bt = new Button; UI->add_element(bt, W_BTN_SAVE);
    bt->x = RIGHT_X + 14; bt->y = BASE_Y + 9; bt->xsize = 12; bt->ysize = 1;
    bt->caption = "Save";
}

void CUI_CCEnvelopeEditor::enter(void) {
    need_refresh++;
    cur_state = STATE_CCENVELOPE;
    ce_rescan_folder();
    ((CeFileList *)UI->get_element(W_FILE_LIST))->OnChange();
    ce_pull_to_widgets(UI);
    // Default the canvas selected-node to the first node if envelope
    // is populated.
    {
        ccenvelope *e = song->ccenvelopes[ce_slot];
        if (e && e->num_nodes > 0 && (ce_selected < 0 || ce_selected >= e->num_nodes))
            ce_selected = 0;
        else if (!e || e->num_nodes == 0)
            ce_selected = -1;
    }
    UI->set_focus(W_CANVAS);
    Keys.flush();
    ce_set_status("Click in the canvas to draw. Arrows = nav, "
                  "L=loop, S=sustain, E=enable, Del=remove, ESC=back");
}

void CUI_CCEnvelopeEditor::leave(void) {
    ce_store_name(ce_slot);
}

void CUI_CCEnvelopeEditor::update(void) {
    // Navigation keys: peek BEFORE UI->update() so a focused widget
    // (Slot slider, Name TextInput) doesn't swallow ESC / F-keys and
    // trap the user on this page.
    if (Keys.size()) {
        KBKey nk = Keys.checkkey();
        KBMod nks = Keys.cur_state;
        if (nk == SDLK_ESCAPE) {
            Keys.getkey();
            ce_store_name(ce_slot);
            switch_page(UIP_Patterneditor);
            need_refresh++;
            return;
        }
        if ((nk == SDLK_F1 || nk == SDLK_F2 || nk == SDLK_F3 ||
             nk == SDLK_F4 || nk == SDLK_F5 || nk == SDLK_F6 ||
             nk == SDLK_F7 || nk == SDLK_F8 || nk == SDLK_F9 ||
             nk == SDLK_F10 || nk == SDLK_F11 || nk == SDLK_F12)
            && !(nks & KS_CTRL)) {
            // Leave the key in the queue so global_keys() in main.cpp
            // dispatches the page switch.
            ce_store_name(ce_slot);
            need_refresh++;
            return;
        }
    }

    UI->update();

    // Slot changed?
    ValueSlider *vs_slot = (ValueSlider *)UI->get_element(W_SLOT);
    if (vs_slot && vs_slot->value != ce_slot) {
        ce_store_name(ce_slot);
        ce_slot = vs_slot->value;
        ce_selected = -1;
        ce_pull_to_widgets(UI);
        need_refresh++; doredraw++;
    }

    // Name changed
    TextInput *ti_name = (TextInput *)UI->get_element(W_NAME);
    if (ti_name && ti_name->changed) {
        ce_store_name(ce_slot);
        ti_name->changed = 0;
    }

    // Load button
    Button *b;
    b = (Button *)UI->get_element(W_BTN_LOAD);
    if (b && b->changed) {
        if (ce_filename_buf[0]) {
            char path[2048];
            snprintf(path, sizeof path, "%s/%s", ce_folder, ce_filename_buf);
            ccenvelope *e = ce_ensure(ce_slot);
            if (e && ccenv_read_file(path, e) == 0) {
                ce_set_status("Loaded %s into slot %d", ce_filename_buf, ce_slot);
                file_changed++;
                ce_selected = (e->num_nodes > 0) ? 0 : -1;
                ce_pull_to_widgets(UI);
            } else {
                ce_set_status("Failed to load %s", ce_filename_buf);
            }
        } else {
            ce_set_status("Select a file from the list first.");
        }
        b->changed = 0; need_refresh++; doredraw++;
    }
    b = (Button *)UI->get_element(W_BTN_SAVE);
    if (b && b->changed) {
        if (ce_filename_buf[0] && ce_folder[0]) {
            ccenvelope *e = song->ccenvelopes[ce_slot];
            if (e && !e->isempty()) {
                char path[2048];
                snprintf(path, sizeof path, "%s/%s", ce_folder, ce_filename_buf);
                int n = (int)strlen(path);
                if (n < 4 || strcasecmp(path + n - 4, ".env") != 0) {
                    if (n + 4 < (int)sizeof path) strcat(path, ".env");
                }
                if (ccenv_write_file(path, e) == 0) {
                    ce_set_status("Saved to %s", path);
                    ce_rescan_folder();
                    ((CeFileList *)UI->get_element(W_FILE_LIST))->OnChange();
                } else {
                    ce_set_status("Save failed: %s", path);
                }
            } else {
                ce_set_status("Slot is empty, nothing to save.");
            }
        } else {
            ce_set_status("Filename or folder missing.");
        }
        b->changed = 0; need_refresh++; doredraw++;
    }

    // Push metadata into envelope.
    ce_push_from_widgets(UI);

    // Live-view: while ANY voice is processing this envelope, keep
    // the page refreshing at main-loop rate so the playhead animates
    // and the last-emitted-CC value in the status line updates.
    int live_pos = -1, live_val = -1;
    if (zt_envelope_live_position(ce_slot, &live_pos, &live_val)) {
        need_refresh++;
        if (live_val >= 0) {
            ccenvelope *e_live = song->ccenvelopes[ce_slot];
            int cc_num = e_live ? (int)e_live->cc : -1;
            ce_set_status("LIVE  pos=%d  CC#%d -> %d",
                          live_pos, cc_num, live_val);
        }
    }
}

void CUI_CCEnvelopeEditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);
    printtitle(PAGE_TITLE_ROW_Y, "CC Envelope Editor (Shift+F6)",
               COLORS.Text, COLORS.Background, S);

    const int LBL_X   = 2;
    const int RIGHT_X = 50;

    print(col(LBL_X), row(BASE_Y + 0), "Slot",   COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 2), "Name",   COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 4), "CC#",    COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 6), "Kind",   COLORS.Text, S);
    {
        ccenvelope *e = song->ccenvelopes[ce_slot];
        const char *knd = "CC";
        if (e) {
            if      (e->kind == 1) knd = "Pitchbend";
            else if (e->kind == 2) knd = "ChanPress";
        }
        char buf[24]; snprintf(buf, sizeof buf, "(%s)", knd);
        print(col(22), row(BASE_Y + 6), buf, COLORS.Text, S);
    }

    // Flag row. Labels to the RIGHT of each checkbox.
    print(col(LBL_X),       row(BASE_Y + 8), "Flags", COLORS.Text, S);
    print(col(12 + 4),      row(BASE_Y + 8), "En",    COLORS.Text, S);
    print(col(12 + 10 + 4), row(BASE_Y + 8), "Lp",    COLORS.Text, S);
    print(col(12 + 20 + 4), row(BASE_Y + 8), "Su",    COLORS.Text, S);
    print(col(12 + 30 + 4), row(BASE_Y + 8), "Cy",    COLORS.Text, S);

    // Right pane labels (all rows 0..7; clear of the canvas).
    print(col(RIGHT_X), row(BASE_Y + 0),  ".env presets", COLORS.Text, S);
    print(col(RIGHT_X), row(BASE_Y + 7),  "Filename",     COLORS.Text, S);

    // Canvas legend on the LEFT only (cols 2..44). Pad to a fixed
    // width with trailing spaces so older, longer strings don't leak
    // through when the message shortens. `print()` doesn't blank the
    // background, so we pre-build a 48-char buffer.
    {
        ccenvelope *e = song->ccenvelopes[ce_slot];
        char line[64];
        memset(line, ' ', sizeof line);
        line[sizeof line - 1] = 0;
        int nn = e ? e->num_nodes : 0;
        char buf[64];
        if (nn == 0) {
            snprintf(buf, sizeof buf, "Empty -- click in canvas");
        } else if (ce_selected >= 0 && ce_selected < nn) {
            snprintf(buf, sizeof buf,
                     "Node %d/%d  t=%u v=%u",
                     ce_selected, nn,
                     (unsigned)e->tick[ce_selected],
                     (unsigned)e->value[ce_selected]);
        } else {
            snprintf(buf, sizeof buf, "%d nodes (no sel)", nn);
        }
        memcpy(line, buf, strlen(buf));   // overwrite leading bytes
        // Append loop / sustain info after column 28 / 38 respectively.
        if (e && (e->flags & ZTM_CCENVF_LOOP) && 28 + 12 < (int)sizeof line) {
            char lr[16];
            snprintf(lr, sizeof lr, "L %u..%u",
                     (unsigned)e->loop_start, (unsigned)e->loop_end);
            memcpy(line + 28, lr, strlen(lr));
        }
        if (e && (e->flags & ZTM_CCENVF_SUSTAIN) && 38 + 12 < (int)sizeof line) {
            char sr[16];
            snprintf(sr, sizeof sr, "S %u..%u",
                     (unsigned)e->sustain_start, (unsigned)e->sustain_end);
            memcpy(line + 38, sr, strlen(sr));
        }
        line[48] = 0;   // truncate so we stay clear of the right pane
        // printBG fills the character cells with COLORS.Background so
        // a previous longer status doesn't leave residual glyphs.
        printBG(col(CANVAS_X), row(CANVAS_Y - 1), line,
                COLORS.Text, COLORS.Background, S);
    }

    // Folder line BELOW the canvas, padded with printBG.
    {
        char line[160];
        memset(line, ' ', sizeof line);
        line[sizeof line - 1] = 0;
        char buf[160];
        snprintf(buf, sizeof buf, "Folder: %s",
                 ce_folder[0] ? ce_folder : "(none)");
        memcpy(line, buf, strlen(buf));
        line[100] = 0;
        printBG(col(2), row(CANVAS_Y + CANVAS_H + 1), line,
                COLORS.Text, COLORS.Background, S);
    }

    // Status line padded with printBG.
    {
        char line[160];
        memset(line, ' ', sizeof line);
        line[sizeof line - 1] = 0;
        memcpy(line, ce_status, strlen(ce_status));
        line[100] = 0;
        printBG(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 2), line,
                COLORS.Text, COLORS.Background, S);
    }

    print(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 1),
          "Click=add/select, Drag=move, Right-click=delete, "
          "Arrows=nav, Sh-Arr=tick, Space=insert, L=loop, S=sus, E=ena, "
          "Del=remove, ESC=back",
          COLORS.Text, S);

    // Mark the whole page surface dirty so SDL pushes the new frame
    // on the next backend present. Without this, fillRect / print
    // writes sit in the in-memory surface unread by the window until
    // something else (focus-gained, popup close, etc.) triggers a
    // full refresh -- which is why clicks "didn't update" until the
    // user Alt-Tab'd away and back.
    screenmanager.UpdateAll();

    need_refresh = 0; updated = 2;
    ztPlayer->num_orders();
    S->unlock();
}
