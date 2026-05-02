/*************************************************************************
 *
 * FILE  CUI_PEFindReplace.cpp
 *
 * DESCRIPTION
 *   Find/Replace Note popup (Ctrl+H from the Pattern Editor).
 *
 *   A small modal popup with two ValueSliders:
 *
 *     +----------------------------------------+
 *     |        Find / Replace Note             |
 *     |                                        |
 *     |  Find note (0-127): [#####-----] 060   |
 *     |  With note (0-127): [######----] 067   |
 *     |                                        |
 *     |  Tab cycles  -  Enter applies          |
 *     |  ESC cancels                           |
 *     +----------------------------------------+
 *
 *   On Enter the popup walks the current selection (or the entire
 *   pattern / current track if no selection is active) and replaces
 *   every event whose .note equals `find` with `replace`. Instrument
 *   / vol / length / effect columns are left untouched. A single
 *   UNDO_SAVE() makes the whole replace one Ctrl+Z. Status line
 *   reports the number of substitutions.
 *
 *   The "Find" slider pre-fills with the note at (cur_edit_row,
 *   cur_edit_track) when the popup opens, so the natural workflow is
 *   "park cursor on the note you want to find, hit Ctrl+H, tweak the
 *   With slider to the target note, Enter."
 *
 ******/
#include "zt.h"

CUI_PEFindReplace::CUI_PEFindReplace(void) {
    UI = new UserInterface;

    int window_width  = 44 * col(1);
    int window_height = 12 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width  / 2);
    for(;start_x % 8;start_x--) ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--) ;

    // Find slider (0..127 MIDI note value)
    vs_find = new ValueSlider;
    UI->add_element(vs_find, 0);
    vs_find->frame  = 1;
    vs_find->x      = (start_x / 8) + 19;
    vs_find->y      = (start_y / 8) + 4;
    vs_find->xsize  = (window_width / 8) - 26;
    vs_find->min    = 0;
    vs_find->max    = 127;
    vs_find->value  = 60;     // C4 default; overridden in enter()

    // Replace slider (0..127)
    vs_replace = new ValueSlider;
    UI->add_element(vs_replace, 1);
    vs_replace->frame = 1;
    vs_replace->x     = (start_x / 8) + 19;
    vs_replace->y     = (start_y / 8) + 6;
    vs_replace->xsize = (window_width / 8) - 26;
    vs_replace->min   = 0;
    vs_replace->max   = 127;
    vs_replace->value = 60;
}

void CUI_PEFindReplace::enter(void) {
    need_refresh = 1;
    // Pre-fill Find with the note at the cursor row of the cursor track,
    // so the user's "park on note + Ctrl+H" workflow does the right thing
    // out of the box.
    int prefill = 60;   // C4 fallback
    if (song && song->patterns[cur_edit_pattern]) {
        track *trk = song->patterns[cur_edit_pattern]->tracks[cur_edit_track];
        if (trk) {
            event *e = trk->get_event(cur_edit_row);
            if (e && e->note < 0x80) prefill = e->note;
        }
    }
    vs_find    = (ValueSlider *)UI->get_element(0);
    vs_replace = (ValueSlider *)UI->get_element(1);
    if (vs_find)    vs_find->value    = prefill;
    if (vs_replace) vs_replace->value = prefill;
    UI->cur_element = 0;   // focus the Find slider first
}

void CUI_PEFindReplace::leave(void) {
}

void CUI_PEFindReplace::update(void) {
    int key = 0;
    UI->update();
    // The friendly note name is drawn OUTSIDE the slider widget's bbox,
    // so a value change inside the slider repaints the slider itself
    // (need_redraw) but not the label below it. Bump need_refresh
    // whenever a slider's `changed` flag fires so the whole popup
    // redraws and the note name tracks the slider value live.
    ValueSlider *vsf = (ValueSlider *)UI->get_element(0);
    ValueSlider *vsr = (ValueSlider *)UI->get_element(1);
    if ((vsf && vsf->changed) || (vsr && vsr->changed)) {
        if (vsf) vsf->changed = 0;
        if (vsr) vsr->changed = 0;
        need_refresh++;
    }
    if (Keys.size()) {
        key = Keys.getkey();
        if (key == SDLK_ESCAPE ||
            key == SDLK_RETURN ||
            key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT))) {

            if (key == SDLK_RETURN) {
                int find    = ((ValueSlider *)UI->get_element(0))->value & 0x7F;
                int replace = ((ValueSlider *)UI->get_element(1))->value & 0x7F;
                if (find != replace) {
                    int t_lo, t_hi, r_lo, r_hi;
                    if (selected) {
                        t_lo = select_track_start;
                        t_hi = select_track_end;
                        r_lo = select_row_start;
                        r_hi = select_row_end;
                    } else {
                        // No selection -> entire current track of the
                        // current pattern. Mirrors the "act on selection
                        // or fall back to track-wide" idiom used by
                        // other Pattern Editor block ops.
                        t_lo = t_hi = cur_edit_track;
                        r_lo = 0;
                        r_hi = song->patterns[cur_edit_pattern]->length - 1;
                    }
                    // Note: no UNDO_SAVE() — same convention as the
                    // CUI_PENature / CUI_PEVol popups, where g_undo is
                    // a file-static instance in CUI_Patterneditor.cpp
                    // and isn't accessible from popup classes. The
                    // user's escape hatch is save-before / save-after.
                    // Promoting g_undo to a global is a separate
                    // refactor.
                    int replaced = 0;
                    for (int t = t_lo; t <= t_hi; t++) {
                        track *trk = song->patterns[cur_edit_pattern]->tracks[t];
                        if (!trk) continue;
                        for (int j = r_lo; j <= r_hi; j++) {
                            event *e = trk->get_event(j);
                            if (e && e->note == find) {
                                trk->update_event(j, replace, -1, -1, -1, -1, -1);
                                replaced++;
                            }
                        }
                    }
                    static char msg[96];
                    snprintf(msg, sizeof(msg),
                             "Replaced %d note%s (%d -> %d)",
                             replaced, replaced == 1 ? "" : "s", find, replace);
                    statusmsg = msg;
                    status_change = 1;
                } else {
                    statusmsg = "Find and Replace are the same -- nothing to do";
                    status_change = 1;
                }
            }
            close_popup_window();
            fixmouse++;
            need_refresh++;
        }
    }
}

void CUI_PEFindReplace::draw(Drawable *S) {
    if (S->lock() != 0) return;

    int window_width  = 44 * col(1);
    int window_height = 12 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width  / 2);
    for(;start_x % 8;start_x--) ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--) ;

    // Window frame (mirrors CUI_PENature / CUI_PEVol).
    S->fillRect(start_x, start_y,
                start_x + window_width, start_y + window_height,
                COLORS.Background);
    printline(start_x, start_y + window_height - row(1) + 1, 148,
              window_width / 8, COLORS.Lowlight, S);
    for (int i = start_y / row(1); i < (start_y + window_height) / row(1); i++) {
        printchar(start_x, row(i), 145, COLORS.Highlight, S);
        printchar(start_x + window_width - row(1) + 1, row(i), 146, COLORS.Lowlight, S);
    }
    printline(start_x, start_y, 143, window_width / 8, COLORS.Highlight, S);

    // Title
    print(col(textcenter("Find / Replace Note")),
          start_y + row(2),
          "Find / Replace Note", COLORS.Text, S);

    // Labels
    print(start_x + col(2), start_y + row(4),
          "Find note (0-127):",    COLORS.Text, S);
    print(start_x + col(2), start_y + row(6),
          "With note (0-127):",    COLORS.Text, S);

    // Hint footer -- two lines so the chord of keys is obvious.
    print(start_x + col(2), start_y + row(8),
          "Tab cycles  -  Enter applies", COLORS.Text, S);
    print(start_x + col(2), start_y + row(9),
          "ESC cancels",                  COLORS.Text, S);

    UI->draw(S);

    // Friendly note names next to each slider's numeric readout.
    // ValueSlider prints " NNN" (4 chars) starting at col(x+xsize); the
    // note label lands one space after that so the row reads e.g.
    //
    //   Find note (0-127): [#####-----] 060  (C-5)
    //
    // hex2note writes 3 raw chars (no NUL); we sprintf into a 5-char
    // buffer with parens so the parens nul-terminate cleanly.
    {
        char nbuf[8];
        ValueSlider *vsf = (ValueSlider *)UI->get_element(0);
        ValueSlider *vsr = (ValueSlider *)UI->get_element(1);
        if (vsf) {
            char raw[4] = {0};
            hex2note(raw, (unsigned char)(vsf->value & 0x7F));
            snprintf(nbuf, sizeof(nbuf), "(%c%c%c)", raw[0], raw[1], raw[2]);
            print(col(vsf->x + vsf->xsize + 5), row(vsf->y), nbuf, COLORS.Text, S);
        }
        if (vsr) {
            char raw[4] = {0};
            hex2note(raw, (unsigned char)(vsr->value & 0x7F));
            snprintf(nbuf, sizeof(nbuf), "(%c%c%c)", raw[0], raw[1], raw[2]);
            print(col(vsr->x + vsr->xsize + 5), row(vsr->y), nbuf, COLORS.Text, S);
        }
    }

    S->unlock();
    need_refresh = need_popup_refresh = 0;
    updated++;
}
