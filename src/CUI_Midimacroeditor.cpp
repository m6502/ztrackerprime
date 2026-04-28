/*************************************************************************
 *
 * FILE  CUI_Midimacroeditor.cpp
 *
 * DESCRIPTION
 *   The MIDI macro editor (F4).
 *
 *   Each song carries up to ZTM_MAX_MIDIMACROS (256) macros. A macro is
 *   a name + a sequence of unsigned-short slots terminated by
 *   ZTM_MIDIMAC_END (0x100). Each slot is either:
 *     - a raw byte 0x00..0xFF (sent as part of a packed short MIDI msg)
 *     - ZTM_MIDIMAC_PARAM1 (0x101) -- substituted at runtime by the
 *       yy of a Zxxyy pattern effect.
 *
 *   On disk: zt_module::build_MIDI_macro / load_MIDI_macro round-trip
 *   the MMAC chunk in the .zt file. So data persists across save/load.
 *
 *   UI layout:
 *     Slot   [slider]
 *     Name   [textinput]
 *     Length [slider]
 *     [Apply preset]    <- press P or click for preset menu
 *
 *     Data (hex bytes; status byte 0x80+ starts a new MIDI msg):
 *       <step> <hex byte | "P1" | "..">  ... 8 cells per row
 *
 *   The data grid is a ListBox-like sub-widget kept in tab order so
 *   Tab walks the user from Length into the grid; arrow keys then
 *   navigate cells. Tab again leaves the grid back to the slot slider.
 *
 ******/
#include "zt.h"

#define BASE_Y          (TRACKS_ROW_Y + 0)
// SPACE_AT_BOTTOM mirrors the CUI_Patterneditor constant: 8 rows are
// reserved at the bottom of the screen for the toolbar.
#define SPACE_AT_BOTTOM 8
#define DATA_X          12
#define DATA_HDR_Y      (BASE_Y + 6)
#define DATA_Y          (BASE_Y + 7)
#define DATA_COLS       8
#define DATA_ROWS       8
#define CELL_W          4
#define GRID_ID         3
#define PRESET_LIST_ID  4
#define HINT_ID         99   // not added; just an unused sentinel

static int      mm_slot      = 0;
static int      mm_cur       = 0;       // 0..ZTM_MIDIMACRO_MAXLEN-1
static int      mm_cur_digit = 0;       // 0 = high nibble, 1 = low

static char     mm_name_buf[ZTM_MIDIMACRONAME_MAXLEN] = {0};

// ---------------------------------------------------------------------------
// Helpers

static midimacro *mm_ensure(int slot) {
    if (slot < 0 || slot >= ZTM_MAX_MIDIMACROS) return NULL;
    if (!song->midimacros[slot]) song->midimacros[slot] = new midimacro;
    return song->midimacros[slot];
}

static int mm_length(midimacro *m) {
    if (!m) return 0;
    for (int i = 0; i < ZTM_MIDIMACRO_MAXLEN; ++i)
        if (m->data[i] == ZTM_MIDIMAC_END) return i;
    return ZTM_MIDIMACRO_MAXLEN;
}

static void mm_set_length(midimacro *m, int new_len) {
    if (!m) return;
    if (new_len < 0) new_len = 0;
    if (new_len >= ZTM_MIDIMACRO_MAXLEN) new_len = ZTM_MIDIMACRO_MAXLEN - 1;
    int cur = mm_length(m);
    if (new_len > cur) for (int i = cur; i < new_len; ++i) m->data[i] = 0x00;
    m->data[new_len] = ZTM_MIDIMAC_END;
    file_changed++;
}

static void mm_load_name(int slot) {
    midimacro *m = song->midimacros[slot];
    if (m) {
        memcpy(mm_name_buf, m->name, ZTM_MIDIMACRONAME_MAXLEN);
        mm_name_buf[ZTM_MIDIMACRONAME_MAXLEN - 1] = 0;
    } else {
        mm_name_buf[0] = 0;
    }
}

static void mm_store_name(int slot) {
    bool any_text = (mm_name_buf[0] != 0);
    midimacro *cur = song->midimacros[slot];
    if (!any_text && (!cur || cur->isempty())) return;
    midimacro *m = mm_ensure(slot);
    if (!m) return;
    if (memcmp(m->name, mm_name_buf, ZTM_MIDIMACRONAME_MAXLEN) != 0) {
        memcpy(m->name, mm_name_buf, ZTM_MIDIMACRONAME_MAXLEN);
        m->name[ZTM_MIDIMACRONAME_MAXLEN - 1] = 0;
        file_changed++;
    }
}

// ---------------------------------------------------------------------------
// Presets (data + pure apply function live in preset_data.h so a unit
// test can verify them without dragging in SDL/UI globals).

#include "preset_data.h"
#include "preset_selector.h"

static int  mm_preset_index = 0;
// See ar_preset_just_applied in CUI_Arpeggioeditor.cpp -- same role here.
static bool mm_preset_just_applied = false;

static void mm_apply_preset(int idx) {
    if (idx < 0 || idx >= MM_PRESET_COUNT) return;
    midimacro *m = mm_ensure(mm_slot);
    if (!m) return;
    mm_apply_preset_to(m, idx);
    memset(mm_name_buf, 0, sizeof(mm_name_buf));
    memcpy(mm_name_buf, m->name, ZTM_MIDIMACRONAME_MAXLEN);
    file_changed++;
    sprintf(szStatmsg, "Applied preset: %s", MM_PRESETS[idx].name);
    statusmsg = szStatmsg;
    status_change = 1;
}

// ---------------------------------------------------------------------------
// Grid pseudo-element: a tab-stop that does nothing in update() but
// claims focus so the page-level update() can identify "we are on the
// grid" reliably.

namespace {
class MmGridFocus : public UserInterfaceElement {
public:
    MmGridFocus() { xsize = 1; ysize = 1; }
    int update() override {
        // Return 0 here; the page's update() handles arrow keys + digit
        // input directly when this element is focused. Tab is consumed
        // via the SDLK_TAB handling in the page (we set ret=1 there).
        return 0;
    }
    void draw(Drawable *S, int active) override { (void)S; (void)active; }
};

// Inline preset selector -- modeled after SkinSelector on F11.
// Always visible to the right of the sliders so the full preset
// catalogue is in view (replaces the invisible P-cycle).
class MmPresetSelector : public ListBox {
public:
    MmPresetSelector() {
        empty_message = "No presets";
        is_sorted = false;   // preserve hand-curated order
        use_checks = true;
        use_key_select = true;
        OnChange();
    }
    void OnChange() override {
        clear();
        // insertItem with is_sorted=false prepends to the head, so walk
        // backwards to end up with the array's natural order on screen.
        for (int i = MM_PRESET_COUNT - 1; i >= 0; --i) {
            LBNode *p = insertItem((char *)MM_PRESETS[i].name);
            if (p) {
                p->int_data = i;
                if (i == mm_preset_index) p->checked = true;
            }
        }
    }
    PresetCursor snapshot() const {
        PresetCursor s;
        s.num_items    = MM_PRESET_COUNT;
        s.active_index = mm_preset_index;
        s.cursor_row   = cur_sel + y_start;
        return s;
    }
    void apply_event(const PresetEvent &e) {
        if (e.new_cursor_row >= 0) setCursor(e.new_cursor_row);
        if (e.fire_apply) {
            int row = (e.new_cursor_row >= 0) ? e.new_cursor_row : (cur_sel + y_start);
            LBNode *p = getNode(row);
            if (p) OnSelect(p);
        }
    }
    int update() override {
        KBKey k = Keys.checkkey();
        int   ks = Keys.getstate();
        if (k == SDLK_P && !(ks & (KS_CTRL|KS_ALT|KS_META|KS_SHIFT))) {
            Keys.getkey();
            apply_event(preset_on_cycle(snapshot()));
            need_refresh++; need_redraw++;
            return 0;
        }
        if (k == SDLK_SPACE || k == SDLK_RETURN) {
            Keys.getkey();
            apply_event(preset_on_apply(snapshot()));
            need_refresh++; need_redraw++;
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
    int mouseupdate(int parent_cur) override {
        int prev_mousestate = mousestate;
        int new_cur = ListBox::mouseupdate(parent_cur);
        if (mousestate && !prev_mousestate && new_cur == this->ID) {
            apply_event(preset_on_click(snapshot(), cur_sel + y_start));
            sprintf(szStatmsg, "Macro preset click row=%d idx=%d",
                    cur_sel + y_start, mm_preset_index);
            statusmsg = szStatmsg; status_change = 1;
        }
        return new_cur;
    }
    void OnSelect(LBNode *selected) override {
        if (!selected) return;
        mm_preset_index = selected->int_data;
        mm_apply_preset(mm_preset_index);
        // Refresh checkmarks to follow the new selection.
        selectNone();
        selected->checked = true;
        mm_preset_just_applied = true;
        ListBox::OnSelect(selected);
    }
    void OnSelectChange() override {}
};
} // namespace

// ---------------------------------------------------------------------------
// Constructor / lifecycle

CUI_Midimacroeditor::CUI_Midimacroeditor(void) {
    UI = new UserInterface;

    ValueSlider *vs;
    TextInput   *ti;

    // 0 -- Slot
    vs = new ValueSlider;
    UI->add_element(vs, 0);
    vs->x = 12; vs->y = BASE_Y;     vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_MAX_MIDIMACROS - 1; vs->value = 0;

    // 1 -- Name
    ti = new TextInput;
    UI->add_element(ti, 1);
    ti->frame = 1;
    ti->x = 12; ti->y = BASE_Y + 2;
    ti->xsize = 32; ti->length = ZTM_MIDIMACRONAME_MAXLEN - 1;
    ti->str = (unsigned char*)mm_name_buf;

    // 2 -- Length
    vs = new ValueSlider;
    UI->add_element(vs, 2);
    vs->x = 12; vs->y = BASE_Y + 4; vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_MIDIMACRO_MAXLEN - 1; vs->value = 0;

    // 3 -- Grid focus stub (real UIE so Tab can land here)
    MmGridFocus *gf = new MmGridFocus;
    UI->add_element(gf, GRID_ID);
    gf->x = DATA_X; gf->y = DATA_Y;

    // 4 -- Preset list. Mirrors the F11 SkinSelector ergonomics so
    // every preset is visible at once (Tab to focus, arrows to move,
    // Enter to apply). Replaces the invisible P-cycle.
    MmPresetSelector *ps = new MmPresetSelector;
    UI->add_element(ps, PRESET_LIST_ID);
    ps->x = 47; ps->y = BASE_Y + 2;
    ps->xsize = 38;
    ps->ysize = MM_PRESET_COUNT - 1;   // ListBox interprets ysize as last visible row
}

void CUI_Midimacroeditor::enter(void) {
    need_refresh++;
    cur_state = STATE_MIDIMACEDIT;
    mm_load_name(mm_slot);
    midimacro *m = song->midimacros[mm_slot];
    int len = mm_length(m);
    if (mm_cur >= ZTM_MIDIMACRO_MAXLEN) mm_cur = ZTM_MIDIMACRO_MAXLEN - 1;
    if (mm_cur < 0) mm_cur = 0;
    if (len > 0 && mm_cur >= len) mm_cur = len - 1;
    mm_cur_digit = 0;
    ((ValueSlider *)UI->get_element(0))->value = mm_slot;
    ((ValueSlider *)UI->get_element(2))->value = len;
    UI->set_focus(0);
    Keys.flush();
}

void CUI_Midimacroeditor::leave(void) {
    mm_store_name(mm_slot);
}

// ---------------------------------------------------------------------------

void CUI_Midimacroeditor::update() {
    UI->update();

    ValueSlider *vs;
    TextInput   *ti;

    // Preset just applied via the listbox -- pull macro length into
    // the slider widget BEFORE the slider->macro sync block, or stale
    // widget values would overwrite the preset on the next frame.
    if (mm_preset_just_applied) {
        mm_preset_just_applied = false;
        midimacro *m = song->midimacros[mm_slot];
        if (m) {
            ValueSlider *vlen = (ValueSlider *)UI->get_element(2);
            if (vlen) vlen->value = mm_length(m);
        }
        TextInput *t = (TextInput *)UI->get_element(1);
        if (t) t->cursor = 0;
        need_refresh++;
        doredraw++;
    }

    // Slot change
    vs = (ValueSlider *)UI->get_element(0);
    if (vs && vs->value != mm_slot) {
        mm_store_name(mm_slot);
        mm_slot = vs->value;
        mm_cur = 0; mm_cur_digit = 0;
        mm_load_name(mm_slot);
        midimacro *m = song->midimacros[mm_slot];
        ValueSlider *vlen = (ValueSlider *)UI->get_element(2);
        if (vlen) vlen->value = mm_length(m);
        // Reset TextInput cursor so we don't render stale cursor past
        // the new name's end.
        ti = (TextInput *)UI->get_element(1);
        if (ti) ti->cursor = 0;
        need_refresh++;
        doredraw++;
    }

    // Name typed
    ti = (TextInput *)UI->get_element(1);
    if (ti && ti->changed) { mm_store_name(mm_slot); ti->changed = 0; }

    // Length slider
    vs = (ValueSlider *)UI->get_element(2);
    if (vs) {
        midimacro *m = song->midimacros[mm_slot];
        int cur = mm_length(m);
        if (vs->value != cur) {
            midimacro *mm = (vs->value > 0) ? mm_ensure(mm_slot) : song->midimacros[mm_slot];
            if (mm) {
                mm_set_length(mm, vs->value);
                if (mm_cur >= vs->value) mm_cur = vs->value > 0 ? vs->value - 1 : 0;
                need_refresh++;
            }
        }
    }

    // Page-level shortcuts: work from any focus that isn't the Name
    // TextInput (where letter keys should type into the name). This
    // also guarantees the keys are consumed, so they can't accumulate
    // in the Keys queue and stall the app.
    int focused = UI->cur_element;
    {
        KBKey pkey = Keys.checkkey();
        int   pks  = Keys.getstate();
        if (pkey && focused != 1) {
            bool page_consumed = false;
            if (pkey == SDLK_P && !(pks & (KS_CTRL|KS_ALT|KS_META|KS_SHIFT))
                && focused != PRESET_LIST_ID) {
                mm_preset_index = (mm_preset_index + 1) % MM_PRESET_COUNT;
                mm_apply_preset(mm_preset_index);
                ((ValueSlider *)UI->get_element(2))->value = mm_length(song->midimacros[mm_slot]);
                // Reset name TextInput cursor so trailing chars from a
                // longer prior name don't paint over the new shorter
                // name on screen.
                TextInput *t = (TextInput *)UI->get_element(1);
                if (t) t->cursor = 0;
                // Mirror the new selection in the visible listbox.
                ListBox *psel = dynamic_cast<ListBox *>(UI->get_element(PRESET_LIST_ID));
                if (psel) {
                    psel->selectNone();
                    psel->setCheck(mm_preset_index, true);
                    psel->setCursor(mm_preset_index);
                }
                page_consumed = true;
            }
            else if ((pkey == SDLK_DELETE || pkey == SDLK_BACKSPACE)
                     && (pks & KS_SHIFT) && !(pks & KS_CTRL)) {
                midimacro *mw = song->midimacros[mm_slot];
                if (mw) {
                    mw->data[0] = ZTM_MIDIMAC_END;
                    mm_cur = 0; mm_cur_digit = 0;
                    ((ValueSlider *)UI->get_element(2))->value = 0;
                    file_changed++;
                    sprintf(szStatmsg, "Cleared bytes on macro %d", mm_slot);
                    statusmsg = szStatmsg; status_change = 1;
                }
                page_consumed = true;
            }
            else if ((pkey == SDLK_DELETE || pkey == SDLK_BACKSPACE)
                     && (pks & KS_CTRL)) {
                if (song->midimacros[mm_slot]) {
                    delete song->midimacros[mm_slot];
                    song->midimacros[mm_slot] = NULL;
                    mm_name_buf[0] = 0;
                    ((ValueSlider *)UI->get_element(2))->value = 0;
                    mm_cur = 0; mm_cur_digit = 0;
                    file_changed++;
                    sprintf(szStatmsg, "Deleted macro %d", mm_slot);
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

    // Grid input -- only when focus is parked on the grid stub.
    if (focused == GRID_ID) {
        KBKey key = Keys.checkkey();
        int kstate = Keys.getstate();
        bool consumed = false;

        if (!key) { /* no key */ }
        else if (key == SDLK_P && !(kstate & (KS_CTRL|KS_ALT|KS_META|KS_SHIFT))) {
            // P (no modifier) cycles through presets and applies the
            // next one. Status line shows which preset just landed.
            mm_preset_index = (mm_preset_index + 1) % MM_PRESET_COUNT;
            mm_apply_preset(mm_preset_index);
            ValueSlider *vlen = (ValueSlider *)UI->get_element(2);
            if (vlen) vlen->value = mm_length(song->midimacros[mm_slot]);
            ListBox *psel = dynamic_cast<ListBox *>(UI->get_element(PRESET_LIST_ID));
            if (psel) {
                psel->selectNone();
                psel->setCheck(mm_preset_index, true);
                psel->setCursor(mm_preset_index);
            }
            consumed = true;
        }
        else if ((key == SDLK_DELETE || key == SDLK_BACKSPACE) && (kstate & KS_SHIFT) && !(kstate & KS_CTRL)) {
            // Shift+Del / Shift+BS: clear all bytes on this slot but
            // keep the name. Length goes to 0 (END at index 0).
            midimacro *mw = song->midimacros[mm_slot];
            if (mw) {
                mw->data[0] = ZTM_MIDIMAC_END;
                mm_cur = 0; mm_cur_digit = 0;
                ((ValueSlider *)UI->get_element(2))->value = 0;
                file_changed++;
                sprintf(szStatmsg, "Cleared bytes on macro %d", mm_slot);
                statusmsg = szStatmsg; status_change = 1;
            }
            consumed = true;
        }
        else if ((key == SDLK_DELETE || key == SDLK_BACKSPACE) && (kstate & KS_CTRL)) {
            // Ctrl+Del / Ctrl+BS: wipe the entire slot.
            if (song->midimacros[mm_slot]) {
                delete song->midimacros[mm_slot];
                song->midimacros[mm_slot] = NULL;
                mm_name_buf[0] = 0;
                ((ValueSlider *)UI->get_element(2))->value = 0;
                mm_cur = 0; mm_cur_digit = 0;
                file_changed++;
                sprintf(szStatmsg, "Deleted macro %d", mm_slot);
                statusmsg = szStatmsg; status_change = 1;
            }
            consumed = true;
        }
        else {
            // Helper: ensure macro exists and has at least mm_cur+1 bytes
            // so the user can type into a fresh slot without first
            // dragging the Length slider. The macro auto-grows as the
            // cursor moves past the current END.
            auto grow_to = [&](int idx) -> midimacro* {
                midimacro *mm = mm_ensure(mm_slot);
                if (!mm) return NULL;
                int len = mm_length(mm);
                if (idx >= len) {
                    for (int i = len; i <= idx; ++i) mm->data[i] = 0x00;
                    if (idx + 1 < ZTM_MIDIMACRO_MAXLEN) mm->data[idx + 1] = ZTM_MIDIMAC_END;
                    ValueSlider *vlen = (ValueSlider *)UI->get_element(2);
                    if (vlen) vlen->value = idx + 1;
                    file_changed++;
                }
                return mm;
            };

            int hex = -1;
            if      (key >= SDLK_0 && key <= SDLK_9) hex = key - SDLK_0;
            else if (key >= SDLK_A && key <= SDLK_F) hex = (key - SDLK_A) + 10;

            if (hex >= 0 && !(kstate & (KS_CTRL|KS_ALT|KS_META))) {
                midimacro *mm = grow_to(mm_cur);
                if (mm) {
                    unsigned short v = mm->data[mm_cur];
                    if (v >= ZTM_MIDIMAC_END) v = 0;
                    if (mm_cur_digit == 0) {
                        v = (v & 0x0F) | ((hex & 0xF) << 4);
                        mm->data[mm_cur] = v;
                        mm_cur_digit = 1;
                    } else {
                        v = (v & 0xF0) | (hex & 0xF);
                        mm->data[mm_cur] = v;
                        mm_cur_digit = 0;
                        if (mm_cur + 1 < ZTM_MIDIMACRO_MAXLEN - 1) mm_cur++;
                    }
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_E && (kstate & KS_SHIFT)) {
                midimacro *mm = mm_ensure(mm_slot);
                if (mm) {
                    mm->data[mm_cur] = ZTM_MIDIMAC_END;
                    ValueSlider *vlen = (ValueSlider *)UI->get_element(2);
                    if (vlen) vlen->value = mm_cur;
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_X && (kstate & KS_SHIFT)) {
                midimacro *mm = grow_to(mm_cur);
                if (mm) {
                    mm->data[mm_cur] = ZTM_MIDIMAC_PARAM1;
                    if (mm_cur + 1 < ZTM_MIDIMACRO_MAXLEN - 1) mm_cur++;
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_PERIOD || key == SDLK_DELETE) {
                midimacro *mm = mm_ensure(mm_slot);
                if (mm && mm_cur < mm_length(mm)) {
                    mm->data[mm_cur] = 0x00;
                    if (mm_cur + 1 < mm_length(mm)) mm_cur++;
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_LEFT)  { if (mm_cur > 0) mm_cur--;                                   mm_cur_digit = 0; consumed = true; }
            else if (key == SDLK_RIGHT) { if (mm_cur + 1 < ZTM_MIDIMACRO_MAXLEN - 1) mm_cur++;        mm_cur_digit = 0; consumed = true; }
            else if (key == SDLK_UP)    { if (mm_cur >= DATA_COLS) mm_cur -= DATA_COLS;               mm_cur_digit = 0; consumed = true; }
            else if (key == SDLK_DOWN)  { if (mm_cur + DATA_COLS < ZTM_MIDIMACRO_MAXLEN - 1) mm_cur += DATA_COLS; mm_cur_digit = 0; consumed = true; }
            else if (key == SDLK_HOME)  { mm_cur = 0;                                                 mm_cur_digit = 0; consumed = true; }
            else if (key == SDLK_END)   { int len = mm_length(song->midimacros[mm_slot]); mm_cur = len > 0 ? len - 1 : 0; mm_cur_digit = 0; consumed = true; }
        }

        if (consumed) {
            Keys.getkey();
            need_refresh++;
            doredraw++;
        } else if (key) {
            // Silently swallow unhandled keys while on the grid so they
            // don't leak into global_keys / keyhandler (otherwise typing
            // qwert in a cell would do mysterious things). Tab and ESC
            // are pass-through: they're handled by UI::update / global
            // _keys before this code runs, so they never reach here.
            // Anything else with no modifiers gets eaten.
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

void CUI_Midimacroeditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);
    {
        const char *t = file_changed
            ? "MIDI Macro Editor (F4) [modified - Ctrl+S saves]"
            : "MIDI Macro Editor (F4)";
        printtitle(PAGE_TITLE_ROW_Y, t, COLORS.Text, COLORS.Background, S);
    }

    midimacro *m = song->midimacros[mm_slot];
    int cur_len = m ? mm_length(m) : 0;
    char buf[64];

    print(row(6),  col(BASE_Y),     "Slot",   COLORS.Text, S);
    print(row(6),  col(BASE_Y + 2), "Name",   COLORS.Text, S);
    print(row(4),  col(BASE_Y + 4), "Length", COLORS.Text, S);

    // Label above the inline Preset listbox (Tab to focus, Enter to apply).
    print(row(47), col(BASE_Y), "Presets (Tab/Arrows/Enter/Space; P=cycle)", COLORS.Text, S);

    // Data grid header
    print(row(DATA_X - 3), row(DATA_HDR_Y), "##", COLORS.Text, S);
    for (int gx = 0; gx < DATA_COLS; ++gx) {
        char h[4]; snprintf(h, sizeof(h), "%02X", gx);
        print(col(DATA_X + gx * CELL_W), row(DATA_HDR_Y), h, COLORS.Text, S);
    }

    // Data grid
    for (int i = 0; i < ZTM_MIDIMACRO_MAXLEN; ++i) {
        int gy = i / DATA_COLS;
        int gx = i %  DATA_COLS;
        int px = DATA_X + gx * CELL_W;
        int py = DATA_Y + gy;

        TColor fg = COLORS.EditText;
        TColor bg = COLORS.EditBG;
        const char *cell = "..";

        if (i < cur_len) {
            unsigned short v = m ? m->data[i] : 0;
            if (v == ZTM_MIDIMAC_PARAM1)        { cell = "P1"; fg = COLORS.Highlight; }
            else if (v >= ZTM_MIDIMAC_END)      { cell = ".."; }
            else { snprintf(buf, sizeof(buf), "%02X", v & 0xFF); cell = buf; }
        } else {
            fg = COLORS.Background; cell = "..";
        }
        printBG(col(px), row(py), cell, fg, bg, S);

        if (gx == 0) {
            char snum[6]; snprintf(snum, sizeof(snum), "%02X", i);
            print(col(DATA_X - 3), row(py), snum, COLORS.Text, S);
        }
    }

    // Documentation block pinned to the same x/y anchor that the
    // CUI_Arpeggioeditor uses (column 4, three rows above the
    // SPACE_AT_BOTTOM toolbar margin). Hardcoding the column here -- not
    // DATA_X -- keeps F4 / Shift+F4 visually aligned even if the data
    // grid's x moves.
    {
        int hint_y = CHARS_Y - SPACE_AT_BOTTOM - 3;
        char invoke[96];
        snprintf(invoke, sizeof(invoke),
                 "Trigger from pattern: Z%02Xyy  (yy substitutes for any P1 byte; range 00-7F)",
                 mm_slot);
        print(row(4), col(hint_y),     invoke, COLORS.Highlight, S);
        print(row(4), col(hint_y + 1),
              "Status bytes (0x80+) start a new MIDI msg: B0=CC, 90=NoteOn, C0=ProgChg, E0=PitchBend",
              COLORS.Text, S);
        print(row(4), col(hint_y + 2),
              "Shift+E=END, Shift+X=PARAM1, .=clear, Shift+Del=clear data, Ctrl+Del=wipe slot",
              COLORS.Text, S);
    }

    // Cursor highlight -- drawn whenever the grid stub holds focus.
    // The cell value is preserved (dark text on yellow background)
    // and the active hex digit gets an extra inverse highlight
    // (yellow text on dark background) so the user can see which
    // nibble the next keypress will affect.
    if (UI->cur_element == GRID_ID) {
        int gy = mm_cur / DATA_COLS;
        int gx = mm_cur %  DATA_COLS;
        int px = DATA_X + gx * CELL_W;
        int py = DATA_Y + gy;
        unsigned short v = (m && mm_cur < ZTM_MIDIMACRO_MAXLEN) ? m->data[mm_cur] : ZTM_MIDIMAC_END;
        const char *cell = "..";
        char cbuf[4];
        if      (v == ZTM_MIDIMAC_PARAM1) cell = "P1";
        else if (v >= ZTM_MIDIMAC_END)    cell = "..";
        else { snprintf(cbuf, sizeof(cbuf), "%02X", v & 0xFF); cell = cbuf; }
        // Whole cell inverted: dark text on yellow background.
        printBG(col(px), row(py), cell, COLORS.EditBG, COLORS.Highlight, S);
        if (v < ZTM_MIDIMAC_END) {
            char d[2] = { cell[mm_cur_digit], 0 };
            // Active nibble: yellow text on dark background. Stays
            // legible against either Highlight or Background page
            // colors and doesn't overwrite the cell value.
            printBG(col(px + mm_cur_digit), row(py), d,
                    COLORS.Highlight, COLORS.EditBG, S);
        }
    }
    (void)buf;   // (used elsewhere above)

    need_refresh = 0; updated = 2;
    ztPlayer->num_orders();
    S->unlock();
}
