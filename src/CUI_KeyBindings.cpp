// CUI_KeyBindings — In-app editor for remappable keyboard shortcuts.
//
// Paired with src/keybindings.{h,cpp}, which owns the storage and the
// load/save-to-zt.conf logic. This file only provides the UI: a scrollable
// list of every ZtAction with its current binding, a capture mode to record
// a new key combo, and commands to save / reset / unbind.
//
// Access from anywhere via Ctrl+Alt+K (wired in main.cpp). Key handling is
// custom (the UserInterface framework doesn't help when you need to observe
// arbitrary modifier+key combos without the page reacting to them first).

#include "zt.h"
#include "keybindings.h"
#include "conf.h"

#include <SDL.h>
#include <string.h>
#include <stdio.h>

// Layout constants. The list spans from the title down to the toolbar clearance
// established by the rest of the UI (row 51 bottom at 480px).
#define KB_LIST_X        2
#define KB_LIST_Y        11
#define KB_ACTION_COL    4      // column offset inside the list row for action name
#define KB_ACTION_WIDTH  30     // max chars of action name displayed
#define KB_BINDING_COL   (KB_ACTION_COL + KB_ACTION_WIDTH + 2)
#define KB_BINDING_WIDTH 28

CUI_KeyBindings::CUI_KeyBindings(void)
{
    UI = NULL;      // This page bypasses the UserInterface framework.
    cursor_y = 1;   // Skip ZT_ACTION_NONE at index 0.
    list_start = 0;
    capturing = 0;
    dirty = 0;
    status_line[0] = '\0';
}

int CUI_KeyBindings::visible_rows(void) const
{
    // Leave room for title at row KB_LIST_Y - 2 and a status line at the
    // bottom. SPACE_AT_BOTTOM=8 rows are already reserved for the toolbar;
    // also reserve 2 more rows at the bottom for the status/help text.
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
    snprintf(status_line, sizeof(status_line),
             "UP/DOWN select  ENTER capture  DEL unbind  Ctrl+S save  Ctrl+R reset  Ctrl+Shift+R reset-all  ESC exit");
}

void CUI_KeyBindings::leave(void)
{
}

void CUI_KeyBindings::update(void)
{
    if (!Keys.size()) return;

    KBMod kstate = Keys.getstate();
    KBKey key = Keys.getkey();

    // ----- Capture mode: the next real (non-modifier) key becomes the binding.
    if (capturing) {
        // Ignore bare modifier-only presses; wait for an actual key.
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
                     "Bound %s -> %s (Ctrl+S to save to zt.conf)",
                     KeyBindings::actionName((ZtAction)cursor_y), combo);
        }
        capturing = 0;
        need_refresh++;
        return;
    }

    // ----- Browse mode.
    int rows = visible_rows();
    int total = ZT_ACTION_COUNT - 1;  // exclude ZT_ACTION_NONE

    // Global exits always work (ESC = back to pattern editor).
    if (key == SDLK_ESCAPE) {
        switch_page(UIP_Patterneditor);
        return;
    }

    // Ctrl+S: save all bindings to zt.conf.
    if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_S) {
        g_keybindings.save(zt_config_globals.Config);
        zt_config_globals.save();
        dirty = 0;
        snprintf(status_line, sizeof(status_line), "Saved to zt.conf.");
        need_refresh++;
        return;
    }

    // Ctrl+Shift+R: reset ALL bindings to built-in defaults.
    if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_R) {
        g_keybindings.setDefaults();
        dirty = 1;
        snprintf(status_line, sizeof(status_line),
                 "All bindings reset to defaults. Ctrl+S to save.");
        need_refresh++;
        return;
    }

    // Ctrl+R: reset the current row's binding to default.
    if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_R) {
        // Need defaults to restore a single entry. Easiest: snapshot current,
        // call setDefaults to get the default for this row, restore the rest.
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
                 "Reset %s to default (%s). Ctrl+S to save.",
                 KeyBindings::actionName((ZtAction)cursor_y), combo);
        need_refresh++;
        return;
    }

    // DEL / BACKSPACE: unbind current row.
    if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
        if (cursor_y > 0 && cursor_y < ZT_ACTION_COUNT) {
            g_keybindings.bindings[cursor_y].key = 0;
            g_keybindings.bindings[cursor_y].modifiers = 0;
            dirty = 1;
            snprintf(status_line, sizeof(status_line),
                     "Unbound %s. Ctrl+S to save.",
                     KeyBindings::actionName((ZtAction)cursor_y));
        }
        need_refresh++;
        return;
    }

    // RETURN / SPACE: enter capture mode.
    if (key == SDLK_RETURN || key == SDLK_SPACE) {
        capturing = 1;
        Keys.flush();   // drop any key-repeat so the first intentional press is caught
        snprintf(status_line, sizeof(status_line),
                 "Press new key combo for '%s' (ESC to cancel)...",
                 KeyBindings::actionName((ZtAction)cursor_y));
        need_refresh++;
        return;
    }

    // Navigation.
    auto clamp_scroll = [&]() {
        if (cursor_y < 1) cursor_y = 1;
        if (cursor_y > total) cursor_y = total;
        if (cursor_y - list_start < 0) list_start = cursor_y;
        if (cursor_y - list_start >= rows) list_start = cursor_y - rows + 1;
        if (list_start < 0) list_start = 0;
        if (list_start > total - rows + 1) list_start = total - rows + 1;
        if (list_start < 0) list_start = 0;
    };

    switch (key) {
        case SDLK_UP:       cursor_y--;           break;
        case SDLK_DOWN:     cursor_y++;           break;
        case SDLK_PAGEUP:   cursor_y -= 10;       break;
        case SDLK_PAGEDOWN: cursor_y += 10;       break;
        case SDLK_HOME:     cursor_y = 1;         break;
        case SDLK_END:      cursor_y = total;     break;
        default: break;
    }
    clamp_scroll();
    need_refresh++;
}

void CUI_KeyBindings::draw(Drawable *S)
{
    if (S->lock() != 0) return;

    // Background under the list area so stale content from other pages
    // doesn't bleed through when returning here.
    int rows = visible_rows();
    S->fillRect(col(1),
                row(KB_LIST_Y - 1),
                INTERNAL_RESOLUTION_X - 2,
                row(KB_LIST_Y + rows + 2),
                COLORS.Background);

    char buf[96];
    snprintf(buf, sizeof(buf), "Keybindings Editor (Ctrl+Alt+K)%s",
             dirty ? "   [unsaved]" : "");
    printtitle(PAGE_TITLE_ROW_Y, buf, COLORS.Text, COLORS.Background, S);

    // Column headers.
    print(col(KB_LIST_X + KB_ACTION_COL), row(KB_LIST_Y - 1),
          "Action", COLORS.Brighttext, S);
    print(col(KB_LIST_X + KB_BINDING_COL), row(KB_LIST_Y - 1),
          "Binding", COLORS.Brighttext, S);

    int total = ZT_ACTION_COUNT - 1;

    for (int i = 0; i < rows; i++) {
        int action_idx = list_start + i + 1;  // +1 to skip ZT_ACTION_NONE
        if (action_idx > total) break;

        int y = row(KB_LIST_Y + i);
        TColor fg, bg;
        bool is_cur = (action_idx == cursor_y);
        if (is_cur && capturing) {
            fg = COLORS.Text;
            bg = COLORS.EditText;   // "capture" accent
        } else if (is_cur) {
            fg = COLORS.EditBG;
            bg = COLORS.Highlight;
        } else {
            fg = COLORS.Text;
            bg = COLORS.Background;
        }

        // Clear the row and print action name + binding. Pad to fixed widths
        // so the highlight bar is rectangular and column edges line up.
        char action_buf[KB_ACTION_WIDTH + 1];
        snprintf(action_buf, sizeof(action_buf), "%-*.*s",
                 KB_ACTION_WIDTH, KB_ACTION_WIDTH,
                 KeyBindings::actionName((ZtAction)action_idx));

        char binding_buf[KB_BINDING_WIDTH + 1];
        char combo[64];
        const KeyBinding &b = g_keybindings.bindings[action_idx];
        if (b.key == 0) {
            snprintf(combo, sizeof(combo), "(unbound)");
        } else {
            KeyBindings::formatKeyCombo(b.key, b.modifiers, combo, sizeof(combo));
        }
        snprintf(binding_buf, sizeof(binding_buf), "%-*.*s",
                 KB_BINDING_WIDTH, KB_BINDING_WIDTH, combo);

        printBG(col(KB_LIST_X + KB_ACTION_COL), y, action_buf, fg, bg, S);
        printBG(col(KB_LIST_X + KB_BINDING_COL), y, binding_buf, fg, bg, S);
    }

    // Status line just above the toolbar reservation.
    int total_rows = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int status_y = total_rows - 8 - 1;  // one row above SPACE_AT_BOTTOM
    if (status_y < KB_LIST_Y + rows + 1) status_y = KB_LIST_Y + rows + 1;
    // Clear the status strip first.
    S->fillRect(col(1), row(status_y),
                INTERNAL_RESOLUTION_X - 2, row(status_y + 1),
                COLORS.Background);
    print(col(2), row(status_y), status_line,
          capturing ? COLORS.Highlight : COLORS.Text, S);

    S->unlock();
    screenmanager.UpdateAll();
}
