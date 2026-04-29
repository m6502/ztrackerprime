#include "zt.h"
#include "midi_mappings.h"

#include <stdio.h>
#include <string.h>

CUI_MidiMappings::CUI_MidiMappings() {
    UI = new UserInterface;
    cursor_y = 0;
    cursor_x = 0;
    status_line[0] = '\0';
}

void CUI_MidiMappings::enter(void) {
    cur_state = STATE_MIDI_MAPPINGS;
    need_refresh = 1;
    Keys.flush();
    // Cancel any stale Learn target so the page opens in browse mode.
    zt_mm_set_learn_target(-1, -1);
    snprintf(status_line, sizeof(status_line),
             "Enter = Learn this slot   Backspace/Delete = Clear   ESC = Back");
    statusmsg = status_line;
    status_change = 1;
}

void CUI_MidiMappings::leave(void) {
    // Drop Learn so a stray MIDI message after exiting does not get
    // consumed into a stale slot.
    zt_mm_set_learn_target(-1, -1);
}

void CUI_MidiMappings::update(void) {
    int learn_a = zt_mm_get_learn_action();
    int learn_s = zt_mm_get_learn_slot();
    if (learn_a >= 0) {
        // Re-render the status line every frame while in Learn mode
        // so the user sees the current target. The MIDI thread will
        // capture the next short channel message and clear the
        // target via zt_mm_dispatch.
        snprintf(status_line, sizeof(status_line),
                 "LEARN: send a MIDI key/CC for %s slot %d   (ESC to cancel)",
                 zt_mm_action_display_name(learn_a),
                 learn_s + 1);
        statusmsg = status_line;
        status_change = 1;
    }

    if (!Keys.size()) return;
    int key = Keys.checkkey();
    int kstate = Keys.getstate();
    (void)kstate;

    switch (key) {
        case SDLK_ESCAPE:
            Keys.getkey();
            if (learn_a >= 0) {
                zt_mm_set_learn_target(-1, -1);
                snprintf(status_line, sizeof(status_line),
                         "Learn cancelled");
                statusmsg = status_line;
                status_change = 1;
                need_refresh++;
            } else {
                switch_page(UIP_Patterneditor);
            }
            return;

        case SDLK_UP:
            Keys.getkey();
            if (cursor_y > 0) cursor_y--;
            need_refresh++;
            return;
        case SDLK_DOWN:
            Keys.getkey();
            if (cursor_y < ZT_MM_NUM_ACTIONS - 1) cursor_y++;
            need_refresh++;
            return;
        case SDLK_LEFT:
            Keys.getkey();
            if (cursor_x > 0) cursor_x--;
            need_refresh++;
            return;
        case SDLK_RIGHT:
            Keys.getkey();
            if (cursor_x < ZT_MM_SLOTS_PER_ACTION - 1) cursor_x++;
            need_refresh++;
            return;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            Keys.getkey();
            zt_mm_set_learn_target(cursor_y, cursor_x);
            snprintf(status_line, sizeof(status_line),
                     "LEARN: send a MIDI key/CC for %s slot %d   (ESC to cancel)",
                     zt_mm_action_display_name(cursor_y),
                     cursor_x + 1);
            statusmsg = status_line;
            status_change = 1;
            need_refresh++;
            return;

        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            Keys.getkey();
            zt_mm_clear_slot(cursor_y, cursor_x);
            snprintf(status_line, sizeof(status_line),
                     "Cleared %s slot %d",
                     zt_mm_action_display_name(cursor_y),
                     cursor_x + 1);
            statusmsg = status_line;
            status_change = 1;
            need_refresh++;
            return;

        default:
            // Drop unrecognised keys so they do not leak into the
            // page below.
            Keys.getkey();
            return;
    }
}

void CUI_MidiMappings::draw(Drawable *S) {
    if (S->lock() != 0) return;

    UI->draw(S);
    draw_status(S);
    printtitle(PAGE_TITLE_ROW_Y, "MIDI Mappings (Shift+F3)",
               COLORS.Text, COLORS.Background, S);

    const int header_y = TRACKS_ROW_Y;
    const int x_action = 2;
    const int x_slots[ZT_MM_SLOTS_PER_ACTION] = { 28, 38, 48 };

    // Column headers.
    print(col(x_action), row(header_y), "Action", COLORS.Highlight, S);
    for (int s = 0; s < ZT_MM_SLOTS_PER_ACTION; s++) {
        char header[16];
        snprintf(header, sizeof(header), "Slot %d", s + 1);
        print(col(x_slots[s]), row(header_y), header,
              COLORS.Highlight, S);
    }

    // One row per action.
    ZtMmMappings *mm = zt_mm_get_mappings();
    int learn_a = zt_mm_get_learn_action();
    int learn_s = zt_mm_get_learn_slot();
    for (int a = 0; a < ZT_MM_NUM_ACTIONS; a++) {
        const int row_y = header_y + 2 + a;
        TColor name_col = (a == cursor_y) ? COLORS.Highlight : COLORS.Text;
        print(col(x_action), row(row_y),
              zt_mm_action_display_name(a), name_col, S);

        for (int s = 0; s < ZT_MM_SLOTS_PER_ACTION; s++) {
            char buf[8];
            zt_mm_format_binding(&mm->bindings[a][s], buf, sizeof(buf));
            TColor slot_col;
            if (a == learn_a && s == learn_s) {
                slot_col = COLORS.Highlight;   // capturing
            } else if (a == cursor_y && s == cursor_x) {
                slot_col = COLORS.Highlight;   // focused
            } else if (mm->bindings[a][s].valid) {
                slot_col = COLORS.Text;
            } else {
                slot_col = COLORS.Lowlight;
            }
            print(col(x_slots[s]), row(row_y), buf, slot_col, S);
        }
    }

    // Helpful legend below the grid.
    const int legend_y = header_y + 4 + ZT_MM_NUM_ACTIONS;
    print(col(x_action), row(legend_y),
          "Up/Down = action   Left/Right = slot",
          COLORS.Text, S);
    print(col(x_action), row(legend_y + 1),
          "Bindings save to zt.conf as midi_map_<action>: SS,DD SS,DD SS,DD",
          COLORS.Lowlight, S);

    need_refresh = 0;
    updated = 2;
    S->unlock();
}
