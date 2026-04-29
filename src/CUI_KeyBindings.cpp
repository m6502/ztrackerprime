// CUI_KeyBindings — unified Shortcuts & MIDI Mappings editor.
//
// Lists every ZtAction with its keyboard binding AND its three MIDI
// mapping slots in a single grid:
//
//   Action                    Keyboard           MIDI 1   MIDI 2   MIDI 3
//   --------------------------------------------------------------------
//   Quit zTracker             ctrl+alt+q         ----     ----     ----
//   Play Song from start      f5                 ----     ----     ----
//   Audition current note (4) (none)             90 3C    ----     ----
//   ...
//
// Cursor walks a 2-D grid:
//   cursor_y = action row     (0 reserved for ZT_ACTION_NONE, skipped)
//   cursor_x = column         (0 = keyboard, 1..N = MIDI slot 0..N-1)
//
// Enter / Space:
//   * On the Keyboard column   -> capture mode, next keystroke binds.
//   * On a MIDI slot column    -> Learn mode, next incoming MIDI message
//                                 is captured into the slot.
//
// Backspace / Delete:
//   * Keyboard column -> unbind the action.
//   * MIDI slot       -> clear the slot.
//
// Ctrl+S writes the whole table (keyboard + MIDI) back to zt.conf via
// the same save path that the rest of the app uses.

#include "zt.h"
#include "keybindings.h"
#include "midi_mappings.h"
#include "conf.h"

#include <SDL.h>
#include <string.h>
#include <stdio.h>

// Grid layout. The page targets a 640px-wide internal resolution so
// every column has a fixed character width and the highlight bar
// renders as a clean rectangle.
#define KB_LIST_X        2
#define KB_LIST_Y        11
#define KB_DESC_COL      1     // action description (English)
#define KB_DESC_WIDTH    36
#define KB_KEY_COL       (KB_DESC_COL + KB_DESC_WIDTH + 1)
#define KB_KEY_WIDTH     18
#define KB_MIDI_COL_0    (KB_KEY_COL + KB_KEY_WIDTH + 1)
#define KB_MIDI_WIDTH    6
#define KB_NUM_COLUMNS   (1 + ZT_MM_SLOTS_PER_ACTION)   // 0 = key, 1..3 = MIDI

CUI_KeyBindings::CUI_KeyBindings(void)
{
    UI = NULL;      // bypass the UserInterface framework
    cursor_y = 1;   // skip ZT_ACTION_NONE
    cursor_x = 0;
    list_start = 0;
    capturing = 0;
    dirty = 0;
    status_line[0] = '\0';
}

int CUI_KeyBindings::visible_rows(void) const
{
    int total_rows = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int rows = total_rows - KB_LIST_Y - 8 /*toolbar*/ - 2 /*status*/;
    if (rows < 4) rows = 4;
    return rows;
}

void CUI_KeyBindings::enter(void)
{
    cur_state = STATE_KEYBINDINGS;
    need_refresh = 1;
    capturing = 0;
    Keys.flush();
    zt_mm_set_learn_target(ZT_ACTION_NONE, -1);
    snprintf(status_line, sizeof(status_line),
             "UP/DN/L/R move  ENTER edit  DEL clear  Ctrl+S save  Ctrl+R reset  Ctrl+Shift+R reset-all  ESC exit");
}

void CUI_KeyBindings::leave(void)
{
    zt_mm_set_learn_target(ZT_ACTION_NONE, -1);
}

// Helpers -----------------------------------------------------------------

namespace {

int midi_slot_for_column(int col_x) {
    // Columns are 0 = key, 1..ZT_MM_SLOTS_PER_ACTION = midi slot 0..N-1.
    return col_x - 1;
}

bool is_key_column(int col_x) { return col_x == 0; }
bool is_midi_column(int col_x) {
    return col_x >= 1 && col_x <= ZT_MM_SLOTS_PER_ACTION;
}

} // namespace

// Update -------------------------------------------------------------------

void CUI_KeyBindings::update(void)
{
    // While Learn mode is active the MIDI thread will eventually
    // clear the target; refresh the status line every frame so the
    // user sees what they are capturing.
    int learn_a = zt_mm_get_learn_action();
    int learn_s = zt_mm_get_learn_slot();
    if (learn_a != ZT_ACTION_NONE) {
        snprintf(status_line, sizeof(status_line),
                 "LEARN: send MIDI for %s slot %d   (ESC to cancel)",
                 KeyBindings::actionDescription((ZtAction)learn_a),
                 learn_s + 1);
    }

    if (!Keys.size()) return;

    KBMod kstate = Keys.getstate();
    KBKey key = Keys.getkey();

    // ----- Keyboard capture mode: next non-modifier key becomes binding.
    if (capturing) {
        if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
            key == SDLK_LCTRL  || key == SDLK_RCTRL  ||
            key == SDLK_LALT   || key == SDLK_RALT   ||
            key == SDLK_LGUI   || key == SDLK_RGUI) {
            need_refresh++;
            return;
        }
        if (key == SDLK_ESCAPE) {
            capturing = 0;
            snprintf(status_line, sizeof(status_line), "Capture cancelled.");
            need_refresh++;
            return;
        }
        if (cursor_y > 0 && cursor_y < ZT_ACTION_COUNT) {
            g_keybindings.bindings[cursor_y].key = (int)key;
            g_keybindings.bindings[cursor_y].modifiers = (int)kstate;
            dirty = 1;
            char combo[64];
            KeyBindings::formatKeyCombo((int)key, (int)kstate, combo, sizeof(combo));
            snprintf(status_line, sizeof(status_line),
                     "Bound %s -> %s (Ctrl+S to save)",
                     KeyBindings::actionDescription((ZtAction)cursor_y), combo);
        }
        capturing = 0;
        need_refresh++;
        return;
    }

    int rows = visible_rows();
    int total = ZT_ACTION_COUNT - 1;

    // ESC: cancel Learn first if active, otherwise exit page.
    if (key == SDLK_ESCAPE) {
        if (learn_a != ZT_ACTION_NONE) {
            zt_mm_set_learn_target(ZT_ACTION_NONE, -1);
            snprintf(status_line, sizeof(status_line), "Learn cancelled.");
            need_refresh++;
            return;
        }
        switch_page(UIP_Patterneditor);
        return;
    }

    // Ctrl+S: save the whole settings (keyboard + MIDI mappings) to
    // zt.conf. zt_config_globals.save() walks both tables.
    if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_S) {
        g_keybindings.save(zt_config_globals.Config);
        zt_config_globals.save();
        dirty = 0;
        snprintf(status_line, sizeof(status_line),
                 "Saved keyboard + MIDI mappings to zt.conf.");
        need_refresh++;
        return;
    }

    // Ctrl+Shift+R: reset ALL keyboard bindings to defaults. (MIDI
    // mappings are user-only; resetting them needs a manual clear.)
    if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_R) {
        g_keybindings.setDefaults();
        dirty = 1;
        snprintf(status_line, sizeof(status_line),
                 "All keyboard bindings reset to defaults. Ctrl+S to save.");
        need_refresh++;
        return;
    }

    // Ctrl+R: reset the current row's keyboard binding to default.
    if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_R) {
        if (cursor_y > 0 && cursor_y < ZT_ACTION_COUNT) {
            KeyBinding saved[ZT_ACTION_COUNT];
            memcpy(saved, g_keybindings.bindings, sizeof(saved));
            g_keybindings.setDefaults();
            KeyBinding def = g_keybindings.bindings[cursor_y];
            memcpy(g_keybindings.bindings, saved, sizeof(saved));
            g_keybindings.bindings[cursor_y] = def;
            dirty = 1;
            char combo[64];
            KeyBindings::formatKeyCombo(def.key, def.modifiers, combo, sizeof(combo));
            snprintf(status_line, sizeof(status_line),
                     "Reset %s key to %s. Ctrl+S to save.",
                     KeyBindings::actionDescription((ZtAction)cursor_y), combo);
        }
        need_refresh++;
        return;
    }

    // DEL / BACKSPACE: clear the focused cell.
    if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
        if (is_key_column(cursor_x) && cursor_y > 0 && cursor_y < ZT_ACTION_COUNT) {
            g_keybindings.bindings[cursor_y].key = 0;
            g_keybindings.bindings[cursor_y].modifiers = 0;
            dirty = 1;
            snprintf(status_line, sizeof(status_line),
                     "Unbound %s. Ctrl+S to save.",
                     KeyBindings::actionDescription((ZtAction)cursor_y));
        } else if (is_midi_column(cursor_x)) {
            int slot = midi_slot_for_column(cursor_x);
            zt_mm_clear_slot(cursor_y, slot);
            dirty = 1;
            snprintf(status_line, sizeof(status_line),
                     "Cleared %s MIDI slot %d. Ctrl+S to save.",
                     KeyBindings::actionDescription((ZtAction)cursor_y), slot + 1);
        }
        need_refresh++;
        return;
    }

    // Enter / Space: enter edit mode for the focused cell.
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
        if (is_key_column(cursor_x)) {
            capturing = 1;
            Keys.flush();
            snprintf(status_line, sizeof(status_line),
                     "Press new key combo for '%s' (ESC to cancel)...",
                     KeyBindings::actionDescription((ZtAction)cursor_y));
        } else if (is_midi_column(cursor_x)) {
            int slot = midi_slot_for_column(cursor_x);
            zt_mm_set_learn_target(cursor_y, slot);
            snprintf(status_line, sizeof(status_line),
                     "LEARN: send MIDI for %s slot %d (ESC to cancel)",
                     KeyBindings::actionDescription((ZtAction)cursor_y), slot + 1);
        }
        need_refresh++;
        return;
    }

    // Navigation. Up/Down move row, Left/Right move column. Home /
    // End jump to first / last action; PageUp / PageDn jump 10
    // actions at a time.
    auto clamp = [&]() {
        if (cursor_y < 1) cursor_y = 1;
        if (cursor_y > total) cursor_y = total;
        if (cursor_x < 0) cursor_x = 0;
        if (cursor_x >= KB_NUM_COLUMNS) cursor_x = KB_NUM_COLUMNS - 1;
        if (cursor_y - list_start < 0) list_start = cursor_y;
        if (cursor_y - list_start >= rows) list_start = cursor_y - rows + 1;
        if (list_start < 0) list_start = 0;
        if (list_start > total - rows + 1) list_start = total - rows + 1;
        if (list_start < 0) list_start = 0;
    };

    switch (key) {
        case SDLK_UP:       cursor_y--;           break;
        case SDLK_DOWN:     cursor_y++;           break;
        case SDLK_LEFT:     cursor_x--;           break;
        case SDLK_RIGHT:    cursor_x++;           break;
        case SDLK_PAGEUP:   cursor_y -= 10;       break;
        case SDLK_PAGEDOWN: cursor_y += 10;       break;
        case SDLK_HOME:     cursor_y = 1;         break;
        case SDLK_END:      cursor_y = total;     break;
        default: break;
    }
    clamp();
    need_refresh++;
}

// Draw ---------------------------------------------------------------------

void CUI_KeyBindings::draw(Drawable *S)
{
    if (S->lock() != 0) return;

    int rows = visible_rows();
    S->fillRect(col(1),
                row(KB_LIST_Y - 1),
                INTERNAL_RESOLUTION_X - 2,
                row(KB_LIST_Y + rows + 2),
                COLORS.Background);

    char title[96];
    snprintf(title, sizeof(title), "Shortcuts & MIDI Mappings (Shift+F3)%s",
             dirty ? "   [unsaved]" : "");
    printtitle(PAGE_TITLE_ROW_Y, title, COLORS.Text, COLORS.Background, S);

    // Column headers.
    print(col(KB_LIST_X + KB_DESC_COL), row(KB_LIST_Y - 1),
          "Action", COLORS.Brighttext, S);
    print(col(KB_LIST_X + KB_KEY_COL), row(KB_LIST_Y - 1),
          "Keyboard", COLORS.Brighttext, S);
    for (int s = 0; s < ZT_MM_SLOTS_PER_ACTION; s++) {
        char hdr[16];
        snprintf(hdr, sizeof(hdr), "MIDI%d", s + 1);
        print(col(KB_LIST_X + KB_MIDI_COL_0 + s * (KB_MIDI_WIDTH + 1)),
              row(KB_LIST_Y - 1), hdr, COLORS.Brighttext, S);
    }

    int total = ZT_ACTION_COUNT - 1;
    ZtMmMappings *mm = zt_mm_get_mappings();
    int learn_a = zt_mm_get_learn_action();
    int learn_s = zt_mm_get_learn_slot();

    for (int i = 0; i < rows; i++) {
        int action_idx = list_start + i + 1;  // skip ZT_ACTION_NONE
        if (action_idx > total) break;

        int y = row(KB_LIST_Y + i);
        bool is_cur_row = (action_idx == cursor_y);

        // Action description column. Highlight the row, but only the
        // focused cell within the row gets the bright capture/learn
        // indicator.
        TColor desc_fg = is_cur_row ? COLORS.EditBG  : COLORS.Text;
        TColor desc_bg = is_cur_row ? COLORS.Highlight : COLORS.Background;
        char desc_buf[KB_DESC_WIDTH + 1];
        snprintf(desc_buf, sizeof(desc_buf), "%-*.*s",
                 KB_DESC_WIDTH, KB_DESC_WIDTH,
                 KeyBindings::actionDescription((ZtAction)action_idx));
        printBG(col(KB_LIST_X + KB_DESC_COL), y, desc_buf, desc_fg, desc_bg, S);

        // Keyboard column.
        const KeyBinding &b = g_keybindings.bindings[action_idx];
        char combo[64];
        if (b.key == 0) {
            snprintf(combo, sizeof(combo), "(unbound)");
        } else {
            KeyBindings::formatKeyCombo(b.key, b.modifiers, combo, sizeof(combo));
        }
        bool kb_focused = is_cur_row && cursor_x == 0;
        TColor kb_fg = kb_focused
            ? (capturing ? COLORS.Text : COLORS.EditBG)
            : COLORS.Text;
        TColor kb_bg = kb_focused
            ? (capturing ? COLORS.EditText : COLORS.Highlight)
            : COLORS.Background;
        char kb_buf[KB_KEY_WIDTH + 1];
        snprintf(kb_buf, sizeof(kb_buf), "%-*.*s",
                 KB_KEY_WIDTH, KB_KEY_WIDTH, combo);
        printBG(col(KB_LIST_X + KB_KEY_COL), y, kb_buf, kb_fg, kb_bg, S);

        // MIDI slot columns.
        for (int s = 0; s < ZT_MM_SLOTS_PER_ACTION; s++) {
            int col_x = 1 + s;
            bool slot_focused = is_cur_row && cursor_x == col_x;
            bool slot_learning = (action_idx == learn_a && s == learn_s);
            char slot_buf[KB_MIDI_WIDTH + 1];
            char raw[8];
            zt_mm_format_binding(&mm->bindings[action_idx][s], raw, sizeof(raw));
            snprintf(slot_buf, sizeof(slot_buf), "%-*.*s",
                     KB_MIDI_WIDTH, KB_MIDI_WIDTH, raw);
            TColor slot_fg, slot_bg;
            if (slot_learning) {
                slot_fg = COLORS.Text;
                slot_bg = COLORS.EditText;
            } else if (slot_focused) {
                slot_fg = COLORS.EditBG;
                slot_bg = COLORS.Highlight;
            } else if (mm->bindings[action_idx][s].valid) {
                slot_fg = COLORS.Text;
                slot_bg = COLORS.Background;
            } else {
                slot_fg = COLORS.Lowlight;
                slot_bg = COLORS.Background;
            }
            int x_px = col(KB_LIST_X + KB_MIDI_COL_0 + s * (KB_MIDI_WIDTH + 1));
            printBG(x_px, y, slot_buf, slot_fg, slot_bg, S);
        }
    }

    // Status line just above the toolbar reservation.
    int total_rows = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int status_y = total_rows - 8 - 1;
    if (status_y < KB_LIST_Y + rows + 1) status_y = KB_LIST_Y + rows + 1;
    S->fillRect(col(1), row(status_y),
                INTERNAL_RESOLUTION_X - 2, row(status_y + 1),
                COLORS.Background);
    print(col(2), row(status_y), status_line,
          (capturing || learn_a != ZT_ACTION_NONE) ? COLORS.Highlight : COLORS.Text, S);

    S->unlock();
    screenmanager.UpdateAll();
}
