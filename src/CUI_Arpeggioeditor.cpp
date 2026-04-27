/*************************************************************************
 *
 * FILE  CUI_Arpeggioeditor.cpp
 *
 * DESCRIPTION
 *   The arpeggio editor (Shift+F4).
 *
 *   Each song carries up to ZTM_MAX_ARPEGGIOS (256) arpeggios. Each has
 *   a name, length (number of steps), speed (ticks per step), repeat
 *   position (0..length-1), num_cc (0..ZTM_ARPEGGIO_NUM_CC), an array
 *   of cc[] controller numbers, and per-step pitch[] + ccval[][] tables.
 *
 *   pitch[i] is an unsigned short; 0x8000 = ZTM_ARP_EMPTY_PITCH means
 *   "no pitch event at this step". Other values are signed semitone
 *   offsets reinterpreted as int16: bit 15 + sign-extension.
 *
 *   ccval[c][i] is an unsigned char; 0x80 = ZTM_ARP_EMPTY_CCVAL means
 *   "no CC event at this step". Other values are 0..127.
 *
 *   On disk: zt_module::build_arpeggio / load_arpeggio (ARPG chunk)
 *   round-trip these structures through .zt save/load.
 *
 ******/
#include "zt.h"

#define BASE_Y          (TRACKS_ROW_Y + 0)
#define GRID_X          4
#define GRID_HDR_Y      (BASE_Y + 10)
#define GRID_Y          (BASE_Y + 11)
#define GRID_VISIBLE    14

static int  ar_slot     = 0;
static int  ar_cur_step = 0;
static int  ar_cur_col  = 0;       // 0 = pitch, 1..num_cc = ccval column
static int  ar_view_top = 0;
static int  ar_cur_digit = 0;      // 0=hundreds, 1=tens, 2=ones for pitch / ccval

static char ar_name_buf[ZTM_ARPEGGIONAME_MAXLEN] = {0};

// ---------------------------------------------------------------------------
// Helpers

static arpeggio *ar_ensure(int slot) {
    if (slot < 0 || slot >= ZTM_MAX_ARPEGGIOS) return NULL;
    if (!song->arpeggios[slot]) {
        song->arpeggios[slot] = new arpeggio;
    }
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
    if (ar_name_buf[0] == 0 && (!song->arpeggios[slot] || song->arpeggios[slot]->isempty())) return;
    arpeggio *a = ar_ensure(slot);
    if (!a) return;
    if (memcmp(a->name, ar_name_buf, ZTM_ARPEGGIONAME_MAXLEN) != 0) {
        memcpy(a->name, ar_name_buf, ZTM_ARPEGGIONAME_MAXLEN);
        a->name[ZTM_ARPEGGIONAME_MAXLEN - 1] = 0;
        file_changed++;
    }
}

// Format the pitch value for display: "+05", "-03", or "..." for empty.
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
// Constructor / lifecycle

CUI_Arpeggioeditor::CUI_Arpeggioeditor(void) {
    UI = new UserInterface;

    ValueSlider *vs;
    TextInput   *ti;

    // 0 — Slot selector
    vs = new ValueSlider;
    UI->add_element(vs, 0);
    vs->x = 12; vs->y = BASE_Y;
    vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_MAX_ARPEGGIOS - 1;
    vs->value = 0;

    // 1 — Name
    ti = new TextInput;
    UI->add_element(ti, 1);
    ti->frame = 1;
    ti->x = 12; ti->y = BASE_Y + 2;
    ti->xsize = 32; ti->length = ZTM_ARPEGGIONAME_MAXLEN - 1;
    ti->str = (unsigned char*)ar_name_buf;

    // 2 — Length (number of steps)
    vs = new ValueSlider;
    UI->add_element(vs, 2);
    vs->x = 12; vs->y = BASE_Y + 4;
    vs->xsize = 18; vs->ysize = 1;
    vs->min = 1; vs->max = ZTM_ARPEGGIO_LEN;
    vs->value = 1;

    // 3 — Speed (ticks per step)
    vs = new ValueSlider;
    UI->add_element(vs, 3);
    vs->x = 12; vs->y = BASE_Y + 5;
    vs->xsize = 18; vs->ysize = 1;
    vs->min = 1; vs->max = 255;
    vs->value = 1;

    // 4 — Repeat position
    vs = new ValueSlider;
    UI->add_element(vs, 4);
    vs->x = 12; vs->y = BASE_Y + 6;
    vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_ARPEGGIO_LEN - 1;
    vs->value = 0;

    // 5 — num_cc (0..4)
    vs = new ValueSlider;
    UI->add_element(vs, 5);
    vs->x = 12; vs->y = BASE_Y + 7;
    vs->xsize = 18; vs->ysize = 1;
    vs->min = 0; vs->max = ZTM_ARPEGGIO_NUM_CC;
    vs->value = 0;

    // 6..9 — CC# selectors (which 4 controllers this arpeggio drives).
    // Stack on a single row, 4 short sliders with readouts.
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i) {
        vs = new ValueSlider;
        UI->add_element(vs, 6 + i);
        vs->x = 12 + i * 16;
        vs->y = BASE_Y + 8;
        vs->xsize = 8; vs->ysize = 1;
        vs->min = 0; vs->max = 127;
        vs->value = 0;
    }
}

void CUI_Arpeggioeditor::enter(void) {
    need_refresh++;
    cur_state = STATE_ARPEDIT;
    ar_load_name(ar_slot);

    arpeggio *a = song->arpeggios[ar_slot];
    ValueSlider *vs;
    vs = (ValueSlider *)UI->get_element(0); vs->value = ar_slot;
    vs = (ValueSlider *)UI->get_element(2); vs->value = a ? a->length     : 1;
    vs = (ValueSlider *)UI->get_element(3); vs->value = a ? a->speed      : 1;
    vs = (ValueSlider *)UI->get_element(4); vs->value = a ? a->repeat_pos : 0;
    vs = (ValueSlider *)UI->get_element(5); vs->value = a ? a->num_cc     : 0;
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i) {
        vs = (ValueSlider *)UI->get_element(6 + i);
        vs->value = a ? a->cc[i] : 0;
    }
    UI->set_focus(0);
    Keys.flush();
}

void CUI_Arpeggioeditor::leave(void) {
    ar_store_name(ar_slot);
}

// ---------------------------------------------------------------------------
// Update

void CUI_Arpeggioeditor::update() {
    UI->update();

    ValueSlider *vs;
    TextInput   *ti;

    // Slot change
    vs = (ValueSlider *)UI->get_element(0);
    if (vs && vs->value != ar_slot) {
        ar_store_name(ar_slot);
        ar_slot = vs->value;
        ar_cur_step = ar_view_top = ar_cur_col = 0;
        ar_cur_digit = 0;
        ar_load_name(ar_slot);
        arpeggio *a = song->arpeggios[ar_slot];
        ValueSlider *v;
        v = (ValueSlider *)UI->get_element(2); v->value = a ? a->length     : 1;
        v = (ValueSlider *)UI->get_element(3); v->value = a ? a->speed      : 1;
        v = (ValueSlider *)UI->get_element(4); v->value = a ? a->repeat_pos : 0;
        v = (ValueSlider *)UI->get_element(5); v->value = a ? a->num_cc     : 0;
        for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i) {
            v = (ValueSlider *)UI->get_element(6 + i);
            v->value = a ? a->cc[i] : 0;
        }
        need_refresh++;
    }

    // Name typed
    ti = (TextInput *)UI->get_element(1);
    if (ti && ti->changed) { ar_store_name(ar_slot); ti->changed = 0; }

    // Sync metadata sliders into the song's arp entry (auto-creating
    // the entry on first edit). Read the slider values, compare with
    // the entry's stored values, and write back if they differ.
    {
        int v_len    = ((ValueSlider *)UI->get_element(2))->value;
        int v_speed  = ((ValueSlider *)UI->get_element(3))->value;
        int v_repeat = ((ValueSlider *)UI->get_element(4))->value;
        int v_numcc  = ((ValueSlider *)UI->get_element(5))->value;
        int v_cc[ZTM_ARPEGGIO_NUM_CC];
        for (int k = 0; k < ZTM_ARPEGGIO_NUM_CC; ++k) {
            v_cc[k] = ((ValueSlider *)UI->get_element(6 + k))->value;
        }

        arpeggio *a = song->arpeggios[ar_slot];
        bool need_create =
            (!a && (v_len != 1 || v_speed != 1 || v_repeat != 0 || v_numcc != 0
                    || v_cc[0] || v_cc[1] || v_cc[2] || v_cc[3]));
        if (need_create) a = ar_ensure(ar_slot);

        if (a) {
            if (a->length     != v_len)    { a->length     = v_len;    file_changed++; need_refresh++; }
            if (a->speed      != v_speed)  { a->speed      = v_speed;  file_changed++; need_refresh++; }
            if (a->repeat_pos != v_repeat) { a->repeat_pos = v_repeat; file_changed++; need_refresh++; }
            if (a->num_cc     != v_numcc)  { a->num_cc     = v_numcc;  file_changed++; need_refresh++; }
            for (int k = 0; k < ZTM_ARPEGGIO_NUM_CC; ++k) {
                if (a->cc[k] != (unsigned char)v_cc[k]) {
                    a->cc[k] = (unsigned char)v_cc[k];
                    file_changed++; need_refresh++;
                }
            }
        }
    }

    // Grid keybindings: only fire when focus is parked on a non-input
    // tab stop. Sliders and the name TextInput already eat digits/arrows
    // for their own use, so we only run grid input when no UI element
    // currently holds focus (cur_element == -1) — the user can press
    // Tab past the last slider to reach the grid.
    KBKey key = Keys.checkkey();
    int focused = UI->cur_element;
    bool on_grid = (focused < 0) || (focused >= 10);
    if (key && on_grid) {
        arpeggio *a = song->arpeggios[ar_slot];
        int len     = a ? a->length : 1;
        int num_cc  = a ? a->num_cc : 0;
        int max_col = num_cc;   // pitch col 0, then num_cc cc cols
        bool consumed = false;

        // Decimal digit input → write into current cell at digit position.
        int digit = -1;
        if (key >= SDLK_0 && key <= SDLK_9) digit = key - SDLK_0;

        if (digit >= 0) {
            arpeggio *aa = ar_ensure(ar_slot);
            if (aa && ar_cur_step < len) {
                if (ar_cur_col == 0) {
                    // pitch — 3-digit signed semitone, but treat as
                    // unsigned 0..ZTM_ARPEGGIO_LEN range and let user
                    // type the value directly.
                    int p = (int16_t)aa->pitch[ar_cur_step];
                    if (aa->pitch[ar_cur_step] == ZTM_ARP_EMPTY_PITCH) p = 0;
                    if (p < 0) p = -p;
                    if      (ar_cur_digit == 0) p = (p % 100) + digit * 100;
                    else if (ar_cur_digit == 1) p = (p / 100) * 100 + digit * 10 + (p % 10);
                    else                         p = (p / 10) * 10 + digit;
                    if (p > 127) p = 127;
                    // preserve sign from existing
                    int existing = (int16_t)aa->pitch[ar_cur_step];
                    if (existing < 0 && aa->pitch[ar_cur_step] != ZTM_ARP_EMPTY_PITCH) p = -p;
                    aa->pitch[ar_cur_step] = (unsigned short)(int16_t)p;
                } else {
                    int c = ar_cur_col - 1;
                    int v = aa->ccval[c][ar_cur_step];
                    if (v == ZTM_ARP_EMPTY_CCVAL) v = 0;
                    if      (ar_cur_digit == 0) v = (v % 100) + digit * 100;
                    else if (ar_cur_digit == 1) v = (v / 100) * 100 + digit * 10 + (v % 10);
                    else                         v = (v / 10) * 10 + digit;
                    if (v > 127) v = 127;
                    aa->ccval[c][ar_cur_step] = (unsigned char)v;
                }
                ar_cur_digit = (ar_cur_digit + 1) % 3;
                if (ar_cur_digit == 0 && ar_cur_step < len - 1) ar_cur_step++;
                file_changed++;
                consumed = true;
            }
        }
        else if (key == SDLK_MINUS && ar_cur_col == 0) {
            // Negate pitch sign
            arpeggio *aa = ar_ensure(ar_slot);
            if (aa && ar_cur_step < len) {
                int p = (int16_t)aa->pitch[ar_cur_step];
                if (aa->pitch[ar_cur_step] == ZTM_ARP_EMPTY_PITCH) p = 0;
                aa->pitch[ar_cur_step] = (unsigned short)(int16_t)(-p);
                file_changed++;
                consumed = true;
            }
        }
        else if (key == SDLK_PERIOD || key == SDLK_DELETE) {
            arpeggio *aa = ar_ensure(ar_slot);
            if (aa && ar_cur_step < len) {
                if (ar_cur_col == 0)
                    aa->pitch[ar_cur_step] = ZTM_ARP_EMPTY_PITCH;
                else
                    aa->ccval[ar_cur_col - 1][ar_cur_step] = ZTM_ARP_EMPTY_CCVAL;
                ar_cur_digit = 0;
                if (ar_cur_step < len - 1) ar_cur_step++;
                file_changed++;
                consumed = true;
            }
        }
        else {
            switch (key) {
                case SDLK_LEFT:
                    if (ar_cur_col > 0) ar_cur_col--;
                    ar_cur_digit = 0; consumed = true; break;
                case SDLK_RIGHT:
                    if (ar_cur_col < max_col) ar_cur_col++;
                    ar_cur_digit = 0; consumed = true; break;
                case SDLK_UP:
                    if (ar_cur_step > 0) ar_cur_step--;
                    ar_cur_digit = 0; consumed = true; break;
                case SDLK_DOWN:
                    if (ar_cur_step < len - 1) ar_cur_step++;
                    ar_cur_digit = 0; consumed = true; break;
                case SDLK_PAGEUP:
                    ar_cur_step -= GRID_VISIBLE;
                    if (ar_cur_step < 0) ar_cur_step = 0;
                    ar_cur_digit = 0; consumed = true; break;
                case SDLK_PAGEDOWN:
                    ar_cur_step += GRID_VISIBLE;
                    if (ar_cur_step >= len) ar_cur_step = len - 1;
                    ar_cur_digit = 0; consumed = true; break;
                case SDLK_HOME:
                    ar_cur_step = 0; ar_cur_digit = 0; consumed = true; break;
                case SDLK_END:
                    ar_cur_step = len - 1; ar_cur_digit = 0; consumed = true; break;
            }
        }

        // Keep cursor visible
        if (ar_cur_step < ar_view_top)                  ar_view_top = ar_cur_step;
        if (ar_cur_step >= ar_view_top + GRID_VISIBLE)  ar_view_top = ar_cur_step - GRID_VISIBLE + 1;
        if (ar_view_top < 0)                            ar_view_top = 0;

        if (consumed) {
            Keys.getkey();
            need_refresh++;
            doredraw++;
        }
    }
}

// ---------------------------------------------------------------------------
// Draw

void CUI_Arpeggioeditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);
    printtitle(PAGE_TITLE_ROW_Y, "Arpeggio Editor (Shift+F4)", COLORS.Text, COLORS.Background, S);

    arpeggio *a = song->arpeggios[ar_slot];
    int  length     = a ? a->length     : 1;
    int  num_cc     = a ? a->num_cc     : 0;
    int  repeat_pos = a ? a->repeat_pos : 0;

    char buf[64];

    // Right-aligned labels in the leftmost 10 columns; the slider /
    // textinput widgets start at col 12 and ValueSlider draws its own
    // numeric readout at the right end of the bar so we don't paint
    // values ourselves any more.
    print(row(6), col(BASE_Y),     "Slot",   COLORS.Text, S);
    print(row(6), col(BASE_Y + 2), "Name",   COLORS.Text, S);
    print(row(4), col(BASE_Y + 4), "Length", COLORS.Text, S);
    print(row(5), col(BASE_Y + 5), "Speed",  COLORS.Text, S);
    print(row(4), col(BASE_Y + 6), "Repeat", COLORS.Text, S);
    print(row(5), col(BASE_Y + 7), "NumCC",  COLORS.Text, S);
    print(row(7), col(BASE_Y + 8), "CC#",    COLORS.Text, S);

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
    for (int row_i = 0; row_i < GRID_VISIBLE; ++row_i) {
        int step = ar_view_top + row_i;
        int py = GRID_Y + row_i;
        if (step >= length) {
            // erase trailing rows
            printBG(col(GRID_X), row(py), "                                              ",
                    COLORS.Text, COLORS.Background, S);
            continue;
        }
        // step number
        snprintf(buf, sizeof(buf), "%03X", step);
        TColor sf = (step == repeat_pos) ? COLORS.Highlight : COLORS.Text;
        printBG(col(GRID_X), row(py), buf, sf, COLORS.Background, S);

        // pitch
        char pbuf[4];
        format_pitch(a ? a->pitch[step] : ZTM_ARP_EMPTY_PITCH, pbuf);
        TColor fg = (step == ar_cur_step && ar_cur_col == 0) ? COLORS.EditBG : COLORS.EditText;
        TColor bg = (step == ar_cur_step && ar_cur_col == 0) ? COLORS.Highlight : COLORS.EditBG;
        printBG(col(GRID_X + 6), row(py), pbuf, fg, bg, S);

        // ccvals
        for (int i = 0; i < num_cc; ++i) {
            char vbuf[4];
            format_ccval(a ? a->ccval[i][step] : ZTM_ARP_EMPTY_CCVAL, vbuf);
            int cell_col = GRID_X + 11 + i * 5;
            TColor f = (step == ar_cur_step && ar_cur_col == i + 1) ? COLORS.EditBG : COLORS.EditText;
            TColor b = (step == ar_cur_step && ar_cur_col == i + 1) ? COLORS.Highlight : COLORS.EditBG;
            printBG(col(cell_col), row(py), vbuf, f, b, S);
        }
    }

    // Status hint below the grid
    {
        int hint_y = GRID_Y + GRID_VISIBLE + 1;
        print(row(4), col(hint_y),
              "Arrows nav | digits 0-9 type value | '-' negate pitch | '.' clear",
              COLORS.Text, S);
    }

    need_refresh = 0; updated = 2;
    ztPlayer->num_orders();
    S->unlock();
}
