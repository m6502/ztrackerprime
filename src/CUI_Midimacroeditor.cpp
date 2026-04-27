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
 *     - ZTM_MIDIMAC_PARAM1 (0x101) — substituted at runtime by the
 *       yy of a Zxxyy pattern effect.
 *
 *   On disk: zt_module::build_MIDI_macro / load_MIDI_macro write/read
 *   the MMAC chunk. So data round-trips through .zt save/load.
 *
 ******/
#include "zt.h"

#define BASE_Y          (TRACKS_ROW_Y + 0)
#define DATA_X          4
#define DATA_Y          (BASE_Y + 8)
#define DATA_COLS       8     // 8 cells per row
#define DATA_ROWS       8     // 64 / 8 = 8 rows
#define CELL_W          4     // "FF " takes 3 chars, +1 gap
#define CELL_H          1

static int      mm_slot       = 0;       // currently selected macro index
static int      mm_cur        = 0;       // cursor position in data[] (0..63)
static int      mm_cur_digit  = 0;       // 0 = high nibble, 1 = low nibble
static char     mm_name_buf[ZTM_MIDIMACRONAME_MAXLEN] = {0};

// Ensure song->midimacros[mm_slot] exists; allocate empty if NULL.
static midimacro *mm_ensure(int slot) {
    if (slot < 0 || slot >= ZTM_MAX_MIDIMACROS) return NULL;
    if (!song->midimacros[slot]) {
        song->midimacros[slot] = new midimacro;
    }
    return song->midimacros[slot];
}

// Compute current length: index of first END marker.
static int mm_length(midimacro *m) {
    if (!m) return 0;
    for (int i = 0; i < ZTM_MIDIMACRO_MAXLEN; ++i) {
        if (m->data[i] == ZTM_MIDIMAC_END) return i;
    }
    return ZTM_MIDIMACRO_MAXLEN;
}

// Set length: pad with 0x00 if growing, set END if shrinking.
static void mm_set_length(midimacro *m, int new_len) {
    if (!m) return;
    if (new_len < 0) new_len = 0;
    if (new_len >= ZTM_MIDIMACRO_MAXLEN) new_len = ZTM_MIDIMACRO_MAXLEN - 1;
    int cur_len = mm_length(m);
    if (new_len > cur_len) {
        for (int i = cur_len; i < new_len; ++i) m->data[i] = 0x00;
    }
    m->data[new_len] = ZTM_MIDIMAC_END;
    file_changed++;
}

// Pull the macro's name into the editing buffer for the TextInput.
static void mm_load_name(int slot) {
    midimacro *m = song->midimacros[slot];
    if (m) {
        memcpy(mm_name_buf, m->name, ZTM_MIDIMACRONAME_MAXLEN);
        mm_name_buf[ZTM_MIDIMACRONAME_MAXLEN - 1] = 0;
    } else {
        mm_name_buf[0] = 0;
    }
}

// Push the editing buffer back into the macro (creating it if needed).
static void mm_store_name(int slot) {
    if (mm_name_buf[0] == 0 && (!song->midimacros[slot] || song->midimacros[slot]->isempty())) {
        // empty name on an empty/missing macro — leave it absent
        return;
    }
    midimacro *m = mm_ensure(slot);
    if (!m) return;
    if (memcmp(m->name, mm_name_buf, ZTM_MIDIMACRONAME_MAXLEN) != 0) {
        memcpy(m->name, mm_name_buf, ZTM_MIDIMACRONAME_MAXLEN);
        m->name[ZTM_MIDIMACRONAME_MAXLEN - 1] = 0;
        file_changed++;
    }
}

CUI_Midimacroeditor::CUI_Midimacroeditor(void) {
    UI = new UserInterface;

    ValueSlider *vs;
    TextInput   *ti;

    // 0 — Slot selector (00..FF)
    vs = new ValueSlider;
    UI->add_element(vs, 0);
    vs->x = 12;  vs->y = BASE_Y;
    vs->xsize = 18; vs->ysize = 1;
    vs->value = 0;
    vs->min = 0; vs->max = ZTM_MAX_MIDIMACROS - 1;

    // 1 — Name TextInput (40 chars max)
    ti = new TextInput;
    UI->add_element(ti, 1);
    ti->frame = 1;
    ti->x = 12; ti->y = BASE_Y + 2;
    ti->xsize = 32; ti->length = ZTM_MIDIMACRONAME_MAXLEN - 1;
    ti->str = (unsigned char*)mm_name_buf;

    // 2 — Length slider
    vs = new ValueSlider;
    UI->add_element(vs, 2);
    vs->x = 12; vs->y = BASE_Y + 4;
    vs->xsize = 18; vs->ysize = 1;
    vs->value = 0;
    vs->min = 0; vs->max = ZTM_MIDIMACRO_MAXLEN - 1;
}

void CUI_Midimacroeditor::enter(void) {
    need_refresh++;
    cur_state = STATE_MIDIMACEDIT;
    mm_load_name(mm_slot);
    midimacro *m = song->midimacros[mm_slot];
    ValueSlider *vs;
    vs = (ValueSlider *)UI->get_element(0);  vs->value = mm_slot;
    vs = (ValueSlider *)UI->get_element(2);  vs->value = mm_length(m);
    UI->set_focus(0);
    Keys.flush();
}

void CUI_Midimacroeditor::leave(void) {
    mm_store_name(mm_slot);
}

void CUI_Midimacroeditor::update() {
    UI->update();

    ValueSlider *vs;
    TextInput   *ti;

    // Slot changed?
    vs = (ValueSlider *)UI->get_element(0);
    if (vs && vs->value != mm_slot) {
        mm_store_name(mm_slot);
        mm_slot = vs->value;
        mm_cur = 0; mm_cur_digit = 0;
        mm_load_name(mm_slot);
        midimacro *m = song->midimacros[mm_slot];
        vs = (ValueSlider *)UI->get_element(2);
        if (vs) vs->value = mm_length(m);
        need_refresh++;
    }

    // Name typed?
    ti = (TextInput *)UI->get_element(1);
    if (ti && ti->changed) {
        mm_store_name(mm_slot);
        ti->changed = 0;
    }

    // Length slider changed?
    vs = (ValueSlider *)UI->get_element(2);
    if (vs) {
        midimacro *m = song->midimacros[mm_slot];
        int cur_len = mm_length(m);
        if (vs->value != cur_len) {
            midimacro *mm = mm_ensure(mm_slot);
            mm_set_length(mm, vs->value);
            if (mm_cur >= vs->value) mm_cur = vs->value > 0 ? vs->value - 1 : 0;
            need_refresh++;
        }
    }

    // Data-grid keys: only handled when focus is on the grid (we use
    // a "grid mode" entered by pressing Tab from the length slider —
    // simpler than a custom UIE).
    KBKey key = Keys.checkkey();
    int kstate = Keys.getstate();
    int focused_id = UI->cur_element;

    // Grid keybindings are active when no UI element is currently
    // claiming the key — we check focused_id and let the slider/textinput
    // consume keys first via UI->update().
    if (key && focused_id != 1 /* not in name field */) {
        midimacro *m = song->midimacros[mm_slot];
        int cur_len = m ? mm_length(m) : 0;
        bool consumed = false;

        // Hex digit input → write into current slot, advance digit.
        // Letters E (END) and P (PARAM1) place sentinels.
        // '.' / Delete clears slot (sets 0x00).
        int hex = -1;
        if      (key >= SDLK_0 && key <= SDLK_9) hex = key - SDLK_0;
        else if (key >= SDLK_A && key <= SDLK_F) hex = (key - SDLK_A) + 10;

        if (focused_id != 0 && focused_id != 2 && cur_len > 0 && mm_cur < cur_len) {
            if (hex >= 0 && !(kstate & (KS_CTRL|KS_ALT|KS_META))) {
                midimacro *mm = mm_ensure(mm_slot);
                if (mm) {
                    unsigned short cur = mm->data[mm_cur];
                    if (cur >= ZTM_MIDIMAC_END) cur = 0;   // overwrite sentinel
                    if (mm_cur_digit == 0) {
                        cur = (cur & 0x0F) | ((hex & 0xF) << 4);
                        mm->data[mm_cur] = cur;
                        mm_cur_digit = 1;
                    } else {
                        cur = (cur & 0xF0) | (hex & 0xF);
                        mm->data[mm_cur] = cur;
                        mm_cur_digit = 0;
                        if (mm_cur < cur_len - 1) mm_cur++;
                    }
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_E) {
                midimacro *mm = mm_ensure(mm_slot);
                if (mm) {
                    mm->data[mm_cur] = ZTM_MIDIMAC_END;
                    // truncate length up to here
                    file_changed++;
                    vs = (ValueSlider *)UI->get_element(2);
                    if (vs) vs->value = mm_cur;
                    consumed = true;
                }
            }
            else if (key == SDLK_P) {
                midimacro *mm = mm_ensure(mm_slot);
                if (mm) {
                    mm->data[mm_cur] = ZTM_MIDIMAC_PARAM1;
                    if (mm_cur < cur_len - 1) mm_cur++;
                    file_changed++;
                    consumed = true;
                }
            }
            else if (key == SDLK_PERIOD || key == SDLK_DELETE) {
                midimacro *mm = mm_ensure(mm_slot);
                if (mm) {
                    mm->data[mm_cur] = 0x00;
                    if (mm_cur < cur_len - 1) mm_cur++;
                    file_changed++;
                    consumed = true;
                }
            }
            else {
                switch (key) {
                    case SDLK_LEFT:  if (mm_cur > 0) mm_cur--; mm_cur_digit = 0; consumed = true; break;
                    case SDLK_RIGHT: if (mm_cur < cur_len - 1) mm_cur++; mm_cur_digit = 0; consumed = true; break;
                    case SDLK_UP:    if (mm_cur >= DATA_COLS) mm_cur -= DATA_COLS; mm_cur_digit = 0; consumed = true; break;
                    case SDLK_DOWN:  if (mm_cur + DATA_COLS < cur_len) mm_cur += DATA_COLS; mm_cur_digit = 0; consumed = true; break;
                    case SDLK_HOME:  mm_cur = 0; mm_cur_digit = 0; consumed = true; break;
                    case SDLK_END:   mm_cur = cur_len > 0 ? cur_len - 1 : 0; mm_cur_digit = 0; consumed = true; break;
                }
            }
        }

        if (consumed) {
            Keys.getkey();
            need_refresh++;
            doredraw++;
        }
    }
}

void CUI_Midimacroeditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);
    printtitle(PAGE_TITLE_ROW_Y, "MIDI Macro Editor (F4)", COLORS.Text, COLORS.Background, S);

    midimacro *m = song->midimacros[mm_slot];
    int cur_len = m ? mm_length(m) : 0;

    char buf[64];

    // Slot label + numeric readout
    print(row(4), col(BASE_Y), "Slot", COLORS.Text, S);
    snprintf(buf, sizeof(buf), "%03d / %03d", mm_slot, ZTM_MAX_MIDIMACROS - 1);
    printBG(col(31), row(BASE_Y), buf, COLORS.Text, COLORS.Background, S);

    // Name label
    print(row(4), col(BASE_Y + 2), "Name", COLORS.Text, S);

    // Length
    print(row(2), col(BASE_Y + 4), "Length", COLORS.Text, S);
    snprintf(buf, sizeof(buf), "%03d / %03d", cur_len, ZTM_MIDIMACRO_MAXLEN - 1);
    printBG(col(31), row(BASE_Y + 4), buf, COLORS.Text, COLORS.Background, S);

    // Status / hint
    print(row(4), col(BASE_Y + 6), "Data (hex bytes; E=END, P=PARAM1, .=clear)",
          COLORS.Text, S);

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
            if (v == ZTM_MIDIMAC_PARAM1) {
                cell = "P1";
                fg = COLORS.Highlight;
            } else if (v >= ZTM_MIDIMAC_END) {
                cell = "..";
            } else {
                snprintf(buf, sizeof(buf), "%02X", v & 0xFF);
                cell = buf;
            }
        } else {
            fg = COLORS.Background;   // dim: outside length
            cell = "..";
        }
        printBG(col(px), row(py), cell, fg, bg, S);
        // Step number every 8 cells: col on the left margin
        if (gx == 0) {
            char snum[6];
            snprintf(snum, sizeof(snum), "%02X", i);
            print(col(DATA_X - 3), row(py), snum, COLORS.Text, S);
        }
    }

    // Cursor highlight on current cell
    if (cur_len > 0 && mm_cur < cur_len) {
        int gy = mm_cur / DATA_COLS;
        int gx = mm_cur %  DATA_COLS;
        int px = DATA_X + gx * CELL_W;
        int py = DATA_Y + gy;
        unsigned short v = m ? m->data[mm_cur] : 0;
        const char *cell = "..";
        char cbuf[4];
        if (v == ZTM_MIDIMAC_PARAM1)       cell = "P1";
        else if (v >= ZTM_MIDIMAC_END)     cell = "..";
        else { snprintf(cbuf, sizeof(cbuf), "%02X", v & 0xFF); cell = cbuf; }
        printBG(col(px), row(py), cell, COLORS.EditBG, COLORS.Highlight, S);
        // Underline the active hex digit
        if (v < ZTM_MIDIMAC_END) {
            char d[2] = {cell[mm_cur_digit], 0};
            printBG(col(px + mm_cur_digit), row(py), d,
                    COLORS.Background, COLORS.Highlight, S);
        }
    }

    need_refresh = 0; updated = 2;
    ztPlayer->num_orders();
    S->unlock();
}
