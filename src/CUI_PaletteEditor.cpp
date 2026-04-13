#include "zt.h"
#include "CUI_PaletteEditor.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Palette Editor (Shift+F12)
//
// Inspired by Impulse Tracker v2.14's Palette Configuration screen.
// Displays every editable COLORS.* slot as a color swatch with R/G/B values
// and provides a panel of predefined palette presets on the right.
//
// Navigation:
//   Arrow keys           - move within current panel (grid or presets)
//   Tab / Shift+Tab      - switch focus between swatch grid and preset panel
//   R / G / B            - select a channel of the focused swatch to edit
//   + / -                - increase/decrease selected channel by 1
//   Shift + (+/-)        - increase/decrease selected channel by 16
//   0..9                 - replace selected channel with that decimal digit
//                         (multiple digits accumulate as base-10)
//   Enter                - on a preset, load that preset
//   Ctrl+S               - save current palette to palettes/custom.conf
//   ESC                  - revert to snapshot taken on entry and return
//                         to pattern editor
// ---------------------------------------------------------------------------

// Layout constants (character cells are 8x8 px).
// Grid: 6 rows x 3 cols, each cell is a "slot box":
//   [ Slot Name        ]   <- 1 row label
//   [###### swatch ####]   <- PE_SWATCH_H rows (solid color block)
//   [###### swatch ####]
//   [###### swatch ####]
//   [ R 255            ]   <- 3 rows of RGB values
//   [ G 128            ]
//   [ B  64            ]
// Total ~8 rows tall, ~18 cols wide per slot cell.
#define PE_GRID_COL_X       4     // first swatch column (in char cells)
#define PE_GRID_COL_STEP    24    // spacing between swatch columns
#define PE_GRID_ROW_Y       12    // first swatch row (in char cells)
#define PE_GRID_ROW_STEP    7     // spacing between swatch rows
#define PE_GRID_COLS        3
#define PE_GRID_ROWS        ((NUM_PALETTE_SLOTS + PE_GRID_COLS - 1) / PE_GRID_COLS)

#define PE_SWATCH_W         18    // swatch width in char cells (solid color block)
#define PE_SWATCH_H         3     // swatch height in char cells

#define PE_PRESET_X         80    // preset panel left edge (char cells)
#define PE_PRESET_Y         12    // preset panel top
#define PE_PRESET_W         22

struct PaletteSlot {
    const char *name;
    size_t      offset;   // byte offset of TColor field in colorset
};

// Order matches IT-style "named-rows" layout: a small number of related
// colors are grouped into each preset row.
static const PaletteSlot g_slots[NUM_PALETTE_SLOTS] = {
    { "Background",     offsetof(colorset, Background) },
    { "Highlight",      offsetof(colorset, Highlight) },
    { "Lowlight",       offsetof(colorset, Lowlight) },
    { "Text",           offsetof(colorset, Text) },
    { "Brighttext",     offsetof(colorset, Brighttext) },
    { "Data",           offsetof(colorset, Data) },
    { "Black",          offsetof(colorset, Black) },
    { "EditText",       offsetof(colorset, EditText) },
    { "EditBG",         offsetof(colorset, EditBG) },
    { "EditBGlow",      offsetof(colorset, EditBGlow) },
    { "EditBGhigh",     offsetof(colorset, EditBGhigh) },
    { "SelectedBGLow",  offsetof(colorset, SelectedBGLow) },
    { "SelectedBGHigh", offsetof(colorset, SelectedBGHigh) },
    { "CursorRowLow",   offsetof(colorset, CursorRowLow) },
    { "CursorRowHigh",  offsetof(colorset, CursorRowHigh) },
    { "LCDLow",         offsetof(colorset, LCDLow) },
    { "LCDMid",         offsetof(colorset, LCDMid) },
    { "LCDHigh",        offsetof(colorset, LCDHigh) },
};

struct PalettePreset {
    const char *label;
    const char *file;
};

// Two kinds of preset entries: a .conf in palettes/ (prefix "p/") or a
// skin colors.conf (prefix "s/"). load_palette_file() strips the prefix
// and routes to the right path.
static const PalettePreset g_presets[NUM_PALETTE_PRESETS] = {
    { "Light Blue",        "p/light_blue.conf" },
    { "Gold",              "p/gold.conf" },
    { "Camouflage",        "p/camouflage.conf" },
    { "Midnight Tracking", "p/midnight_tracking.conf" },
    { "Pine Colours",      "p/pine_colours.conf" },
    { "Soundtracker",      "p/soundtracker.conf" },
    { "Volcanic",          "p/volcanic.conf" },
    { "Skin: default",      "s/default" },
    { "Skin: professional", "s/professional" },
    { "Skin: blue-c64",     "s/blue-c64" },
    { "Skin: reaktor",      "s/reaktor" },
    { "Skin: tekstyle",     "s/tekstyle" },
    { "Skin: x.seed",       "s/x.seed" },
    { "Skin: xt-g01",       "s/xt-g01" },
};

static TColor *slot_color_ptr(int slot) {
    if (slot < 0 || slot >= NUM_PALETTE_SLOTS) return NULL;
    char *base = (char *)&COLORS;
    return (TColor *)(base + g_slots[slot].offset);
}

static void unpack_rgb(TColor c, unsigned char *r, unsigned char *g, unsigned char *b) {
    *r = (c >> 16) & 0xFF;
    *g = (c >>  8) & 0xFF;
    *b = (c      ) & 0xFF;
}

static TColor pack_rgb(unsigned char r, unsigned char g, unsigned char b) {
    return (TColor)(0xFF000000u | ((TColor)b) | (((TColor)g) << 8) | (((TColor)r) << 16));
}

CUI_PaletteEditor::CUI_PaletteEditor(void) {
    UI = new UserInterface;
    selected_slot = 0;
    focus_panel = 0;
    channel_edit = 0;
    dirty = 0;
    status_line[0] = '\0';
}

CUI_PaletteEditor::~CUI_PaletteEditor(void) {
    if (UI) delete UI;
    UI = NULL;
}

void CUI_PaletteEditor::enter(void) {
    need_refresh = 1;
    cur_state = STATE_PALETTE_EDITOR;
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        snapshot[i] = *slot_color_ptr(i);
    }
    dirty = 0;
    channel_edit = 0;
    if (selected_slot < 0) selected_slot = 0;
    if (selected_slot >= NUM_PALETTE_SLOTS) selected_slot = 0;
    status_line[0] = '\0';
    Keys.flush();
    UI->enter();
}

void CUI_PaletteEditor::leave(void) {
    channel_edit = 0;
}

void CUI_PaletteEditor::load_palette_file(const char *fname) {
    char path[MAX_PATH + 1];
    const char *base = cur_dir ? cur_dir : ".";
    const char *sep =
#if defined(_WIN32)
        "\\";
#else
        "/";
#endif
    // Route by prefix: "p/<file>" -> palettes/<file>, "s/<name>" -> skins/<name>/colors.conf
    if (strncmp(fname, "p/", 2) == 0) {
        snprintf(path, sizeof(path), "%s%spalettes%s%s", base, sep, sep, fname + 2);
    } else if (strncmp(fname, "s/", 2) == 0) {
        snprintf(path, sizeof(path), "%s%sskins%s%s%scolors.conf", base, sep, sep, fname + 2, sep);
    } else {
        snprintf(path, sizeof(path), "%s%spalettes%s%s", base, sep, sep, fname);
    }
    if (COLORS.load(path)) {
        dirty = 1;
        snprintf(status_line, sizeof(status_line), "Loaded palette: %s", fname);
        doredraw++;
        need_refresh++;
        screenmanager.UpdateAll();
    } else {
        snprintf(status_line, sizeof(status_line), "Failed to load %s", fname);
    }
}

void CUI_PaletteEditor::save_palette_file(const char *fname) {
    char path[MAX_PATH + 1];
#if defined(_WIN32)
    snprintf(path, sizeof(path), "%s\\palettes\\%s", cur_dir ? cur_dir : ".", fname);
#else
    snprintf(path, sizeof(path), "%s/palettes/%s", cur_dir ? cur_dir : ".", fname);
#endif
    FILE *f = fopen(path, "w");
    if (!f) {
        snprintf(status_line, sizeof(status_line), "Cannot open %s for writing", fname);
        return;
    }
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        TColor c = *slot_color_ptr(i);
        unsigned char r, g, b;
        unpack_rgb(c, &r, &g, &b);
        char name_with_colon[64];
        snprintf(name_with_colon, sizeof(name_with_colon), "%s:", g_slots[i].name);
        fprintf(f, "%-16s #%02X%02X%02X\n", name_with_colon, r, g, b);
    }
    fclose(f);
    snprintf(status_line, sizeof(status_line), "Saved palette to palettes/%s", fname);
}

void CUI_PaletteEditor::apply_channel_delta(int delta) {
    if (channel_edit < 1 || channel_edit > 3) return;
    TColor *c = slot_color_ptr(selected_slot);
    if (!c) return;
    unsigned char r, g, b;
    unpack_rgb(*c, &r, &g, &b);
    int v = 0;
    if (channel_edit == 1) v = r;
    else if (channel_edit == 2) v = g;
    else v = b;
    v += delta;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    if (channel_edit == 1) r = (unsigned char)v;
    else if (channel_edit == 2) g = (unsigned char)v;
    else b = (unsigned char)v;
    *c = pack_rgb(r, g, b);
    dirty = 1;
    doredraw++;
    need_refresh++;
    screenmanager.UpdateAll();
}

void CUI_PaletteEditor::apply_channel_set(int value) {
    if (channel_edit < 1 || channel_edit > 3) return;
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    TColor *c = slot_color_ptr(selected_slot);
    if (!c) return;
    unsigned char r, g, b;
    unpack_rgb(*c, &r, &g, &b);
    if (channel_edit == 1) r = (unsigned char)value;
    else if (channel_edit == 2) g = (unsigned char)value;
    else b = (unsigned char)value;
    *c = pack_rgb(r, g, b);
    dirty = 1;
    doredraw++;
    need_refresh++;
    screenmanager.UpdateAll();
}

void CUI_PaletteEditor::update(void) {
    static int digit_accum = -1;          // when >=0, new keystrokes append base-10 digits
    static int digit_channel = 0;         // channel digit_accum belongs to

    UI->update();
    if (!Keys.size()) return;

    KBMod kstate = Keys.getstate();
    KBKey key = Keys.getkey();
    int handled = 1;

    // Reset digit accumulator on focus change or non-digit keys.
    auto reset_accum = [&]() {
        digit_accum = -1;
        digit_channel = 0;
    };

    switch (key) {
        case SDLK_ESCAPE:
            for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
                TColor *c = slot_color_ptr(i);
                if (c) *c = snapshot[i];
            }
            dirty = 0;
            channel_edit = 0;
            screenmanager.UpdateAll();
            switch_page(UIP_Patterneditor);
            doredraw++;
            reset_accum();
            return;

        case SDLK_TAB:
            focus_panel = (focus_panel + 1) % 2;
            channel_edit = 0;
            reset_accum();
            need_refresh++;
            return;

        case SDLK_UP:
            reset_accum();
            if (focus_panel == 0) {
                int row = selected_slot / PE_GRID_COLS;
                int col_ = selected_slot % PE_GRID_COLS;
                if (row > 0) {
                    selected_slot = (row - 1) * PE_GRID_COLS + col_;
                    if (selected_slot >= NUM_PALETTE_SLOTS) selected_slot -= PE_GRID_COLS;
                }
            } else {
                if (selected_slot > 0) selected_slot--;
            }
            channel_edit = 0;
            need_refresh++;
            return;

        case SDLK_DOWN:
            reset_accum();
            if (focus_panel == 0) {
                int row = selected_slot / PE_GRID_COLS;
                int col_ = selected_slot % PE_GRID_COLS;
                int next = (row + 1) * PE_GRID_COLS + col_;
                if (next < NUM_PALETTE_SLOTS) selected_slot = next;
            } else {
                if (selected_slot < NUM_PALETTE_PRESETS - 1) selected_slot++;
            }
            channel_edit = 0;
            need_refresh++;
            return;

        case SDLK_LEFT:
            reset_accum();
            if (focus_panel == 0) {
                int col_ = selected_slot % PE_GRID_COLS;
                if (col_ > 0) selected_slot--;
            } else {
                focus_panel = 0;
                if (selected_slot >= NUM_PALETTE_SLOTS) selected_slot = NUM_PALETTE_SLOTS - 1;
            }
            channel_edit = 0;
            need_refresh++;
            return;

        case SDLK_RIGHT:
            reset_accum();
            if (focus_panel == 0) {
                int col_ = selected_slot % PE_GRID_COLS;
                if (col_ < PE_GRID_COLS - 1 && (selected_slot + 1) < NUM_PALETTE_SLOTS) {
                    selected_slot++;
                } else {
                    focus_panel = 1;
                    selected_slot = 0;
                }
            }
            channel_edit = 0;
            need_refresh++;
            return;

        case SDLK_RETURN:
            reset_accum();
            if (focus_panel == 1 && selected_slot >= 0 && selected_slot < NUM_PALETTE_PRESETS) {
                load_palette_file(g_presets[selected_slot].file);
            }
            return;

        case SDLK_R:
            if (focus_panel == 0) {
                channel_edit = 1;
                reset_accum();
                snprintf(status_line, sizeof(status_line),
                         "Editing R channel of %s (use +/- or 0-9, Tab to exit)",
                         g_slots[selected_slot].name);
                need_refresh++;
            }
            return;

        case SDLK_G:
            if (focus_panel == 0) {
                channel_edit = 2;
                reset_accum();
                snprintf(status_line, sizeof(status_line),
                         "Editing G channel of %s (use +/- or 0-9, Tab to exit)",
                         g_slots[selected_slot].name);
                need_refresh++;
            }
            return;

        case SDLK_B:
            if (focus_panel == 0) {
                channel_edit = 3;
                reset_accum();
                snprintf(status_line, sizeof(status_line),
                         "Editing B channel of %s (use +/- or 0-9, Tab to exit)",
                         g_slots[selected_slot].name);
                need_refresh++;
            }
            return;

        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
            reset_accum();
            apply_channel_delta((kstate & KS_SHIFT) ? 16 : 1);
            return;

        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            reset_accum();
            apply_channel_delta((kstate & KS_SHIFT) ? -16 : -1);
            return;

        case SDLK_S:
            if (kstate & KS_CTRL) {
                save_palette_file("custom.conf");
                need_refresh++;
                return;
            }
            handled = 0;
            break;

        default:
            handled = 0;
            break;
    }

    // Digit accumulator for direct numeric entry into the focused channel.
    if (channel_edit >= 1 && channel_edit <= 3 && key >= SDLK_0 && key <= SDLK_9) {
        int digit = (int)(key - SDLK_0);
        if (digit_channel != channel_edit) {
            digit_accum = -1;
            digit_channel = channel_edit;
        }
        if (digit_accum < 0) digit_accum = 0;
        digit_accum = digit_accum * 10 + digit;
        if (digit_accum > 255) digit_accum = digit;
        apply_channel_set(digit_accum);
        return;
    }

    if (!handled) {
        reset_accum();
    }
}

// Pick a readable text color (Brighttext or Black) against an arbitrary
// swatch background, based on perceived luminance.
static TColor contrast_fg(TColor c) {
    unsigned char r, g, b;
    unpack_rgb(c, &r, &g, &b);
    // Rec. 601 luma (integer approximation).
    int luma = (299 * (int)r + 587 * (int)g + 114 * (int)b) / 1000;
    return (luma < 128) ? COLORS.Brighttext : COLORS.Black;
}

// Draw a single slot cell: name label, big solid color swatch, RGB readout.
// Selection is shown as a thick bright outline that surrounds the whole cell
// (not just the swatch) so the user can see at a glance which slot is active.
static void draw_slot_cell(Drawable *S, int slot, int selected, int channel_edit) {
    int gr = slot / PE_GRID_COLS;
    int gc = slot % PE_GRID_COLS;
    int cx = PE_GRID_COL_X + gc * PE_GRID_COL_STEP;
    int cy = PE_GRID_ROW_Y + gr * PE_GRID_ROW_STEP;

    TColor c = *slot_color_ptr(slot);
    unsigned char r, g, b;
    unpack_rgb(c, &r, &g, &b);

    // --- Cell outer rectangle (for selection border) ---------------------
    // Cell spans: name row, swatch rows, RGB rows (3).
    int cell_rows = 1 /*label*/ + PE_SWATCH_H + 3 /*R,G,B*/;
    int ox0 = col(cx) - 2;
    int oy0 = row(cy) - 2;
    int ox1 = col(cx + PE_SWATCH_W) + 2;
    int oy1 = row(cy + cell_rows) + 2;

    if (selected) {
        // Thick 2-px bright border around entire cell (clearly visible even
        // on dark backgrounds).
        TColor bc = COLORS.Brighttext;
        S->drawHLine(oy0,     ox0, ox1, bc);
        S->drawHLine(oy0 + 1, ox0, ox1, bc);
        S->drawHLine(oy1 - 1, ox0, ox1, bc);
        S->drawHLine(oy1 - 2, ox0, ox1, bc);
        S->drawVLine(ox0,     oy0, oy1, bc);
        S->drawVLine(ox0 + 1, oy0, oy1, bc);
        S->drawVLine(ox1 - 1, oy0, oy1, bc);
        S->drawVLine(ox1 - 2, oy0, oy1, bc);
    }

    // --- Slot name label (above swatch) ----------------------------------
    // Highlighted when selected so the user's eye tracks the name + border.
    TColor name_col = selected ? COLORS.Brighttext : COLORS.Text;
    print(col(cx), row(cy), g_slots[slot].name, name_col, S);

    // --- Big solid color swatch ------------------------------------------
    int sx0 = col(cx);
    int sy0 = row(cy + 1);
    int sx1 = col(cx + PE_SWATCH_W);
    int sy1 = row(cy + 1 + PE_SWATCH_H);
    S->fillRect(sx0, sy0, sx1, sy1, c);

    // Thin inner border so the swatch reads as a distinct rectangle even if
    // the color matches the page background.
    TColor sb = COLORS.Lowlight;
    S->drawHLine(sy0,     sx0, sx1, sb);
    S->drawHLine(sy1 - 1, sx0, sx1, sb);
    S->drawVLine(sx0,     sy0, sy1, sb);
    S->drawVLine(sx1 - 1, sy0, sy1, sb);

    // --- Mini preview text inside the swatch -----------------------------
    // A short label drawn ON the swatch, colored for contrast, so the user
    // can gauge readability of each color at a glance.
    TColor fg = contrast_fg(c);
    print(sx0 + col(1), sy0 + row(1), "C-5 01..v40", fg, S);

    // --- Three RGB rows beneath the swatch -------------------------------
    char buf[32];
    int rgb_y = cy + 1 + PE_SWATCH_H;
    TColor base = selected ? COLORS.Brighttext : COLORS.Text;
    TColor r_col = (selected && channel_edit == 1) ? COLORS.LCDHigh : base;
    TColor g_col = (selected && channel_edit == 2) ? COLORS.LCDHigh : base;
    TColor b_col = (selected && channel_edit == 3) ? COLORS.LCDHigh : base;
    snprintf(buf, sizeof(buf), "R %3d", (int)r);
    print(col(cx), row(rgb_y),     buf, r_col, S);
    snprintf(buf, sizeof(buf), "G %3d", (int)g);
    print(col(cx), row(rgb_y + 1), buf, g_col, S);
    snprintf(buf, sizeof(buf), "B %3d", (int)b);
    print(col(cx), row(rgb_y + 2), buf, b_col, S);
}

void CUI_PaletteEditor::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);

    printtitle(PAGE_TITLE_ROW_Y,
               "Palette Editor (Shift+F12)",
               COLORS.Text, COLORS.Background, S);

    // ---------------------------------------------------------------------
    // Swatch grid — 6 rows x 3 cols. Each slot has its own label matching
    // the TColor field name, a big solid color block, and stacked R/G/B
    // values. The currently-selected slot gets a thick bright border.
    // ---------------------------------------------------------------------
    for (int i = 0; i < NUM_PALETTE_SLOTS; ++i) {
        int sel = (focus_panel == 0 && i == selected_slot);
        draw_slot_cell(S, i, sel, sel ? channel_edit : 0);
    }

    // ---------------------------------------------------------------------
    // Predefined Palettes panel (right side) — unchanged layout.
    // ---------------------------------------------------------------------
    int px = col(PE_PRESET_X);
    int py = row(PE_PRESET_Y);
    int panel_h_rows = 2 + NUM_PALETTE_PRESETS * 2;
    S->fillRect(px - 2, py - 2,
                px + col(PE_PRESET_W) + 2,
                py + row(panel_h_rows),
                COLORS.Background);
    S->drawHLine(py - 2,                  px - 2, px + col(PE_PRESET_W) + 2, COLORS.Lowlight);
    S->drawHLine(py + row(panel_h_rows) - 1, px - 2, px + col(PE_PRESET_W) + 2, COLORS.Lowlight);
    S->drawVLine(px - 2,                  py - 2, py + row(panel_h_rows), COLORS.Lowlight);
    S->drawVLine(px + col(PE_PRESET_W) + 1, py - 2, py + row(panel_h_rows), COLORS.Lowlight);

    print(px, py, "Predefined Palettes", COLORS.Brighttext, S);
    for (int i = 0; i < NUM_PALETTE_PRESETS; ++i) {
        int sel = (focus_panel == 1 && i == selected_slot);
        TColor fg = sel ? COLORS.Brighttext : COLORS.Text;
        TColor bg = sel ? COLORS.SelectedBGHigh : COLORS.Background;
        char line[64];
        snprintf(line, sizeof(line), " %-20s ", g_presets[i].label);
        printBG(px, py + row(2 + i * 2), line, fg, bg, S);
    }

    // ---------------------------------------------------------------------
    // Bottom hint / status area.
    // ---------------------------------------------------------------------
    int hy = row(PE_GRID_ROW_Y + PE_GRID_ROWS * PE_GRID_ROW_STEP + 1);
    const char *hint;
    if (focus_panel == 0) {
        if (channel_edit) {
            const char *ch_name = (channel_edit == 1) ? "R" :
                                  (channel_edit == 2) ? "G" : "B";
            char tmp[128];
            snprintf(tmp, sizeof(tmp),
                     "Editing %s channel: 0-9 type value,  +/- step (Shift=x16),  Tab cancels",
                     ch_name);
            print(col(2), hy, tmp, COLORS.LCDHigh, S);
            hint = NULL;
        } else {
            hint = "Arrows: move  Tab: presets  R/G/B: edit channel  Ctrl+S: save  ESC: revert";
        }
    } else {
        hint = "Arrows: move  Enter: load preset  Tab/Left: back to grid  ESC: revert";
    }
    if (hint) print(col(2), hy, hint, COLORS.Text, S);
    if (status_line[0]) {
        print(col(2), hy + row(1), status_line, COLORS.Brighttext, S);
    }
    if (dirty) {
        print(col(2), hy + row(2), "[modified - press Ctrl+S to save, ESC to revert]",
              COLORS.LCDHigh, S);
    }

    need_refresh = 0;
    updated = 2;
    S->unlock();
}
