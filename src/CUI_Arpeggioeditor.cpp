/*************************************************************************
 *
 * FILE  CUI_Arpeggioeditor.cpp
 *
 * DESCRIPTION
 *   The arpeggio editor (Shift+F4).
 *
 *   Each song carries up to ZTM_MAX_ARPEGGIOS (256) arpeggios. Each has
 *   a name, length (number of steps), speed (subticks per step),
 *   repeat position (0..length-1), num_cc (0..ZTM_ARPEGGIO_NUM_CC), an
 *   array of cc[] controller numbers, and per-step pitch[] + ccval[][]
 *   tables.
 *
 *   pitch[i] is unsigned short; 0x8000 (ZTM_ARP_EMPTY_PITCH) = no
 *   pitch event at this step. Other values are signed semitone offsets
 *   (reinterpreted as int16). ccval[c][i] is unsigned char; 0x80
 *   (ZTM_ARP_EMPTY_CCVAL) = no CC event. Other values are 0..127.
 *
 *   On disk: zt_module::build_arpeggio / load_arpeggio (ARPG chunk)
 *   round-trip these structures through .zt save/load.
 *
 *   UI layout (one slider per row, blank row between each slider for
 *   breathing room; documentation sits below the grid to mirror the
 *   CUI_Midimacroeditor placement):
 *     Slot    [slider]
 *
 *     Name    [textinput]
 *
 *     Length  [slider]
 *
 *     Speed   [slider]
 *
 *     Repeat  [slider]
 *
 *     NumCC   [slider]
 *
 *     CC#:    [s0] [s1] [s2] [s3]
 *
 *     STEP  PIT  C0  C1  ...
 *     000   ...  ... ...
 *
 *     Trigger from pattern: R<slot> ...   (highlighted)
 *     Tab into grid; arrows nav cells; ...
 *     '.' clear; P=preset; ...
 *
 *   The data grid is reachable via Tab from the last metadata slider;
 *   arrows then navigate cells. Tab again leaves the grid back to the
 *   slot slider.
 *
 ******/
#include "zt.h"

#define BASE_Y          (TRACKS_ROW_Y + 0)
// SPACE_AT_BOTTOM mirrors the CUI_Patterneditor constant: 8 rows are
// reserved at the bottom of the screen for the toolbar.
#define SPACE_AT_BOTTOM 8
#define GRID_X          4
#define GRID_HDR_Y      (BASE_Y + 14)
#define GRID_Y          (BASE_Y + 15)
#define GRID_VISIBLE    14
#define GRID_ID         10
#define PRESET_LIST_ID  11

static int  ar_slot      = 0;
static int  ar_cur_step  = 0;
static int  ar_cur_col   = 0;       // 0 = pitch, 1..num_cc = ccval column
static int  ar_view_top  = 0;
static int  ar_cur_digit = 0;       // 0=hundreds, 1=tens, 2=ones

static char ar_name_buf[ZTM_ARPEGGIONAME_MAXLEN] = {0};

// ---------------------------------------------------------------------------
// Helpers

static arpeggio *ar_ensure(int slot) {
    if (slot < 0 || slot >= ZTM_MAX_ARPEGGIOS) return NULL;
    if (!song->arpeggios[slot]) song->arpeggios[slot] = new arpeggio;
    return song->arpeggios[slot];
}

static void ar_load_name(int slot) {
    arpeggio *a = song->arpeggios[slot];
    if (a) {
        memcpy(ar_name_buf, a->name, ZTM_ARPEGGIONAME_MAXLEN);
        ar_name_buf[ZTM_ARPEGGIONAME_MAXLEN - 1] = 0;
    } else {
        ar_name_buf[0] = 0;
    }
}

static void ar_store_name(int slot) {
    bool any_text = (ar_name_buf[0] != 0);
    arpeggio *cur = song->arpeggios[slot];
    if (!any_text && (!cur || cur->isempty())) return;
    arpeggio *a = ar_ensure(slot);
    if (!a) return;
    if (memcmp(a->name, ar_name_buf, ZTM_ARPEGGIONAME_MAXLEN) != 0) {
        memcpy(a->name, ar_name_buf, ZTM_ARPEGGIONAME_MAXLEN);
        a->name[ZTM_ARPEGGIONAME_MAXLEN - 1] = 0;
        file_changed++;
    }
}

static void format_pitch(unsigned short v, char buf[4]) {
    if (v == ZTM_ARP_EMPTY_PITCH) { strcpy(buf, "..."); return; }
    int p = (int16_t)v;
    if (p < 0)      snprintf(buf, 4, "-%02d", -p);
    else            snprintf(buf, 4, "+%02d", p);
}

static void format_ccval(unsigned char v, char buf[4]) {
    if (v == ZTM_ARP_EMPTY_CCVAL) { strcpy(buf, "..."); return; }
    snprintf(buf, 4, "%03d", v);
}

// ---------------------------------------------------------------------------
// Presets

struct ar_preset {
    const char *name;
    int length;
    int speed;
    int repeat_pos;
    int pitches[16];   // -127..127 or 0x8000 sentinel via cast
    int len_pitches;
};

static const ar_preset AR_PRESETS[] = {
    { "Major Triad Up",        3,  6, 0, { 0, 4, 7 },                  3 },
    { "Minor Triad Up",        3,  6, 0, { 0, 3, 7 },                  3 },
    { "Major Triad Up+Octave", 4,  6, 0, { 0, 4, 7, 12 },              4 },
    { "Octave Bounce",         2,  6, 0, { 0, 12 },                    2 },
    { "Trill (whole step)",    2,  3, 0, { 0, 2 },                     2 },
    { "Trill (half step)",     2,  3, 0, { 0, 1 },                     2 },
    { "Major 7 Chord",         4,  6, 0, { 0, 4, 7, 11 },              4 },
    { "Minor 7 Chord",         4,  6, 0, { 0, 3, 7, 10 },              4 },
    { "Major Scale (1 oct)",   8,  4, 0, { 0, 2, 4, 5, 7, 9, 11, 12 }, 8 },
    { "Minor Scale (1 oct)",   8,  4, 0, { 0, 2, 3, 5, 7, 8, 10, 12 }, 8 },
};
static const int AR_PRESET_COUNT = sizeof(AR_PRESETS) / sizeof(AR_PRESETS[0]);
static int  ar_preset_index = 0;
// Set by ArPresetSelector::OnSelect when Enter/Space lands on a preset.
// CUI_Arpeggioeditor::update() polls this BEFORE the slider->arpeggio
// sync block so the freshly-applied preset's length/speed/etc. survive
// (otherwise stale slider widget values would immediately overwrite the
// preset on the next frame, which looked like a UI freeze on Enter and
// "Space selects but values don't change" on Space).
static bool ar_preset_just_applied = false;

static void ar_apply_preset(int idx) {
    if (idx < 0 || idx >= AR_PRESET_COUNT) return;
    arpeggio *a = ar_ensure(ar_slot);
    if (!a) return;
    const ar_preset &p = AR_PRESETS[idx];
    a->length     = p.length;
    a->speed      = p.speed;
    a->repeat_pos = p.repeat_pos;
    a->num_cc     = 0;
    for (int i = 0; i < ZTM_ARPEGGIO_LEN; ++i) a->pitch[i] = ZTM_ARP_EMPTY_PITCH;
    for (int i = 0; i < p.len_pitches; ++i)
        a->pitch[i] = (unsigned short)(int16_t)p.pitches[i];
    // Always overwrite the name to follow the preset selection;
    // without this, cycling through P after the first apply leaves
    // the name stuck on the very first preset.
    memset(a->name, 0, ZTM_ARPEGGIONAME_MAXLEN);
    strncpy(a->name, p.name, ZTM_ARPEGGIONAME_MAXLEN - 1);
    memset(ar_name_buf, 0, sizeof(ar_name_buf));
    memcpy(ar_name_buf, a->name, ZTM_ARPEGGIONAME_MAXLEN);
    file_changed++;
    sprintf(szStatmsg, "Applied preset: %s", p.name);
    statusmsg = szStatmsg;
    status_change = 1;
}

// ---------------------------------------------------------------------------
// Grid focus stub

namespace {
class ArGridFocus : public UserInterfaceElement {
public:
    ArGridFocus() { xsize = 1; ysize = 1; }
    int update() override { return 0; }
    void draw(Drawable *S, int active) override { (void)S; (void)active; }
};

// Inline preset selector -- mirrors the F11 SkinSelector ergonomics so
// the full preset catalogue is visible (replaces the invisible P-cycle).
class ArPresetSelector : public ListBox {
public:
    ArPresetSelector() {
        empty_message = "No presets";
        is_sorted = false;
        use_checks = true;
        use_key_select = true;
        OnChange();
    }
    void OnChange() override {
        clear();
        // insertItem prepends when is_sorted=false, so walk backwards to
        // preserve the array's natural order in the visible list.
        for (int i = AR_PRESET_COUNT - 1; i >= 0; --i) {
            LBNode *p = insertItem((char *)AR_PRESETS[i].name);
            if (p) {
                p->int_data = i;
                if (i == ar_preset_index) p->checked = true;
            }
        }
    }
    int update() override {
        // Robustness pass over ListBox: keep focus on edge Up/Down keys
        // (parent's default surrenders focus on first Up after click).
        // Accept Space as a synonym for Enter.
        KBKey k = Keys.checkkey();
        if (k == SDLK_SPACE) {
            Keys.getkey();
            LBNode *n = getNode(cur_sel + y_start);
            if (n) OnSelect(n);
            need_refresh++;
            need_redraw++;
            return 0;
        }
        if (k == SDLK_UP && cur_sel == 0 && y_start == 0) {
            Keys.getkey();
            return 0;
        }
        if (k == SDLK_DOWN && cur_sel + y_start >= num_elements - 1) {
            Keys.getkey();
            return 0;
        }
        return ListBox::update();
    }
    void OnSelect(LBNode *selected) override {
        if (!selected) return;
        ar_preset_index = selected->int_data;
        ar_apply_preset(ar_preset_index);
        selectNone();
        selected->checked = true;
        ar_preset_just_applied = true;
        ListBox::OnSelect(selected);
    }
    void OnSelectChange() override {}
};
} // namespace

// ---------------------------------------------------------------------------
// Constructor / lifecycle

CUI_Arpeggioeditor::CUI_Arpeggioeditor(void) {
    UI = new UserInterface;

    ValueSlider *vs;
    TextInput   *ti;

    // 0 -- Slot
    vs = new ValueSlider;
    UI->add_element(vs, 0);
    vs->x = 12; vs->y = BASE_Y;     vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_MAX_ARPEGGIOS - 1; vs->value = 0;

    // 1 -- Name
    ti = new TextInput;
    UI->add_element(ti, 1);
    ti->frame = 1;
    ti->x = 12; ti->y = BASE_Y + 2;
    ti->xsize = 32; ti->length = ZTM_ARPEGGIONAME_MAXLEN - 1;
    ti->str = (unsigned char*)ar_name_buf;

    // 2 -- Length
    vs = new ValueSlider;
    UI->add_element(vs, 2);
    vs->x = 12; vs->y = BASE_Y + 4; vs->xsize = 18; vs->ysize = 1;
    vs->min = 1; vs->max = ZTM_ARPEGGIO_LEN; vs->value = 1;

    // 3 -- Speed
    vs = new ValueSlider;
    UI->add_element(vs, 3);
    vs->x = 12; vs->y = BASE_Y + 6; vs->xsize = 18; vs->ysize = 1;
    vs->min = 1; vs->max = 255; vs->value = 1;

    // 4 -- Repeat
    vs = new ValueSlider;
    UI->add_element(vs, 4);
    vs->x = 12; vs->y = BASE_Y + 8; vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_ARPEGGIO_LEN - 1; vs->value = 0;

    // 5 -- NumCC
    vs = new ValueSlider;
    UI->add_element(vs, 5);
    vs->x = 12; vs->y = BASE_Y + 10; vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_ARPEGGIO_NUM_CC; vs->value = 0;

    // 6..9 -- CC#
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i) {
        vs = new ValueSlider;
        UI->add_element(vs, 6 + i);
        vs->x = 12 + i * 16; vs->y = BASE_Y + 12;
        vs->xsize = 8; vs->ysize = 1;
        vs->min = 0; vs->max = 127; vs->value = 0;
    }

    // 10 -- Grid focus stub
    ArGridFocus *gf = new ArGridFocus;
    UI->add_element(gf, GRID_ID);
    gf->x = GRID_X; gf->y = GRID_Y;

    // 11 -- Preset list (always-visible, SkinSelector-style).
    ArPresetSelector *ps = new ArPresetSelector;
    UI->add_element(ps, PRESET_LIST_ID);
    ps->x = 47; ps->y = BASE_Y + 2;
    ps->xsize = 32;
    ps->ysize = AR_PRESET_COUNT - 1;
}

// Migrate legacy out-of-range values (loaded from old .zt files) into
// the slider-valid range silently. ZTM_ARP_EMPTY_CC = 0x80 used to be
// the default cc[i] but is out of range for the cc slider (max 127);
// num_cc default used to be ZTM_ARPEGGIO_NUM_CC even for an empty arp;
// speed used to be uninitialised. Migrate in place without setting
// file_changed so a fresh load doesn't appear "modified".
static void ar_migrate_legacy(arpeggio *a) {
    if (!a) return;
    if (a->speed < 1 || a->speed > 255)              a->speed = 1;
    if (a->num_cc < 0 || a->num_cc > ZTM_ARPEGGIO_NUM_CC) a->num_cc = 0;
    if (a->length < 0 || a->length > ZTM_ARPEGGIO_LEN)    a->length = 0;
    if (a->repeat_pos < 0)                                a->repeat_pos = 0;
    if (a->length > 0 && a->repeat_pos >= a->length)      a->repeat_pos = a->length - 1;
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i) {
        if (a->cc[i] > 0x7F) a->cc[i] = 0;
    }
}

void CUI_Arpeggioeditor::enter(void) {
    need_refresh++;
    cur_state = STATE_ARPEDIT;
    ar_load_name(ar_slot);

    arpeggio *a = song->arpeggios[ar_slot];
    ar_migrate_legacy(a);

    ((ValueSlider *)UI->get_element(0))->value = ar_slot;
    ((ValueSlider *)UI->get_element(2))->value = a ? a->length     : 1;
    ((ValueSlider *)UI->get_element(3))->value = a ? a->speed      : 1;
    ((ValueSlider *)UI->get_element(4))->value = a ? a->repeat_pos : 0;
    ((ValueSlider *)UI->get_element(5))->value = a ? a->num_cc     : 0;
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i)
        ((ValueSlider *)UI->get_element(6 + i))->value = a ? a->cc[i] : 0;

    // Clamp cursor in case the slot was previously edited at a step
    // that's now out of range.
    int max_step = a && a->length > 0 ? a->length - 1 : 0;
    if (ar_cur_step > max_step) ar_cur_step = max_step;
    int max_col = a ? a->num_cc : 0;
    if (ar_cur_col > max_col) ar_cur_col = max_col;
    if (ar_view_top > ar_cur_step) ar_view_top = ar_cur_step;

    UI->set_focus(0);
    Keys.flush();
}

void CUI_Arpeggioeditor::leave(void) {
    ar_store_name(ar_slot);
}

// ---------------------------------------------------------------------------

void CUI_Arpeggioeditor::update() {
    UI->update();

    ValueSlider *vs;
    TextInput   *ti;

    // Preset just applied via the listbox -- pull arpeggio data into
    // the slider widgets BEFORE the slider->arpeggio sync block, or
    // stale widget values will overwrite the preset back to defaults.
    if (ar_preset_just_applied) {
        ar_preset_just_applied = false;
        arpeggio *a = song->arpeggios[ar_slot];
        if (a) {
            ((ValueSlider *)UI->get_element(2))->value = a->length;
            ((ValueSlider *)UI->get_element(3))->value = a->speed;
            ((ValueSlider *)UI->get_element(4))->value = a->repeat_pos;
            ((ValueSlider *)UI->get_element(5))->value = a->num_cc;
            for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i)
                ((ValueSlider *)UI->get_element(6 + i))->value = a->cc[i];
            TextInput *t = (TextInput *)UI->get_element(1);
            if (t) t->cursor = 0;
        }
        need_refresh++;
        doredraw++;
    }

    // Slot change
    vs = (ValueSlider *)UI->get_element(0);
    if (vs && vs->value != ar_slot) {
        ar_store_name(ar_slot);
        ar_slot = vs->value;
        ar_cur_step = ar_view_top = ar_cur_col = 0;
        ar_cur_digit = 0;
        ar_load_name(ar_slot);
        arpeggio *a = song->arpeggios[ar_slot];
        ar_migrate_legacy(a);
        ((ValueSlider *)UI->get_element(2))->value = a ? a->length     : 1;
        ((ValueSlider *)UI->get_element(3))->value = a ? a->speed      : 1;
        ((ValueSlider *)UI->get_element(4))->value = a ? a->repeat_pos : 0;
        ((ValueSlider *)UI->get_element(5))->value = a ? a->num_cc     : 0;
        for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i)
            ((ValueSlider *)UI->get_element(6 + i))->value = a ? a->cc[i] : 0;
        ti = (TextInput *)UI->get_element(1);
        if (ti) ti->cursor = 0;
        need_refresh++;
        doredraw++;
    }

    // Name typed
    ti = (TextInput *)UI->get_element(1);
    if (ti && ti->changed) { ar_store_name(ar_slot); ti->changed = 0; }

    // Metadata sliders → arp entry. Auto-create only when the user
    // actually moves a slider away from defaults (so just visiting a
    // slot doesn't allocate empty arpeggios).
    int v_len    = ((ValueSlider *)UI->get_element(2))->value;
    int v_speed  = ((ValueSlider *)UI->get_element(3))->value;
    int v_repeat = ((ValueSlider *)UI->get_element(4))->value;
    int v_numcc  = ((ValueSlider *)UI->get_element(5))->value;
    int v_cc[ZTM_ARPEGGIO_NUM_CC];
    for (int k = 0; k < ZTM_ARPEGGIO_NUM_CC; ++k)
        v_cc[k] = ((ValueSlider *)UI->get_element(6 + k))->value;

    arpeggio *a = song->arpeggios[ar_slot];
    bool any_change = a && (
        a->length     != v_len    ||
        a->speed      != v_speed  ||
        a->repeat_pos != v_repeat ||
        a->num_cc     != v_numcc  ||
        a->cc[0] != (unsigned char)v_cc[0] ||
        a->cc[1] != (unsigned char)v_cc[1] ||
        a->cc[2] != (unsigned char)v_cc[2] ||
        a->cc[3] != (unsigned char)v_cc[3]);
    bool create_now = !a && (v_len != 1 || v_speed != 1 || v_repeat != 0 || v_numcc != 0
                             || v_cc[0] || v_cc[1] || v_cc[2] || v_cc[3]);
    if (create_now) { a = ar_ensure(ar_slot); any_change = true; }

    if (a && any_change) {
        a->length     = v_len;
        a->speed      = v_speed;
        a->repeat_pos = v_repeat;
        a->num_cc     = v_numcc;
        for (int k = 0; k < ZTM_ARPEGGIO_NUM_CC; ++k)
            a->cc[k] = (unsigned char)v_cc[k];
        // Keep repeat_pos sane wrt new length.
        if (a->length > 0 && a->repeat_pos >= a->length) {
            a->repeat_pos = a->length - 1;
            ((ValueSlider *)UI->get_element(4))->value = a->repeat_pos;
        }
        file_changed++;
        need_refresh++;
    }

    // Cursor sanity: keep cur_step inside [0, length) and cur_col
    // inside [0, num_cc] every frame, in case length/num_cc shrank.
    {
        arpeggio *cur = song->arpeggios[ar_slot];
        int len     = cur ? cur->length : 1;
        int max_col = cur ? cur->num_cc : 0;
        if (len < 1) len = 1;
        if (ar_cur_step >= len) ar_cur_step = len - 1;
        if (ar_cur_step < 0)    ar_cur_step = 0;
        if (ar_cur_col > max_col) ar_cur_col = max_col;
        if (ar_cur_col < 0)       ar_cur_col = 0;
        if (ar_view_top > ar_cur_step) ar_view_top = ar_cur_step;
    }

    // Page-level P / Shift+Del / Ctrl+Del -- work from any focus that
    // isn't the Name TextInput. Without this, pressing P on a slider
    // wedges the app: the slider doesn't consume P, the grid handler
    // only fires on the grid stub, so the keypress sticks in the
    // Keys queue and never gets popped.
    int focused = UI->cur_element;
    {
        KBKey pkey = Keys.checkkey();
        int   pks  = Keys.getstate();
        if (pkey && focused != 1) {
            bool page_consumed = false;
            if (pkey == SDLK_P && !(pks & (KS_CTRL|KS_ALT|KS_META|KS_SHIFT))
                && focused != PRESET_LIST_ID) {
                ar_preset_index = (ar_preset_index + 1) % AR_PRESET_COUNT;
                ar_apply_preset(ar_preset_index);
                arpeggio *na = song->arpeggios[ar_slot];
                ((ValueSlider *)UI->get_element(2))->value = na->length;
                ((ValueSlider *)UI->get_element(3))->value = na->speed;
                ((ValueSlider *)UI->get_element(4))->value = na->repeat_pos;
                ((ValueSlider *)UI->get_element(5))->value = na->num_cc;
                TextInput *tn = (TextInput *)UI->get_element(1);
                if (tn) tn->cursor = 0;
                ListBox *psel = dynamic_cast<ListBox *>(UI->get_element(PRESET_LIST_ID));
                if (psel) {
                    psel->selectNone();
                    psel->setCheck(ar_preset_index, true);
                    psel->setCursor(ar_preset_index);
                }
                page_consumed = true;
            }
            else if ((pkey == SDLK_DELETE || pkey == SDLK_BACKSPACE)
                     && (pks & KS_SHIFT) && !(pks & KS_CTRL)) {
                arpeggio *aw = song->arpeggios[ar_slot];
                if (aw) {
                    for (int s = 0; s < ZTM_ARPEGGIO_LEN; ++s) {
                        aw->pitch[s] = ZTM_ARP_EMPTY_PITCH;
                        for (int c = 0; c < ZTM_ARPEGGIO_NUM_CC; ++c)
                            aw->ccval[c][s] = ZTM_ARP_EMPTY_CCVAL;
                    }
                    file_changed++;
                    sprintf(szStatmsg, "Cleared step data on arpeggio %d", ar_slot);
                    statusmsg = szStatmsg; status_change = 1;
                }
                page_consumed = true;
            }
            else if ((pkey == SDLK_DELETE || pkey == SDLK_BACKSPACE)
                     && (pks & KS_CTRL)) {
                if (song->arpeggios[ar_slot]) {
                    delete song->arpeggios[ar_slot];
                    song->arpeggios[ar_slot] = NULL;
                    ar_name_buf[0] = 0;
                    ((ValueSlider *)UI->get_element(2))->value = 1;
                    ((ValueSlider *)UI->get_element(3))->value = 1;
                    ((ValueSlider *)UI->get_element(4))->value = 0;
                    ((ValueSlider *)UI->get_element(5))->value = 0;
                    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i)
                        ((ValueSlider *)UI->get_element(6 + i))->value = 0;
                    ar_cur_step = ar_cur_col = 0; ar_view_top = 0;
                    file_changed++;
                    sprintf(szStatmsg, "Deleted arpeggio %d", ar_slot);
                    statusmsg = szStatmsg; status_change = 1;
                }
                page_consumed = true;
            }
            if (page_consumed) {
                Keys.getkey();
                need_refresh++;
                doredraw++;
            }
        }
    }

    // Grid input
    if (focused == GRID_ID) {
        KBKey key = Keys.checkkey();
        int kstate = Keys.getstate();
        arpeggio *aa = song->arpeggios[ar_slot];
        int len     = aa ? aa->length : 1;
        int num_cc  = aa ? aa->num_cc : 0;
        int max_col = num_cc;
        bool consumed = false;

        if (!key) { /* idle */ }
        else if (key == SDLK_P && !(kstate & (KS_CTRL|KS_ALT|KS_META|KS_SHIFT))) {
            ar_preset_index = (ar_preset_index + 1) % AR_PRESET_COUNT;
            ar_apply_preset(ar_preset_index);
            arpeggio *na = song->arpeggios[ar_slot];
            ((ValueSlider *)UI->get_element(2))->value = na->length;
            ((ValueSlider *)UI->get_element(3))->value = na->speed;
            ((ValueSlider *)UI->get_element(4))->value = na->repeat_pos;
            ((ValueSlider *)UI->get_element(5))->value = na->num_cc;
            TextInput *tn = (TextInput *)UI->get_element(1);
            if (tn) tn->cursor = 0;   // avoid name-text bleed
            ListBox *psel = dynamic_cast<ListBox *>(UI->get_element(PRESET_LIST_ID));
            if (psel) {
                psel->selectNone();
                psel->setCheck(ar_preset_index, true);
                psel->setCursor(ar_preset_index);
            }
            consumed = true;
        }
        else if ((key == SDLK_DELETE || key == SDLK_BACKSPACE) && (kstate & KS_SHIFT) && !(kstate & KS_CTRL)) {
            // Shift+Del / Shift+BS: clear pitch + cc data on this slot
            // but keep the name and metadata.
            arpeggio *aw = song->arpeggios[ar_slot];
            if (aw) {
                for (int s = 0; s < ZTM_ARPEGGIO_LEN; ++s) {
                    aw->pitch[s] = ZTM_ARP_EMPTY_PITCH;
                    for (int c = 0; c < ZTM_ARPEGGIO_NUM_CC; ++c)
                        aw->ccval[c][s] = ZTM_ARP_EMPTY_CCVAL;
                }
                file_changed++;
                sprintf(szStatmsg, "Cleared step data on arpeggio %d", ar_slot);
                statusmsg = szStatmsg; status_change = 1;
            }
            consumed = true;
        }
        else if ((key == SDLK_DELETE || key == SDLK_BACKSPACE) && (kstate & KS_CTRL)) {
            // Ctrl+Del / Ctrl+BS: wipe the entire slot (free + NULL).
            if (song->arpeggios[ar_slot]) {
                delete song->arpeggios[ar_slot];
                song->arpeggios[ar_slot] = NULL;
                ar_name_buf[0] = 0;
                ((ValueSlider *)UI->get_element(2))->value = 1;
                ((ValueSlider *)UI->get_element(3))->value = 1;
                ((ValueSlider *)UI->get_element(4))->value = 0;
                ((ValueSlider *)UI->get_element(5))->value = 0;
                for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i)
                    ((ValueSlider *)UI->get_element(6 + i))->value = 0;
                ar_cur_step = ar_cur_col = 0; ar_view_top = 0;
                file_changed++;
                sprintf(szStatmsg, "Deleted arpeggio %d", ar_slot);
                statusmsg = szStatmsg; status_change = 1;
            }
            consumed = true;
        }
        else {
            int digit = -1;
            if (key >= SDLK_0 && key <= SDLK_9) digit = key - SDLK_0;

            if (digit >= 0 && ar_cur_step < len) {
                arpeggio *aw = ar_ensure(ar_slot);
                if (aw) {
                    if (ar_cur_col == 0) {
                        int existing = (int16_t)aw->pitch[ar_cur_step];
                        bool was_empty = (aw->pitch[ar_cur_step] == ZTM_ARP_EMPTY_PITCH);
                        int p = was_empty ? 0 : existing;
                        int sign = (p < 0) ? -1 : 1;
                        int abs_p = (p < 0) ? -p : p;
                        if      (ar_cur_digit == 0) abs_p = (abs_p % 100) + digit * 100;
                        else if (ar_cur_digit == 1) abs_p = (abs_p / 100) * 100 + digit * 10 + (abs_p % 10);
                        else                         abs_p = (abs_p / 10) * 10 + digit;
                        if (abs_p > 127) abs_p = 127;
                        aw->pitch[ar_cur_step] = (unsigned short)(int16_t)(sign * abs_p);
                    } else {
                        int c = ar_cur_col - 1;
                        int v = aw->ccval[c][ar_cur_step];
                        if (v == ZTM_ARP_EMPTY_CCVAL) v = 0;
                        if      (ar_cur_digit == 0) v = (v % 100) + digit * 100;
                        else if (ar_cur_digit == 1) v = (v / 100) * 100 + digit * 10 + (v % 10);
                        else                         v = (v / 10) * 10 + digit;
                        if (v > 127) v = 127;
                        aw->ccval[c][ar_cur_step] = (unsigned char)v;
                    }
                    ar_cur_digit = (ar_cur_digit + 1) % 3;
                    if (ar_cur_digit == 0 && ar_cur_step < len - 1) ar_cur_step++;
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_MINUS && ar_cur_col == 0 && ar_cur_step < len) {
                arpeggio *aw = ar_ensure(ar_slot);
                if (aw) {
                    int p = (int16_t)aw->pitch[ar_cur_step];
                    if (aw->pitch[ar_cur_step] == ZTM_ARP_EMPTY_PITCH) p = 0;
                    aw->pitch[ar_cur_step] = (unsigned short)(int16_t)(-p);
                    file_changed++;
                    consumed = true;
                }
            }
            else if ((key == SDLK_PERIOD || key == SDLK_DELETE) && ar_cur_step < len) {
                arpeggio *aw = ar_ensure(ar_slot);
                if (aw) {
                    if (ar_cur_col == 0)
                        aw->pitch[ar_cur_step] = ZTM_ARP_EMPTY_PITCH;
                    else
                        aw->ccval[ar_cur_col - 1][ar_cur_step] = ZTM_ARP_EMPTY_CCVAL;
                    ar_cur_digit = 0;
                    if (ar_cur_step < len - 1) ar_cur_step++;
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_LEFT)  { if (ar_cur_col > 0) ar_cur_col--;        ar_cur_digit = 0; consumed = true; }
            else if (key == SDLK_RIGHT) { if (ar_cur_col < max_col) ar_cur_col++;  ar_cur_digit = 0; consumed = true; }
            else if (key == SDLK_UP)    { if (ar_cur_step > 0) ar_cur_step--;      ar_cur_digit = 0; consumed = true; }
            else if (key == SDLK_DOWN)  { if (ar_cur_step < len - 1) ar_cur_step++;ar_cur_digit = 0; consumed = true; }
            else if (key == SDLK_PAGEUP)   { ar_cur_step -= GRID_VISIBLE; if (ar_cur_step < 0) ar_cur_step = 0; ar_cur_digit = 0; consumed = true; }
            else if (key == SDLK_PAGEDOWN) { ar_cur_step += GRID_VISIBLE; if (ar_cur_step >= len) ar_cur_step = len - 1; ar_cur_digit = 0; consumed = true; }
            else if (key == SDLK_HOME)     { ar_cur_step = 0;        ar_cur_digit = 0; consumed = true; }
            else if (key == SDLK_END)      { ar_cur_step = len - 1;  ar_cur_digit = 0; consumed = true; }
        }

        // Keep the visible window centered on the cursor
        if (ar_cur_step < ar_view_top)                  ar_view_top = ar_cur_step;
        if (ar_cur_step >= ar_view_top + GRID_VISIBLE)  ar_view_top = ar_cur_step - GRID_VISIBLE + 1;
        if (ar_view_top < 0)                            ar_view_top = 0;

        if (consumed) {
            Keys.getkey();
            need_refresh++;
            doredraw++;
        } else if (key) {
            // Eat unhandled keys on the grid so q/w/e/r/t etc. don't
            // leak into global handlers. Tab and ESC are already
            // dispatched by UI::update / global_keys before this runs.
            if (!(kstate & (KS_CTRL|KS_ALT|KS_META))
                && key != SDLK_TAB && key != SDLK_ESCAPE
                && key != SDLK_F1 && key != SDLK_F2 && key != SDLK_F3
                && key != SDLK_F4 && key != SDLK_F5 && key != SDLK_F6
                && key != SDLK_F7 && key != SDLK_F8 && key != SDLK_F9
                && key != SDLK_F10 && key != SDLK_F11 && key != SDLK_F12) {
                Keys.getkey();
            }
        }
    }
}

// ---------------------------------------------------------------------------

void CUI_Arpeggioeditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);
    {
        const char *t = file_changed
            ? "Arpeggio Editor (Shift+F4) [modified - Ctrl+S saves]"
            : "Arpeggio Editor (Shift+F4)";
        printtitle(PAGE_TITLE_ROW_Y, t, COLORS.Text, COLORS.Background, S);
    }

    arpeggio *a = song->arpeggios[ar_slot];
    int  num_cc     = a ? a->num_cc     : 0;
    int  length     = a ? a->length     : 1;
    int  repeat_pos = a ? a->repeat_pos : 0;

    print(row(6), col(BASE_Y),     "Slot",   COLORS.Text, S);
    print(row(6), col(BASE_Y + 2), "Name",   COLORS.Text, S);
    print(row(4), col(BASE_Y + 4), "Length", COLORS.Text, S);
    print(row(5), col(BASE_Y + 6), "Speed",  COLORS.Text, S);
    print(row(4), col(BASE_Y + 8), "Repeat", COLORS.Text, S);
    print(row(5), col(BASE_Y + 10), "NumCC", COLORS.Text, S);
    print(row(7), col(BASE_Y + 12), "CC#",   COLORS.Text, S);

    // Label above the inline Preset listbox (Tab to focus, Enter to apply).
    print(row(47), col(BASE_Y), "Presets (Tab/Arrows/Enter/Space; P=cycle)", COLORS.Text, S);

    // Grid header
    {
        int x = GRID_X;
        print(col(x),    row(GRID_HDR_Y), "STEP", COLORS.Text, S);
        x += 6;
        print(col(x),    row(GRID_HDR_Y), "PIT",  COLORS.Text, S);
        x += 5;
        for (int i = 0; i < num_cc; ++i) {
            char hdr[8];
            snprintf(hdr, sizeof(hdr), "C%d", i);
            print(col(x), row(GRID_HDR_Y), hdr, COLORS.Text, S);
            x += 5;
        }
    }

    // Grid body
    char buf[64];
    for (int row_i = 0; row_i < GRID_VISIBLE; ++row_i) {
        int step = ar_view_top + row_i;
        int py = GRID_Y + row_i;
        if (step >= length) {
            printBG(col(GRID_X), row(py),
                    "                                              ",
                    COLORS.Text, COLORS.Background, S);
            continue;
        }
        snprintf(buf, sizeof(buf), "%03X", step);
        TColor sf = (step == repeat_pos) ? COLORS.Highlight : COLORS.Text;
        printBG(col(GRID_X), row(py), buf, sf, COLORS.Background, S);

        char pbuf[4];
        format_pitch(a ? a->pitch[step] : ZTM_ARP_EMPTY_PITCH, pbuf);
        bool pcell_focus = (UI->cur_element == GRID_ID && step == ar_cur_step && ar_cur_col == 0);
        TColor fg = pcell_focus ? COLORS.EditBG : COLORS.EditText;
        TColor bg = pcell_focus ? COLORS.Highlight : COLORS.EditBG;
        printBG(col(GRID_X + 6), row(py), pbuf, fg, bg, S);

        for (int i = 0; i < num_cc; ++i) {
            char vbuf[4];
            format_ccval(a ? a->ccval[i][step] : ZTM_ARP_EMPTY_CCVAL, vbuf);
            int cell_col = GRID_X + 11 + i * 5;
            bool ccell_focus = (UI->cur_element == GRID_ID && step == ar_cur_step && ar_cur_col == i + 1);
            TColor f = ccell_focus ? COLORS.EditBG : COLORS.EditText;
            TColor b = ccell_focus ? COLORS.Highlight : COLORS.EditBG;
            printBG(col(cell_col), row(py), vbuf, f, b, S);
        }
    }

    // Documentation block anchored just above the toolbar (SPACE_AT_BOTTOM
    // = 8 rows reserved for the toolbar). Pinning to CHARS_Y instead of
    // GRID_Y+GRID_VISIBLE keeps the line vertically stable as the grid
    // changes shape.
    {
        int hint_y = CHARS_Y - SPACE_AT_BOTTOM - 3;
        char hdr[96];
        snprintf(hdr, sizeof(hdr),
                 "Trigger from pattern: R%04X on a row with a note (loops at repeat_pos)",
                 ar_slot);
        print(row(4), col(hint_y),     hdr, COLORS.Highlight, S);
        print(row(4), col(hint_y + 1),
              "Tab into grid; arrows nav cells; digits 0-9 type value; '-' negate pitch",
              COLORS.Text, S);
        print(row(4), col(hint_y + 2),
              "'.' clear; P=preset; Shift+Del=clear data; Ctrl+Del=wipe slot",
              COLORS.Text, S);
    }

    // Status hint when grid has focus -- one row above the doc block.
    if (UI->cur_element == GRID_ID) {
        char hint[80];
        snprintf(hint, sizeof(hint), "GRID @ step %03X col %d | Tab to leave | digits=value | P=preset",
                 ar_cur_step, ar_cur_col);
        printBG(col(2), row(CHARS_Y - SPACE_AT_BOTTOM - 5), hint, COLORS.Highlight, COLORS.Background, S);
    }

    need_refresh = 0; updated = 2;
    ztPlayer->num_orders();
    S->unlock();
}
