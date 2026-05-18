/*************************************************************************
 *
 * FILE  CUI_CCEnvelopeEditor.cpp
 *
 * DESCRIPTION
 *   The CC Envelope editor (Shift+F6).
 *
 *   Each song carries up to ZTM_MAX_CCENVELOPES envelopes. An envelope
 *   is a sparse `{tick, value}` curve linearly interpolated each subtick
 *   to drive MIDI CC (or Pitchbend / Channel Pressure) output during a
 *   note's lifetime. See `doc/design/cc-envelopes.md` for the runtime
 *   model.
 *
 *   This page is the curve authoring UI. One envelope is in focus at a
 *   time (selected by the Slot slider). Per-envelope metadata sits at
 *   the top (name / cc# / kind / speed / flags / loop+sustain range).
 *   Below, a single "Node N" selector picks which node is being edited;
 *   that node's tick and value get two adjacent sliders. Add / Insert /
 *   Delete buttons modify the node array. A right-hand file picker
 *   loads/saves .env presets from the configured ccenv_folder.
 *
 *   Triggered from the pattern editor via the Vxxyy effect (envelope
 *   slot xx, optional cc# override yy = 0x80 to keep envelope's own
 *   cc) OR auto-armed when a note plays an instrument that has this
 *   envelope listed in its ccenv_default[] slots (see Instrument
 *   Editor).
 *
 ******/
#include "zt.h"

#include "ccenv_io.h"
#include "editor_layout.h"
#include "Button.h"
#include "Sliders.h"

#define BASE_Y          (TRACKS_ROW_Y + 0)
#define SPACE_AT_BOTTOM 8

// Widget IDs -- low IDs first so set_focus(0) lands on the slot slider.
enum {
    W_SLOT = 0,
    W_NAME,
    W_CC,
    W_KIND,
    W_SPEED,
    W_NUMNODES,
    W_FLAG_ENA,
    W_FLAG_LOOP,
    W_FLAG_SUS,
    W_FLAG_CARRY,
    W_LOOP_S,
    W_LOOP_E,
    W_SUS_S,
    W_SUS_E,
    W_NODE_IDX,
    W_NODE_TICK,
    W_NODE_VAL,
    W_BTN_ADD,
    W_BTN_INS,
    W_BTN_DEL,
    W_FILE_LIST,
    W_FILENAME,
    W_BTN_LOAD,
    W_BTN_SAVE
};

static int  ce_slot     = 0;
static int  ce_node_sel = 0;     // currently-selected node index
static char ce_name_buf[ZTM_CCENVNAME_MAXLEN] = {0};
static char ce_filename_buf[256] = {0};
static char ce_folder[1024]      = {0};
static char ce_files[256][256];
static int  ce_num_files         = 0;
static char ce_status[160]       = {0};

// CheckBox->value is `int *` (a pointer to a backing int the page owns).
// Mirror the on/off bits here; ce_push_from_widgets() reads these.
static int ce_flag_ena   = 0;
static int ce_flag_loop  = 0;
static int ce_flag_sus   = 0;
static int ce_flag_carry = 0;

// ---------------------------------------------------------------------------
// Helpers

static ccenvelope *ce_ensure(int slot) {
    if (slot < 0 || slot >= ZTM_MAX_CCENVELOPES) return NULL;
    if (!song->ccenvelopes[slot]) song->ccenvelopes[slot] = new ccenvelope;
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
        // insertItem prepends when unsorted; walk backwards.
        for (int i = ce_num_files - 1; i >= 0; --i) {
            insertItem(ce_files[i]);
        }
    }
    void OnSelectChange() override {
        // Each arrow keypress lands here; mirror the highlighted item
        // into ce_filename_buf so Load/Save use it without extra clicks.
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
} // namespace

// ---------------------------------------------------------------------------
// Refresh / sync

static void ce_pull_to_widgets(UserInterface *UI) {
    ccenvelope *e = song->ccenvelopes[ce_slot];
    int nn = e ? e->num_nodes : 0;
    if (ce_node_sel >= nn && nn > 0) ce_node_sel = nn - 1;
    if (ce_node_sel < 0) ce_node_sel = 0;

    ((ValueSlider *)UI->get_element(W_SLOT))->value     = ce_slot;
    ((ValueSlider *)UI->get_element(W_CC))->value       = e ? e->cc       : 1;
    ((ValueSlider *)UI->get_element(W_KIND))->value     = e ? e->kind     : 0;
    ((ValueSlider *)UI->get_element(W_SPEED))->value    = e ? e->speed    : 1;
    ((ValueSlider *)UI->get_element(W_NUMNODES))->value = nn;
    ce_flag_ena   = e ? !!(e->flags & ZTM_CCENVF_ENABLED) : 0;
    ce_flag_loop  = e ? !!(e->flags & ZTM_CCENVF_LOOP)    : 0;
    ce_flag_sus   = e ? !!(e->flags & ZTM_CCENVF_SUSTAIN) : 0;
    ce_flag_carry = e ? !!(e->flags & ZTM_CCENVF_CARRY)   : 0;
    int max_idx = nn > 0 ? nn - 1 : 0;
    ((ValueSlider *)UI->get_element(W_LOOP_S))->max  = max_idx;
    ((ValueSlider *)UI->get_element(W_LOOP_E))->max  = max_idx;
    ((ValueSlider *)UI->get_element(W_SUS_S))->max   = max_idx;
    ((ValueSlider *)UI->get_element(W_SUS_E))->max   = max_idx;
    ((ValueSlider *)UI->get_element(W_NODE_IDX))->max = max_idx;
    ((ValueSlider *)UI->get_element(W_LOOP_S))->value  = e ? e->loop_start    : 0;
    ((ValueSlider *)UI->get_element(W_LOOP_E))->value  = e ? e->loop_end      : 0;
    ((ValueSlider *)UI->get_element(W_SUS_S))->value   = e ? e->sustain_start : 0;
    ((ValueSlider *)UI->get_element(W_SUS_E))->value   = e ? e->sustain_end   : 0;
    ((ValueSlider *)UI->get_element(W_NODE_IDX))->value = ce_node_sel;

    int t = (e && ce_node_sel < nn) ? e->tick[ce_node_sel]  : 0;
    int v = (e && ce_node_sel < nn) ? e->value[ce_node_sel] : 0;
    ((ValueSlider *)UI->get_element(W_NODE_TICK))->value = t;
    ((ValueSlider *)UI->get_element(W_NODE_VAL))->value  = v;

    ce_load_name(ce_slot);
}

static void ce_push_from_widgets(UserInterface *UI) {
    int v_cc       = ((ValueSlider *)UI->get_element(W_CC))->value;
    int v_kind     = ((ValueSlider *)UI->get_element(W_KIND))->value;
    int v_speed    = ((ValueSlider *)UI->get_element(W_SPEED))->value;
    int v_numnodes = ((ValueSlider *)UI->get_element(W_NUMNODES))->value;
    int v_loop_s   = ((ValueSlider *)UI->get_element(W_LOOP_S))->value;
    int v_loop_e   = ((ValueSlider *)UI->get_element(W_LOOP_E))->value;
    int v_sus_s    = ((ValueSlider *)UI->get_element(W_SUS_S))->value;
    int v_sus_e    = ((ValueSlider *)UI->get_element(W_SUS_E))->value;
    int v_node     = ((ValueSlider *)UI->get_element(W_NODE_IDX))->value;
    int v_tick     = ((ValueSlider *)UI->get_element(W_NODE_TICK))->value;
    int v_val      = ((ValueSlider *)UI->get_element(W_NODE_VAL))->value;
    int f_ena      = ce_flag_ena;
    int f_loop     = ce_flag_loop;
    int f_sus      = ce_flag_sus;
    int f_carry    = ce_flag_carry;

    ccenvelope *e = song->ccenvelopes[ce_slot];
    int e_nn = e ? e->num_nodes : 0;
    int e_cc = e ? e->cc        : 1;
    int e_kind = e ? e->kind    : 0;
    int e_speed = e ? e->speed  : 1;
    int e_loop_s = e ? e->loop_start    : 0;
    int e_loop_e = e ? e->loop_end      : 0;
    int e_sus_s  = e ? e->sustain_start : 0;
    int e_sus_e  = e ? e->sustain_end   : 0;
    int e_flags  = e ? e->flags         : 0;
    int e_tick = (e && v_node < e_nn) ? e->tick[v_node]  : -1;
    int e_val  = (e && v_node < e_nn) ? e->value[v_node] : -1;
    int desired_flags = (f_ena ? ZTM_CCENVF_ENABLED : 0)
                      | (f_loop ? ZTM_CCENVF_LOOP : 0)
                      | (f_sus  ? ZTM_CCENVF_SUSTAIN : 0)
                      | (f_carry ? ZTM_CCENVF_CARRY : 0);

    bool any_change = e && (
        e_cc       != v_cc
     || e_kind     != v_kind
     || e_speed    != v_speed
     || e_nn       != v_numnodes
     || e_loop_s   != v_loop_s
     || e_loop_e   != v_loop_e
     || e_sus_s    != v_sus_s
     || e_sus_e    != v_sus_e
     || e_flags    != desired_flags
     || (v_node < e_nn && (e_tick != v_tick || e_val != v_val))
    );
    bool create_now = !e && (v_numnodes > 0 || v_cc != 1 || desired_flags != 0);

    if (!any_change && !create_now) return;
    e = ce_ensure(ce_slot);
    if (!e) return;
    e->cc       = (unsigned char)v_cc;
    e->kind     = (unsigned char)v_kind;
    e->speed    = (unsigned short)v_speed;
    if (v_numnodes < 0) v_numnodes = 0;
    if (v_numnodes > ZTM_CCENV_MAX_NODES) v_numnodes = ZTM_CCENV_MAX_NODES;
    int old_nn = e->num_nodes;
    e->num_nodes = (unsigned char)v_numnodes;
    // When the user expands the array, default-fill the freshly
    // exposed nodes to a sane ramp tail so they don't appear as 0/0.
    if (v_numnodes > old_nn) {
        int last_t = old_nn > 0 ? e->tick[old_nn - 1] : 0;
        for (int i = old_nn; i < v_numnodes; i++) {
            last_t += 16;
            if (last_t > 65535) last_t = 65535;
            e->tick[i]  = (unsigned short)last_t;
            e->value[i] = e->value[i] ? e->value[i] : 64;
        }
    }
    int max_idx = e->num_nodes > 0 ? e->num_nodes - 1 : 0;
    if (v_loop_s < 0) v_loop_s = 0; if (v_loop_s > max_idx) v_loop_s = max_idx;
    if (v_loop_e < v_loop_s) v_loop_e = v_loop_s; if (v_loop_e > max_idx) v_loop_e = max_idx;
    if (v_sus_s  < 0) v_sus_s  = 0; if (v_sus_s  > max_idx) v_sus_s  = max_idx;
    if (v_sus_e  < v_sus_s)  v_sus_e  = v_sus_s;  if (v_sus_e  > max_idx) v_sus_e  = max_idx;
    e->loop_start    = (unsigned char)v_loop_s;
    e->loop_end      = (unsigned char)v_loop_e;
    e->sustain_start = (unsigned char)v_sus_s;
    e->sustain_end   = (unsigned char)v_sus_e;
    e->flags = (unsigned char)desired_flags;
    if (v_node < e->num_nodes) {
        if (v_tick < 0) v_tick = 0; if (v_tick > 65535) v_tick = 65535;
        if (v_val  < 0) v_val  = 0; if (v_val  > 127)   v_val  = 127;
        e->tick[v_node]  = (unsigned short)v_tick;
        e->value[v_node] = (unsigned char)v_val;
    }
    ce_node_sel = v_node;
    file_changed++;
}

// ---------------------------------------------------------------------------
// Page lifecycle

CUI_CCEnvelopeEditor::CUI_CCEnvelopeEditor(void) {
    UI = new UserInterface;

    ValueSlider *vs;
    TextInput   *ti;
    CheckBox    *cb;
    Button      *bt;

    int LBL_X   = 2;       // label column
    int INP_X   = 14;      // first widget column
    int RIGHT_X = 50;      // right-pane file list

    // Row 0: Slot
    vs = new ValueSlider;
    UI->add_element(vs, W_SLOT);
    vs->x = INP_X; vs->y = BASE_Y + 0; vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_MAX_CCENVELOPES - 1; vs->value = 0;

    // Row 1: Name
    ti = new TextInput;
    UI->add_element(ti, W_NAME);
    ti->frame = 1;
    ti->x = INP_X; ti->y = BASE_Y + 2;
    ti->xsize = 32; ti->length = ZTM_CCENVNAME_MAXLEN - 1;
    ti->str = (unsigned char *)ce_name_buf;

    // Row 2: CC#
    vs = new ValueSlider;
    UI->add_element(vs, W_CC);
    vs->x = INP_X; vs->y = BASE_Y + 4; vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = 127; vs->value = 1;

    // Row 3: Kind (0 CC, 1 PB, 2 CP)
    vs = new ValueSlider;
    UI->add_element(vs, W_KIND);
    vs->x = INP_X; vs->y = BASE_Y + 6; vs->xsize = 8; vs->ysize = 1;
    vs->min = 0; vs->max = 2; vs->value = 0;

    // Row 4: Speed
    vs = new ValueSlider;
    UI->add_element(vs, W_SPEED);
    vs->x = INP_X; vs->y = BASE_Y + 8; vs->xsize = 12; vs->ysize = 1;
    vs->min = 1; vs->max = 255; vs->value = 1;

    // Row 5: NumNodes
    vs = new ValueSlider;
    UI->add_element(vs, W_NUMNODES);
    vs->x = INP_X; vs->y = BASE_Y + 10; vs->xsize = 12; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_CCENV_MAX_NODES; vs->value = 0;

    // Row 6..9: flag checkboxes -- CheckBox::value is `int *` pointing
    // at the backing flag bit. ce_pull_to_widgets / ce_push_from_widgets
    // sync these bits with the envelope's flags byte.
    cb = new CheckBox; UI->add_element(cb, W_FLAG_ENA);
    cb->x = INP_X     ; cb->y = BASE_Y + 12; cb->value = &ce_flag_ena;
    cb = new CheckBox; UI->add_element(cb, W_FLAG_LOOP);
    cb->x = INP_X + 14; cb->y = BASE_Y + 12; cb->value = &ce_flag_loop;
    cb = new CheckBox; UI->add_element(cb, W_FLAG_SUS);
    cb->x = INP_X + 24; cb->y = BASE_Y + 12; cb->value = &ce_flag_sus;
    cb = new CheckBox; UI->add_element(cb, W_FLAG_CARRY);
    cb->x = INP_X + 38; cb->y = BASE_Y + 12; cb->value = &ce_flag_carry;

    // Loop start / end
    vs = new ValueSlider; UI->add_element(vs, W_LOOP_S);
    vs->x = INP_X; vs->y = BASE_Y + 14; vs->xsize = 8; vs->ysize = 1;
    vs->min = 0; vs->max = 0; vs->value = 0;
    vs = new ValueSlider; UI->add_element(vs, W_LOOP_E);
    vs->x = INP_X + 14; vs->y = BASE_Y + 14; vs->xsize = 8; vs->ysize = 1;
    vs->min = 0; vs->max = 0; vs->value = 0;

    // Sustain start / end
    vs = new ValueSlider; UI->add_element(vs, W_SUS_S);
    vs->x = INP_X; vs->y = BASE_Y + 16; vs->xsize = 8; vs->ysize = 1;
    vs->min = 0; vs->max = 0; vs->value = 0;
    vs = new ValueSlider; UI->add_element(vs, W_SUS_E);
    vs->x = INP_X + 14; vs->y = BASE_Y + 16; vs->xsize = 8; vs->ysize = 1;
    vs->min = 0; vs->max = 0; vs->value = 0;

    // Node selector + node tick + node value
    vs = new ValueSlider; UI->add_element(vs, W_NODE_IDX);
    vs->x = INP_X; vs->y = BASE_Y + 19; vs->xsize = 8; vs->ysize = 1;
    vs->min = 0; vs->max = 0; vs->value = 0;
    vs = new ValueSlider; UI->add_element(vs, W_NODE_TICK);
    vs->x = INP_X; vs->y = BASE_Y + 21; vs->xsize = 14; vs->ysize = 1;
    vs->min = 0; vs->max = 65535; vs->value = 0;
    vs = new ValueSlider; UI->add_element(vs, W_NODE_VAL);
    vs->x = INP_X; vs->y = BASE_Y + 23; vs->xsize = 14; vs->ysize = 1;
    vs->min = 0; vs->max = 127; vs->value = 0;

    // Node Add / Insert / Delete buttons
    bt = new Button; UI->add_element(bt, W_BTN_ADD);
    bt->x = INP_X     ; bt->y = BASE_Y + 25; bt->xsize = 6; bt->ysize = 1;
    bt->caption = "Add";
    bt = new Button; UI->add_element(bt, W_BTN_INS);
    bt->x = INP_X + 8 ; bt->y = BASE_Y + 25; bt->xsize = 6; bt->ysize = 1;
    bt->caption = "Ins";
    bt = new Button; UI->add_element(bt, W_BTN_DEL);
    bt->x = INP_X + 16; bt->y = BASE_Y + 25; bt->xsize = 6; bt->ysize = 1;
    bt->caption = "Del";

    // File picker (right pane)
    CeFileList *fl = new CeFileList;
    UI->add_element(fl, W_FILE_LIST);
    fl->x = RIGHT_X; fl->y = BASE_Y + 2;
    fl->xsize = 28;  fl->ysize = 16;

    // Filename input + Load / Save buttons
    ti = new TextInput; UI->add_element(ti, W_FILENAME);
    ti->frame = 1; ti->x = RIGHT_X; ti->y = BASE_Y + 20;
    ti->xsize = 28; ti->length = sizeof ce_filename_buf - 1;
    ti->str = (unsigned char *)ce_filename_buf;

    bt = new Button; UI->add_element(bt, W_BTN_LOAD);
    bt->x = RIGHT_X     ; bt->y = BASE_Y + 22; bt->xsize = 10; bt->ysize = 1;
    bt->caption = "Load";
    bt = new Button; UI->add_element(bt, W_BTN_SAVE);
    bt->x = RIGHT_X + 12; bt->y = BASE_Y + 22; bt->xsize = 10; bt->ysize = 1;
    bt->caption = "Save";
}

void CUI_CCEnvelopeEditor::enter(void) {
    need_refresh++;
    cur_state = STATE_CCENVELOPE;
    ce_rescan_folder();
    ((CeFileList *)UI->get_element(W_FILE_LIST))->OnChange();
    ce_pull_to_widgets(UI);
    UI->set_focus(W_SLOT);
    Keys.flush();
    ce_set_status("CC Envelope Editor -- Shift+F6. Slot=%d, %d files in folder.",
                  ce_slot, ce_num_files);
}

void CUI_CCEnvelopeEditor::leave(void) {
    ce_store_name(ce_slot);
}

void CUI_CCEnvelopeEditor::update(void) {
    UI->update();

    // Slot changed?
    ValueSlider *vs_slot = (ValueSlider *)UI->get_element(W_SLOT);
    if (vs_slot && vs_slot->value != ce_slot) {
        ce_store_name(ce_slot);
        ce_slot = vs_slot->value;
        ce_node_sel = 0;
        ce_pull_to_widgets(UI);
        need_refresh++; doredraw++;
    }

    // Node selector changed -> reload tick/value sliders to that node's
    // current data without writing back.
    ValueSlider *vs_ni = (ValueSlider *)UI->get_element(W_NODE_IDX);
    if (vs_ni && vs_ni->value != ce_node_sel) {
        ce_node_sel = vs_ni->value;
        ccenvelope *e = song->ccenvelopes[ce_slot];
        if (e && ce_node_sel < e->num_nodes) {
            ((ValueSlider *)UI->get_element(W_NODE_TICK))->value = e->tick[ce_node_sel];
            ((ValueSlider *)UI->get_element(W_NODE_VAL))->value  = e->value[ce_node_sel];
        }
        need_refresh++; doredraw++;
    }

    // Name changed
    TextInput *ti_name = (TextInput *)UI->get_element(W_NAME);
    if (ti_name && ti_name->changed) {
        ce_store_name(ce_slot);
        ti_name->changed = 0;
    }

    // Buttons
    Button *b;
    b = (Button *)UI->get_element(W_BTN_ADD);
    if (b && b->changed) {
        ccenvelope *e = ce_ensure(ce_slot);
        if (e && e->num_nodes < ZTM_CCENV_MAX_NODES) {
            int t = e->num_nodes > 0 ? e->tick[e->num_nodes - 1] + 16 : 0;
            if (t > 65535) t = 65535;
            int v = e->num_nodes > 0 ? e->value[e->num_nodes - 1]      : 64;
            e->tick[e->num_nodes]  = (unsigned short)t;
            e->value[e->num_nodes] = (unsigned char)v;
            e->num_nodes++;
            ce_node_sel = e->num_nodes - 1;
            file_changed++;
            ce_set_status("Added node %d (tick %d, value %d)", ce_node_sel, t, v);
            ce_pull_to_widgets(UI);
        } else {
            ce_set_status("Node limit reached (%d)", ZTM_CCENV_MAX_NODES);
        }
        b->changed = 0; need_refresh++; doredraw++;
    }
    b = (Button *)UI->get_element(W_BTN_INS);
    if (b && b->changed) {
        ccenvelope *e = ce_ensure(ce_slot);
        if (e && e->num_nodes < ZTM_CCENV_MAX_NODES) {
            int ins = ce_node_sel;
            if (ins < 0) ins = 0;
            if (ins > e->num_nodes) ins = e->num_nodes;
            for (int k = e->num_nodes; k > ins; k--) {
                e->tick[k]  = e->tick[k-1];
                e->value[k] = e->value[k-1];
            }
            int t0 = ins > 0 ? e->tick[ins - 1] : 0;
            int t1 = ins < e->num_nodes ? e->tick[ins] : t0 + 16;
            int v0 = ins > 0 ? e->value[ins - 1] : 0;
            int v1 = ins < e->num_nodes ? e->value[ins] : v0;
            e->tick[ins]  = (unsigned short)((t0 + t1) / 2);
            e->value[ins] = (unsigned char)((v0 + v1) / 2);
            e->num_nodes++;
            ce_node_sel = ins;
            file_changed++;
            ce_set_status("Inserted node at %d", ins);
            ce_pull_to_widgets(UI);
        }
        b->changed = 0; need_refresh++; doredraw++;
    }
    b = (Button *)UI->get_element(W_BTN_DEL);
    if (b && b->changed) {
        ccenvelope *e = song->ccenvelopes[ce_slot];
        if (e && e->num_nodes > 0 && ce_node_sel < e->num_nodes) {
            for (int k = ce_node_sel; k + 1 < e->num_nodes; k++) {
                e->tick[k]  = e->tick[k+1];
                e->value[k] = e->value[k+1];
            }
            e->num_nodes--;
            if (ce_node_sel >= e->num_nodes && e->num_nodes > 0)
                ce_node_sel = e->num_nodes - 1;
            file_changed++;
            ce_set_status("Deleted node");
            ce_pull_to_widgets(UI);
        }
        b->changed = 0; need_refresh++; doredraw++;
    }
    b = (Button *)UI->get_element(W_BTN_LOAD);
    if (b && b->changed) {
        if (ce_filename_buf[0]) {
            char path[2048];
            snprintf(path, sizeof path, "%s/%s", ce_folder, ce_filename_buf);
            ccenvelope *e = ce_ensure(ce_slot);
            if (e && ccenv_read_file(path, e) == 0) {
                ce_set_status("Loaded %s into slot %d", ce_filename_buf, ce_slot);
                file_changed++;
                ce_node_sel = 0;
                ce_pull_to_widgets(UI);
            } else {
                ce_set_status("Failed to load %s", ce_filename_buf);
            }
        } else {
            ce_set_status("No filename selected.");
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
                // Ensure the basename ends in .env
                int n = (int)strlen(path);
                if (n < 4 || strcasecmp(path + n - 4, ".env") != 0) {
                    if (n + 4 < (int)sizeof path) { strcat(path, ".env"); }
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

    // Sync all other widgets back to the envelope struct.
    ce_push_from_widgets(UI);

    if (Keys.size()) {
        KBKey k = Keys.checkkey();
        if (k == SDLK_ESCAPE) {
            Keys.getkey();
            switch_page(UIP_Patterneditor);
            need_refresh++;
        }
    }
}

// ASCII curve preview: 60 cols wide, 8 rows tall. Walks tick range
// and prints '*' at interpolated value. Just below the right pane.
static void ce_draw_curve(Drawable *S) {
    ccenvelope *e = song->ccenvelopes[ce_slot];
    if (!e || e->num_nodes < 2) return;

    const int W = 60;
    const int H = 8;
    int origin_x = col(2);
    int origin_y = row(BASE_Y + 28);

    // Border row -- visual frame for the curve area.
    char hdr[80];
    snprintf(hdr, sizeof hdr,
             "Curve  [tick 0..%u, value 0..127]",
             (unsigned)e->tick[e->num_nodes - 1]);
    print(origin_x, origin_y, hdr, COLORS.Text, S);

    int last_tick = e->tick[e->num_nodes - 1];
    if (last_tick <= 0) last_tick = 1;

    // 8 row plot. Row 0 is top (value 127), row H-1 is bottom (0).
    for (int rxy = 0; rxy < H; rxy++) {
        char line[80];
        for (int x = 0; x < W; x++) {
            int pos = (x * last_tick) / (W - 1);
            int v = 0;
            // Linear scan, fine for <=32 nodes.
            if (pos <= e->tick[0]) v = e->value[0];
            else if (pos >= e->tick[e->num_nodes - 1]) v = e->value[e->num_nodes - 1];
            else {
                int i;
                for (i = 1; i < e->num_nodes; i++) if (e->tick[i] >= pos) break;
                int x0 = e->tick[i - 1], x1 = e->tick[i];
                int y0 = e->value[i - 1], y1 = e->value[i];
                v = (x1 > x0) ? y0 + ((pos - x0) * (y1 - y0)) / (x1 - x0) : y1;
            }
            int rowval = (H - 1) - ((v * (H - 1)) / 127);
            line[x] = (rxy == rowval) ? '*' : (rxy == H - 1 ? '_' : ' ');
        }
        line[W] = 0;
        print(origin_x, origin_y + (1 + rxy) * 8, line, COLORS.Text, S);
    }
}

void CUI_CCEnvelopeEditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);
    printtitle(PAGE_TITLE_ROW_Y, "CC Envelope Editor (Shift+F6)",
               COLORS.Text, COLORS.Background, S);

    int LBL_X = 2;
    print(col(LBL_X), row(BASE_Y + 0),  "Slot",       COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 2),  "Name",       COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 4),  "CC#",        COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 6),  "Kind",       COLORS.Text, S);
    {
        ccenvelope *e = song->ccenvelopes[ce_slot];
        const char *knd = "CC";
        if (e) {
            if      (e->kind == 1) knd = "Pitchbend";
            else if (e->kind == 2) knd = "ChanPress";
        }
        char buf[24]; snprintf(buf, sizeof buf, "(%s)", knd);
        print(col(LBL_X + 24), row(BASE_Y + 6), buf, COLORS.Text, S);
    }
    print(col(LBL_X), row(BASE_Y + 8),  "Speed",      COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 10), "NumNodes",   COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 12), "Flags",      COLORS.Text, S);
    print(col(14    ), row(BASE_Y + 12), "On",        COLORS.Text, S);
    print(col(14+11), row(BASE_Y + 12), "Loop",       COLORS.Text, S);
    print(col(14+21), row(BASE_Y + 12), "Sustain",    COLORS.Text, S);
    print(col(14+35), row(BASE_Y + 12), "Carry",      COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 14), "Loop S..E",  COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 16), "Sus  S..E",  COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 19), "Node",       COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 21), "Tick",       COLORS.Text, S);
    print(col(LBL_X), row(BASE_Y + 23), "Value",      COLORS.Text, S);

    // Right pane labels
    int RIGHT_X = 50;
    print(col(RIGHT_X), row(BASE_Y + 1),  ".env presets",      COLORS.Text, S);
    print(col(RIGHT_X), row(BASE_Y + 19), "Filename",          COLORS.Text, S);

    // Folder line
    {
        char buf[160];
        snprintf(buf, sizeof buf, "Folder: %s",
                 ce_folder[0] ? ce_folder : "(none)");
        print(col(RIGHT_X), row(BASE_Y + 25), buf, COLORS.Text, S);
    }

    // Status line
    if (ce_status[0]) {
        print(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 2), ce_status,
              COLORS.Text, S);
    }

    // Hint line
    print(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 1),
          "Tab = next field, Enter = activate button, ESC = back",
          COLORS.Text, S);

    ce_draw_curve(S);

    need_refresh = 0; updated = 2;
    ztPlayer->num_orders();
    S->unlock();
}
