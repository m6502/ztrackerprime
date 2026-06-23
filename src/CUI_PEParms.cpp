#include "zt.h"

CUI_PEParms::CUI_PEParms(void) {

    UI = new UserInterface;

    

    int window_width = 54 * col(1);
    int window_height = 34 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--)
        ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--)
        ;
    
    /* Initialize BPM Slider */
    vs_step = new ValueSlider;
    UI->add_element(vs_step,0);
    vs_step->frame = 1;
    vs_step->x = (start_x / 8) + 17;
    vs_step->y = (start_y / 8) + 6;
    vs_step->xsize=window_width/8 - 23;
    vs_step->min = 0;
    vs_step->max = 32;
    vs_step->value = cur_step;

        vs_pat_length = new ValueSlider;
        UI->add_element(vs_pat_length,1);
        vs_pat_length->frame = 1;
        vs_pat_length->x = (start_x / 8) + 17;
        vs_pat_length->y = (start_y / 8) + 8; 
        vs_pat_length->xsize=window_width/8 - 23;
        vs_pat_length->min = 32;
        vs_pat_length->max = 999;
        vs_pat_length->value = song->patterns[cur_edit_pattern]->length;

        vs_highlight = new ValueSlider;
        UI->add_element(vs_highlight,2);
        vs_highlight->frame = 1;
        vs_highlight->x = (start_x / 8) + 17;
        vs_highlight->y = (start_y / 8) + 10; 
        vs_highlight->xsize=window_width/8 - 23;
        vs_highlight->min = 1;
        vs_highlight->max = 32;
        vs_highlight->value = zt_config_globals.highlight_increment;

        vs_lowlight = new ValueSlider;
        UI->add_element(vs_lowlight,3);
        vs_lowlight->frame = 1;
        vs_lowlight->x = (start_x / 8) + 17;
        vs_lowlight->y = (start_y / 8) + 12; 
        vs_lowlight->xsize=window_width/8 - 23;
        vs_lowlight->min = 1;
        vs_lowlight->max = 32;
        vs_lowlight->value = zt_config_globals.lowlight_increment;

        cb_centered = new CheckBox;
        UI->add_element(cb_centered,4);
        cb_centered->frame = 0;
        cb_centered->x = (start_x / 8) + 17;
        cb_centered->y = (start_y / 8) + 14;
        cb_centered->xsize = 3;
        cb_centered->value = &zt_config_globals.centered_editing;
        cb_centered->frame = 1;

        cb_stepedit = new CheckBox;
        UI->add_element(cb_stepedit,5);
        cb_stepedit->frame = 0;
        cb_stepedit->x = (start_x / 8) + 17 + 16;
        cb_stepedit->y = (start_y / 8) + 14;
        cb_stepedit->xsize = 3;
        cb_stepedit->value = &zt_config_globals.step_editing;
        cb_stepedit->frame = 1;

        cb_recveloc = new CheckBox;
        UI->add_element(cb_recveloc,6);
        cb_recveloc->frame = 0;
        cb_recveloc->x = (start_x / 8) + 17 + 32;
        cb_recveloc->y = (start_y / 8) + 14;
        cb_recveloc->xsize = 3;
        cb_recveloc->value = &zt_config_globals.record_velocity;
        cb_recveloc->frame = 1;

        vs_speedup = new ValueSlider;
        UI->add_element(vs_speedup,7);
        vs_speedup->frame = 1;
        vs_speedup->x = (start_x / 8) + 17;
        vs_speedup->y = (start_y / 8) + 16;
        vs_speedup->xsize=window_width/8 - 23;
        vs_speedup->min=1;
        vs_speedup->max = 32;
        vs_speedup->value = zt_config_globals.control_navigation_amount;

        // Effect-draw mode toggle (alternative to Shift+§ which is awkward
        // on non-US keyboards). Synced with UIP_Patterneditor->mode in enter().
        drawmode_val = 0;
        cb_drawmode = new CheckBox;
        UI->add_element(cb_drawmode, 8);
        cb_drawmode->frame = 1;
        cb_drawmode->x = (start_x / 8) + 17;
        cb_drawmode->y = (start_y / 8) + 18;
        cb_drawmode->xsize = 3;
        cb_drawmode->value = &drawmode_val;

        // Overwrite Previous Drawbars (MD_CC_DRAW protection toggle).
        // Default value comes from zt.conf via zt_config_globals.
        cb_cc_draw_overwrite = new CheckBox;
        UI->add_element(cb_cc_draw_overwrite, 9);
        cb_cc_draw_overwrite->frame = 1;
        cb_cc_draw_overwrite->x = (start_x / 8) + 17 + 16;
        cb_cc_draw_overwrite->y = (start_y / 8) + 18;
        cb_cc_draw_overwrite->xsize = 3;
        cb_cc_draw_overwrite->value = &zt_config_globals.cc_draw_overwrite;

        // Keyjazz piano layout (Ableton Live / Logic). OFF = tracker layout.
        cb_keyjazz_piano = new CheckBox;
        UI->add_element(cb_keyjazz_piano, 10);
        cb_keyjazz_piano->frame = 1;
        cb_keyjazz_piano->x = (start_x / 8) + 17 + 32;
        cb_keyjazz_piano->y = (start_y / 8) + 18;
        cb_keyjazz_piano->xsize = 3;
        cb_keyjazz_piano->value = &zt_config_globals.keyjazz_piano_layout;

        // ---- Track Options section (folded in from the old popup) ----------
        use_color = 0;

        ti_name = new TextInput;
        UI->add_element(ti_name, 11);
        ti_name->frame = 1;
        ti_name->x = (start_x / 8) + 17;
        ti_name->y = (start_y / 8) + 23;
        ti_name->xsize = ZTM_TRACKNAME_MAXLEN - 1;
        ti_name->length = ZTM_TRACKNAME_MAXLEN - 1;
        ti_name->str = (unsigned char *)&song->track_name[cur_edit_track][0];

        cb_color = new CheckBox;
        UI->add_element(cb_color, 12);
        cb_color->frame = 1;
        cb_color->x = (start_x / 8) + 17;
        cb_color->y = (start_y / 8) + 25;
        cb_color->xsize = 3;
        cb_color->value = &use_color;

        vs_r = new ValueSlider;
        UI->add_element(vs_r, 13);
        vs_r->frame = 1;
        vs_r->x = (start_x / 8) + 17;
        vs_r->y = (start_y / 8) + 27;
        vs_r->xsize = window_width / 8 - 23;
        vs_r->min = 0; vs_r->max = 255; vs_r->value = 208;

        vs_g = new ValueSlider;
        UI->add_element(vs_g, 14);
        vs_g->frame = 1;
        vs_g->x = (start_x / 8) + 17;
        vs_g->y = (start_y / 8) + 29;
        vs_g->xsize = window_width / 8 - 23;
        vs_g->min = 0; vs_g->max = 255; vs_g->value = 48;

        vs_b = new ValueSlider;
        UI->add_element(vs_b, 15);
        vs_b->frame = 1;
        vs_b->x = (start_x / 8) + 17;
        vs_b->y = (start_y / 8) + 31;
        vs_b->xsize = window_width / 8 - 23;
        vs_b->min = 0; vs_b->max = 255; vs_b->value = 48;
}

void CUI_PEParms::apply_color(void) {
    if (use_color) {
        unsigned int r = (unsigned int)((ValueSlider *)UI->get_element(13))->value;
        unsigned int g = (unsigned int)((ValueSlider *)UI->get_element(14))->value;
        unsigned int b = (unsigned int)((ValueSlider *)UI->get_element(15))->value;
        song->track_color[cur_edit_track] =
            0xFF000000u | (r << 16) | (g << 8) | b;
    } else {
        song->track_color[cur_edit_track] = 0;
    }
}

void CUI_PEParms::enter(void) {
    ValueSlider *vs;
	CheckBox *cb;

    need_refresh = 1;
    vs = (ValueSlider *)UI->get_element(0);
    vs->value = cur_step;
    cur_state = STATE_PEDIT_WIN;
    vs = (ValueSlider *)UI->get_element(1);
    vs->value = song->patterns[cur_edit_pattern]->length;
    vs = (ValueSlider *)UI->get_element(2);
    vs->value = zt_config_globals.highlight_increment;
    vs = (ValueSlider *)UI->get_element(3);
    vs->value = zt_config_globals.lowlight_increment;
	cb = (CheckBox *)UI->get_element(4);
	cb->value = &zt_config_globals.centered_editing;
	cb = (CheckBox *)UI->get_element(5);
	cb->value = &zt_config_globals.step_editing;
	cb = (CheckBox *)UI->get_element(6);
	cb->value = &zt_config_globals.record_velocity;
    vs = (ValueSlider *)UI->get_element(7);
    vs->value = zt_config_globals.control_navigation_amount;
    drawmode_val = (UIP_Patterneditor->mode == PEM_MOUSEDRAW) ? 1 : 0;
    cb = (CheckBox *)UI->get_element(8);
    cb->value = &drawmode_val;
    cb = (CheckBox *)UI->get_element(9);
    cb->value = &zt_config_globals.cc_draw_overwrite;
    cb = (CheckBox *)UI->get_element(10);
    cb->value = &zt_config_globals.keyjazz_piano_layout;

    // Track Options section -- bind to the current track each time the popup
    // opens (cur_edit_track moves with the pattern-editor cursor).
    ti_name = (TextInput *)UI->get_element(11);
    if (!song->track_name[cur_edit_track][0]) {
        snprintf(song->track_name[cur_edit_track], ZTM_TRACKNAME_MAXLEN, "Track %.2d", cur_edit_track + 1);
    }
    ti_name->str = (unsigned char *)&song->track_name[cur_edit_track][0];
    ti_name->cursor = (int)strnlen(song->track_name[cur_edit_track], ZTM_TRACKNAME_MAXLEN - 1);

    unsigned long tc = song->track_color[cur_edit_track];
    use_color = tc ? 1 : 0;
    if (tc) {
        ((ValueSlider *)UI->get_element(13))->value = (tc >> 16) & 0xFF;
        ((ValueSlider *)UI->get_element(14))->value = (tc >>  8) & 0xFF;
        ((ValueSlider *)UI->get_element(15))->value = (tc      ) & 0xFF;
    }
    cb_color = (CheckBox *)UI->get_element(12);
    cb_color->value = &use_color;
}

void CUI_PEParms::leave(void) {
    apply_color();
    cur_state = STATE_PEDIT;
}

void CUI_PEParms::update() {
    int key=0;
    ValueSlider *vs;
	CheckBox *cb;

    UI->update();
    vs = (ValueSlider *)UI->get_element(0);
    if (vs->changed)
        cur_step = vs->value;
    if (Keys.size()) {
        key = Keys.getkey();
        // F2 / Esc / right-click close the combined dialog (F2 toggles it,
        // since F2 from the Pattern Editor opens it). Enter also applies the
        // edited pattern length.
        if (key == SDLK_F2 || key == SDLK_ESCAPE || (key == SDLK_RETURN) || key==((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT))) {
            if (key == SDLK_RETURN) {
                vs = (ValueSlider *)UI->get_element(1);
                song->patterns[cur_edit_pattern]->resize(vs->value);
            }
            apply_color();
            close_popup_window();
            fixmouse++;
            need_refresh++;
        }
    }
    vs = (ValueSlider *)UI->get_element(2);
    if(vs->changed)
        zt_config_globals.highlight_increment = vs->value;
    vs = (ValueSlider *)UI->get_element(3);
    if(vs->changed)
        zt_config_globals.lowlight_increment = vs->value;
    cb = (CheckBox *)UI->get_element(4);
    if(cb->changed)
        zt_config_globals.centered_editing = *(cb->value);
    cb = (CheckBox *)UI->get_element(5);
    if(cb->changed)
        zt_config_globals.step_editing = *(cb->value);
    cb = (CheckBox *)UI->get_element(6);
    if(cb->changed)
        zt_config_globals.record_velocity = *(cb->value);

    vs = (ValueSlider *)UI->get_element(7);
    if(vs->changed)
        zt_config_globals.control_navigation_amount = vs->value;

    cb = (CheckBox *)UI->get_element(8);
    if (cb->changed) {
        UIP_Patterneditor->mode = (drawmode_val) ? PEM_MOUSEDRAW : PEM_REGULARKEYS;
        if (UIP_Patterneditor->mode == PEM_REGULARKEYS) midiInQueue.clear();
    }

    cb = (CheckBox *)UI->get_element(10);
    if (cb->changed)
        zt_config_globals.keyjazz_piano_layout = *(cb->value);

    // Track Options: live-apply the custom colour as the checkbox / RGB
    // sliders are tweaked, so the pattern editor updates behind the popup.
    {
        CheckBox    *cbc = (CheckBox *)UI->get_element(12);
        ValueSlider *r   = (ValueSlider *)UI->get_element(13);
        ValueSlider *g   = (ValueSlider *)UI->get_element(14);
        ValueSlider *b   = (ValueSlider *)UI->get_element(15);
        if (cbc->changed || r->changed || g->changed || b->changed) {
            apply_color();
            need_refresh++;
            need_popup_refresh++;
        }
    }

    // Live drawing while the popup is open. With DrawMode on, forward
    // mouse activity to the pattern editor when the cursor is outside
    // the popup window. Only forward if the queue is empty (so the PE
    // can refresh its drag from LastX/LastY) or the next pending key
    // is a mouse-button event — never a keyboard key, which would
    // hijack the popup's own input.
    if (UIP_Patterneditor->mode == PEM_MOUSEDRAW) {
        int win_w = 54 * col(1);
        int win_h = 34 * row(1);
        int wx = (INTERNAL_RESOLUTION_X / 2) - (win_w / 2);
        int wy = (INTERNAL_RESOLUTION_Y / 2) - (win_h / 2);
        int outside_popup = (LastX < wx) || (LastX >= wx + win_w) ||
                            (LastY < wy) || (LastY >= wy + win_h);
        if (outside_popup) {
            int peeked = Keys.checkkey();
            int forwardable_key =
                peeked == 0 ||
                peeked == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP   << 8) | SDL_BUTTON_LEFT)) ||
                peeked == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT));
            if (forwardable_key) {
                UIP_Patterneditor->update();
                need_refresh++;
                need_popup_refresh++;
            }
        }
    }
}

void CUI_PEParms::draw(Drawable *S) {

    int window_width = 54 * col(1);
    int window_height = 34 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--)
        ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--)
        ;

    vs_step->x = (start_x / 8) + 17;
    vs_step->y = (start_y / 8) + 6; 
    vs_pat_length->x = (start_x / 8) + 17;
    vs_pat_length->y = (start_y / 8) + 8; 
    vs_highlight->x = (start_x / 8) + 17;
    vs_highlight->y = (start_y / 8) + 10; 
    vs_lowlight->x = (start_x / 8) + 17;
    vs_lowlight->y = (start_y / 8) + 12; 
    cb_centered->x = (start_x / 8) + 17;
    cb_centered->y = (start_y / 8) + 14;
    cb_stepedit->x = (start_x / 8) + 17 + 16;
    cb_stepedit->y = (start_y / 8) + 14;
    cb_recveloc->x = (start_x / 8) + 17 + 32;
    cb_recveloc->y = (start_y / 8) + 14;
    vs_speedup->x = (start_x / 8) + 17;
    vs_speedup->y = (start_y / 8) + 16;
    cb_drawmode->x = (start_x / 8) + 17;
    cb_drawmode->y = (start_y / 8) + 18;
    cb_cc_draw_overwrite->x = (start_x / 8) + 17 + 16;
    cb_cc_draw_overwrite->y = (start_y / 8) + 18;
    cb_keyjazz_piano->x = (start_x / 8) + 17 + 32;
    cb_keyjazz_piano->y = (start_y / 8) + 18;

    ti_name->x = (start_x / 8) + 17;
    ti_name->y = (start_y / 8) + 23;
    cb_color->x = (start_x / 8) + 17;
    cb_color->y = (start_y / 8) + 25;
    vs_r->x = (start_x / 8) + 17;
    vs_r->y = (start_y / 8) + 27;
    vs_g->x = (start_x / 8) + 17;
    vs_g->y = (start_y / 8) + 29;
    vs_b->x = (start_x / 8) + 17;
    vs_b->y = (start_y / 8) + 31;


    if (S->lock()==0) {
        S->fillRect(start_x,start_y,start_x + window_width,start_y + window_height,COLORS.Background);
        printline(start_x,start_y + window_height - row(1) + 1,148,window_width / 8, COLORS.Lowlight,S);
        for (int i = start_y / row(1); i < (start_y + window_height) / row(1);i++) {
            printchar(start_x,row(i),145,COLORS.Highlight,S);
            printchar(start_x + window_width - row(1) + 1,row(i),146,COLORS.Lowlight,S);
        }
        printline(start_x,start_y,143,window_width / 8,COLORS.Highlight,S);
        print(col(textcenter("Pattern & Track Options")),start_y + row(2),"Pattern & Track Options",COLORS.Text,S);
        // Labels right-align so the colon lands at col 15. The slider/
        // checkbox x-origin is col 17, so the frame border at col 16
        // sits between the colon and the chip without stomping either.
        print(start_x + col(2),start_y + row(6),     "     EditStep:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(8),     "   Pat length:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(10),    "Row Highlight:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(12),    " Row Lowlight:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(14),    "     Centered:",COLORS.Text,S);
        print(start_x + col(23),start_y + row(14),"StepEdit:",COLORS.Text,S);
        print(start_x + col(39),start_y + row(14),"RecVeloc:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(16),    "      Speedup:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(18),    "     DrawMode:",COLORS.Text,S);
        // CC drawbar overwrite-protect toggle. Label width matches the
        // StepEdit / RecVeloc pattern on row 14.
        print(start_x + col(23),start_y + row(18),"CCOver:",COLORS.Text,S);
        // Keyjazz piano-layout (Ableton/Logic) toggle, mirrors RecVeloc's
        // column on row 14. Chip at col 49, label colon lands at col 47.
        print(start_x + col(39),start_y + row(18),"PianoKey:",COLORS.Text,S);

        // ---- Track Options section -----------------------------------------
        // The centred header alone separates the two halves (no divider line;
        // window is screen-centred so textcenter() lands it inside the popup).
        print(col(textcenter("Track Options")), start_y + row(21), "Track Options", COLORS.Text, S);
        print(start_x + col(2), start_y + row(23), "   Track Name:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(25), " Custom Color:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(27), "          Red:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(29), "        Green:", COLORS.Text, S);
        print(start_x + col(2), start_y + row(31), "         Blue:", COLORS.Text, S);

        // Colour swatch next to the Custom Color toggle -- only when a custom
        // colour is enabled, so disabling it doesn't leave a black void.
        if (use_color) {
            unsigned long sw = 0xFF000000u | (((unsigned long)vs_r->value) << 16)
                                           | (((unsigned long)vs_g->value) << 8)
                                           |  ((unsigned long)vs_b->value);
            int sx = start_x + col(23);
            int sy = start_y + row(25) + 1;
            int ex = start_x + col(33);
            int ey = sy + row(1) - 3;
            S->fillRect(sx, sy, ex, ey, sw);
        }

        UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = need_popup_refresh = 0;
        updated++;
    }
}

