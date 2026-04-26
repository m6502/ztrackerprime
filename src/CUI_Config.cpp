#include "zt.h"
#include "Button.h"

void BTNCLK_SaveSettings(UserInterfaceElement *b) {
    (void)b;
    //Config.save("zt.conf");
    zt_config_globals.save();
    need_refresh++; 
}

void BTNCLK_GotoSystemConfig(UserInterfaceElement *b) {
    (void)b;
    switch_page(UIP_Sysconfig);
    need_refresh++;
    doredraw++;
}


CUI_Config::CUI_Config(void) {
    UI = new UserInterface;
//    CheckBox *cb_full_screen;
//    CheckBox *cb_fade_in_out;
//    CheckBox *cb_Auto_open_MIDI;
//    TextInput *ti_skin_file;
//    TextInput *ti_color_file;
//    TextInput *ti_work_directory;
//    ValueSlider *vs_key_repeat;
//    ValueSlider *vs_key_wait;

    CheckBox *cb;
    TextInput *ti;
    ValueSlider *vs;
    Button *b;
    //TextBox *tb;

    cb = new CheckBox;
    UI->add_element(cb,0);
    cb->frame = 0;
    cb->x = 20;
    cb->y = 14;
    cb->xsize = 5;
    cb->value = &zt_config_globals.autoload_ztfile;
    cb->frame = 1;

    ti = new TextInput;
    UI->add_element(ti,1);
    ti->frame = 1;
    // Sit on the same row as the Autoload .ZT checkbox, to the right of
    // the "Autoload File" label that draws at col 28 (label is 13 cols).
    ti->x = 42;
    ti->y = 14;
    ti->xsize = 30;
    ti->length = 50;
    ti->str = (unsigned char*)zt_config_globals.autoload_ztfile_filename;

    ti = new TextInput;
    UI->add_element(ti,2);
    ti->frame = 1;
    ti->x = 20;
    ti->y = 16;
    ti->xsize = 50;
    ti->length = 50;
    ti->str = (unsigned char*)zt_config_globals.default_directory;

    cb = new CheckBox;
    UI->add_element(cb,3);
    cb->frame = 0;
    cb->x = 20;
    cb->y = 18;
    cb->xsize = 5;
    cb->value = &zt_config_globals.record_velocity;
    cb->frame = 1;

    vs = new ValueSlider;
    UI->add_element(vs,4);
    vs->x = 20;
    vs->y = 19;
    vs->xsize = 15;
    vs->ysize = 1;
    vs->value = zt_config_globals.autosave_interval_seconds;
    vs->min = 0;
    vs->max = 3600;

    vs = new ValueSlider;
    UI->add_element(vs,5);
    vs->x = 20;
    vs->y = 20;
    vs->xsize = 15;
    vs->ysize = 1;
    vs->value = zt_config_globals.cur_edit_mode;
#ifdef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
    vs->min = VIEW_SQUISH;
    vs->max = VIEW_BIG;
#else
    vs->min = VIEW_BIG;
    vs->max = VIEW_BIG;
    vs->xsize = 0;
    vs->ysize = 0;
    vs->no_tab_stop = 1;  // invisible stub; skip it in UP/DOWN cycling
#endif

    vs = new ValueSlider;
    UI->add_element(vs,6);
    // "Row highlight minor" label is 19 chars at col 2, so the slider
    // starts at col 22.
    vs->x = 22;
    vs->y = 21;
    vs->xsize = 13;
    vs->ysize = 1;
    vs->value = zt_config_globals.highlight_increment;
    vs->min = 1;
    vs->max = 64;

    vs = new ValueSlider;
    UI->add_element(vs,7);
    // Pair with the minor-row slider on row 22. Layout:
    //   col 2..20  "Row highlight minor" label
    //   col 22..34 minor slider, col 35..38 readout
    //   col 40..58 "Row highlight major" label
    //   col 60..72 major slider, col 73..76 readout
    vs->x = 60;
    vs->y = 21;
    vs->xsize = 13;
    vs->ysize = 1;
    vs->value = zt_config_globals.lowlight_increment;
    vs->min = 1;
    vs->max = 64;

    vs = new ValueSlider;
    UI->add_element(vs,8);
    vs->x = 20;
    vs->y = 23;
    vs->xsize = 15;
    vs->ysize = 1;
    vs->value = zt_config_globals.pattern_length;
    vs->min = 1;
    vs->max = 256;

    // MIDI realtime sync toggles. The underlying flags already exist in
    // zt.conf but were previously only reachable via the Sysconfig page;
    // expose them here so Global Config (F12) covers the full config set.
    // MIDI In Sync + Chase MIDI Tempo moved to F11 (Songconfig). Keep
    // the elements here as off-screen no-tab-stops so the existing
    // get_element(N) index numbering in update()/draw() doesn't break.
    cb = new CheckBox;
    UI->add_element(cb, 9);
    cb->frame = 0;
    cb->x = 0; cb->y = 0; cb->xsize = 0;
    cb->value = &zt_config_globals.midi_in_sync;
    cb->no_tab_stop = 1;

    cb = new CheckBox;
    UI->add_element(cb, 10);
    cb->frame = 0;
    cb->x = 0; cb->y = 0; cb->xsize = 0;
    cb->value = &zt_config_globals.midi_in_sync_chase_tempo;
    cb->no_tab_stop = 1;

    // Post-Load Page: where to land after a successful song load.
    // Slider with an inline label name (Inst/Pattern/Songconf) drawn
    // by draw() — same idiom as View Mode above.
    vs = new ValueSlider;
    UI->add_element(vs, 11);
    vs->x = 20;
    vs->y = 24;
    vs->xsize = 15;
    vs->ysize = 1;
    vs->value = zt_config_globals.post_load_page;
    vs->min = 0;
    vs->max = POST_LOAD_PAGE_COUNT - 1;

    b = new Button;
    // Tab order: Page-switch button is the LAST element so DOWN-arrow
    // from the bottom of the config list wraps cleanly back to Autoload
    // .ZT (tabindex 0) instead of jumping past the MIDI rows.
    UI->add_element(b,12);
    b->caption = "   Go to page 1   ";   // symmetric with Sysconfig's "Go to page 2" button (same x, y, xsize)
    b->xsize = 18;
    b->x = 2;
    b->y = 12;
    b->ysize = 1;
    b->OnClick = (ActFunc)BTNCLK_GotoSystemConfig;

    /*
    cb = new CheckBox;
    UI->add_element(cb,1);
    cb->frame = 0;
    cb->x = 17;
    cb->y = 13;
    cb->xsize = 5;
    cb->value = &zt_config_globals.do_fade;
*/

/*

    ti = new TextInput;
    UI->add_element(ti,3);
    ti->frame = 1;
    ti->x = 17; 
    ti->y = 17;
    ti->xsize=50;
    ti->length=50;
    ti->str = (unsigned char*)zt_config_globals.skin;
    ti = new TextInput;
    UI->add_element(ti,4);
    ti->frame = 1;
    ti->x = 17;
    ti->y = 19;
    ti->xsize=50;
    ti->length=50;
    ti->str = (unsigned char*)COLORFILE;
*/
/*
    ti = new TextInput;
    UI->add_element(ti,6);
    ti->frame = 1;
    ti->x = 17; 
    ti->y = 23;
    ti->xsize=50;
    ti->length=50;
    ti->str = (unsigned char*)zt_config_globals.work_directory;
    b = new Button;
    UI->add_element(b,7);
    b->caption = " Save instance";
    b->x = 17;
    b->y = 26;
    b->xsize = 15;
    b->ysize = 1;
    b->OnClick = (ActFunc)BTNCLK_SaveSettings; 
*/

    tb = new TextBox;
    UI->add_element(tb, 9);
    tb->no_tab_stop = 1;  // read-only help; swallows UP/DOWN, exclude from focus cycle
    tb->x = 1;
    tb->y = 28;
    tb->xsize = 78;
    {
        const int max_rows = (INTERNAL_RESOLUTION_Y / 8);
        int remain = max_rows - tb->y - 1 - 9;
        if (remain < 1) remain = 1;
        tb->ysize = remain;
    }
    tb->text=NULL;


}

void CUI_Config::enter(void) {
    need_refresh = 1;
    cur_state = STATE_CONFIG;
    Keys.flush();
    // Start with focus on "Go to page 1" so the user can bounce straight
    // back to System Config without tabbing past every field first.
    UI->set_focus(10);
}

void CUI_Config::leave(void) {

}

void CUI_Config::update() {
    UI->update();
    ValueSlider *vs;
    TextInput *ti;

    vs = (ValueSlider *)UI->get_element(4);
    if (vs && vs->value != zt_config_globals.autosave_interval_seconds) {
        zt_config_globals.autosave_interval_seconds = vs->value;
    }

    vs = (ValueSlider *)UI->get_element(5);
#ifdef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
    if (vs && vs->value != zt_config_globals.cur_edit_mode) {
        zt_config_globals.cur_edit_mode = vs->value;
    }
#endif

    vs = (ValueSlider *)UI->get_element(6);
    if (vs && vs->value != zt_config_globals.highlight_increment) {
        zt_config_globals.highlight_increment = vs->value;
    }

    vs = (ValueSlider *)UI->get_element(7);
    if (vs && vs->value != zt_config_globals.lowlight_increment) {
        zt_config_globals.lowlight_increment = vs->value;
    }

    vs = (ValueSlider *)UI->get_element(8);
    if (vs && vs->value != zt_config_globals.pattern_length) {
        zt_config_globals.pattern_length = vs->value;
    }

    vs = (ValueSlider *)UI->get_element(11);
    if (vs && vs->value != zt_config_globals.post_load_page) {
        zt_config_globals.post_load_page = vs->value;
    }

    ti = (TextInput *)UI->get_element(1);
    if (ti && ti->changed) {
        strncpy(zt_config_globals.autoload_ztfile_filename, (char *)ti->str, sizeof(zt_config_globals.autoload_ztfile_filename) - 1);
        zt_config_globals.autoload_ztfile_filename[sizeof(zt_config_globals.autoload_ztfile_filename) - 1] = '\0';
    }

    ti = (TextInput *)UI->get_element(2);
    if (ti && ti->changed) {
        strncpy(zt_config_globals.default_directory, (char *)ti->str, sizeof(zt_config_globals.default_directory) - 1);
        zt_config_globals.default_directory[sizeof(zt_config_globals.default_directory) - 1] = '\0';
    }

    if (Keys.size()) {
        Keys.getkey();
    }

    if (tb) {
        tb->need_redraw = 1;
    }
}

void CUI_Config::draw(Drawable *S) {
    char buf[1024];
    if (S->lock()==0) {
#ifdef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
        const char *view_mode_name = "Regular";
        switch (zt_config_globals.cur_edit_mode) {
            case VIEW_SQUISH:  view_mode_name = "Squish"; break;
            case VIEW_REGULAR: view_mode_name = "Regular"; break;
            case VIEW_FX:      view_mode_name = "FX"; break;
            case VIEW_BIG:     view_mode_name = "Big"; break;
        }
#endif
        sprintf(buf,"|U|Current Settings in memory:");
        sprintf(buf+strlen(buf),"\n|U| Auto-Open MIDI  |L|[|H|%s|L|]",zt_config_globals.auto_open_midi?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Autoload .ZT    |L|[|H|%s|L|]",zt_config_globals.autoload_ztfile?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Autoload File   |L|[|H|%s|L|]",zt_config_globals.autoload_ztfile_filename);
        sprintf(buf+strlen(buf),"\n|U| Default Dir     |L|[|H|%s|L|]",zt_config_globals.default_directory);
        sprintf(buf+strlen(buf),"\n|U| Record Velocity |L|[|H|%s|L|]",zt_config_globals.record_velocity?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Autosave (s)    |L|[|H|%d|L|]",zt_config_globals.autosave_interval_seconds);
#ifdef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
        sprintf(buf+strlen(buf),"\n|U| View Mode       |L|[|H|%s|L|]",view_mode_name);
#endif
        sprintf(buf+strlen(buf),"\n|U| Row hl minor    |L|[|H|%d|L|]",zt_config_globals.highlight_increment);
        sprintf(buf+strlen(buf),"\n|U| Row hl major    |L|[|H|%d|L|]",zt_config_globals.lowlight_increment);
        sprintf(buf+strlen(buf),"\n|U| Pattern Len     |L|[|H|%d|L|]",zt_config_globals.pattern_length);
        sprintf(buf+strlen(buf),"\n|U| Full Screen     |L|[|H|%s|L|]",zt_config_globals.full_screen?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Send Panic      |L|[|H|%s|L|]",zt_config_globals.auto_send_panic?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| MIDI In Sync    |L|[|H|%s|L|]",zt_config_globals.midi_in_sync?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Chase MIDI Tempo|L|[|H|%s|L|]",zt_config_globals.midi_in_sync_chase_tempo?"On":"Off");
        const char *post_load_name = "Inst Editor";
        switch (zt_config_globals.post_load_page) {
            case POST_LOAD_PATTERN_EDIT: post_load_name = "Pattern Edit"; break;
            case POST_LOAD_SONG_CONFIG:  post_load_name = "Song Config";  break;
            case POST_LOAD_INST_EDIT:
            default:                     post_load_name = "Inst Editor";  break;
        }
        sprintf(buf+strlen(buf),"\n|U| Post-Load Page  |L|[|H|%s|L|]",post_load_name);
        sprintf(buf+strlen(buf),"\n|U| Step Editing    |L|[|H|%s|L|]",zt_config_globals.step_editing?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Centered Edit   |L|[|H|%s|L|]",zt_config_globals.centered_editing?"On":"Off");
        sprintf(buf+strlen(buf),"\n|U| Screen Size     |L|[|H|%dx%d|L|]",zt_config_globals.screen_width, zt_config_globals.screen_height);
        sprintf(buf+strlen(buf),"\n|U| Zoom            |L|[|H|%.2f|L|]",zt_config_globals.zoom);
        sprintf(buf+strlen(buf),"\n|U| Scale Filter    |L|[|H|%s|L|]",zt_config_globals.scale_filter);
        sprintf(buf+strlen(buf),"\n|U| Ctrl Nav Amt    |L|[|H|%d|L|]",zt_config_globals.control_navigation_amount);
        sprintf(buf+strlen(buf),"\n|U| Inst Glob Vol   |L|[|H|%d|L|]",zt_config_globals.instrument_global_volume);
        sprintf(buf+strlen(buf),"\n|U| Default TPB     |L|[|H|%d|L|]",zt_config_globals.default_tpb);
        sprintf(buf+strlen(buf),"\n|U| Default BPM     |L|[|H|%d|L|]",zt_config_globals.default_bpm);
#ifdef DISABLED_CONFIGURATION_VALUES
        sprintf(buf+strlen(buf),"\n|U| Key Repeat      |L|[|H|%d|L|] (disabled)",zt_config_globals.key_repeat_time);
        sprintf(buf+strlen(buf),"\n|U| Key Wait        |L|[|H|%d|L|] (disabled)",zt_config_globals.key_wait_time);
#else
        sprintf(buf+strlen(buf),"\n|U| Key Repeat      |L|[|H|%d|L|]",zt_config_globals.key_repeat_time);
        sprintf(buf+strlen(buf),"\n|U| Key Wait        |L|[|H|%d|L|]",zt_config_globals.key_wait_time);
#endif

        if(tb->text != NULL)
        {
            free((void*)tb->text);
        }
        tb->text = strdup(buf);
        // Auto-fit textbox height to actual content (count newlines).
        // No trailing pad row — avoids wasted black space at the bottom.
        {
            int nlines = 1;
            for (const char *p = buf; *p; p++) if (*p == '\n') nlines++;
            tb->ysize = nlines - 1;
            if (tb->ysize < 1) tb->ysize = 1;
        }
        UI->draw(S);
#ifdef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
        {
            ValueSlider *vs = (ValueSlider *)UI->get_element(5);
            if (vs) {
                const char *view_mode_name = "Regular";
                switch (zt_config_globals.cur_edit_mode) {
                    case VIEW_SQUISH:  view_mode_name = "Squish"; break;
                    case VIEW_REGULAR: view_mode_name = "Regular"; break;
                    case VIEW_FX:      view_mode_name = "FX"; break;
                    case VIEW_BIG:     view_mode_name = "Big"; break;
                }
                char label[16];
                snprintf(label, sizeof(label), " %-8s", view_mode_name);
                printBG(col(vs->x + vs->xsize), row(vs->y), "         ", COLORS.Text, COLORS.Background, S);
                printBG(col(vs->x + vs->xsize), row(vs->y), label, COLORS.Text, COLORS.Background, S);
            }
        }
#endif
        // Inline label for the Post-Load Page slider (mirrors the View
        // Mode pattern above — slider value alone reads as 0/1/2 which
        // is meaningless to the user without the page name beside it).
        {
            ValueSlider *vs = (ValueSlider *)UI->get_element(11);
            if (vs) {
                const char *post_load_name = "Inst Editor";
                switch (zt_config_globals.post_load_page) {
                    case POST_LOAD_PATTERN_EDIT: post_load_name = "Pattern Edit"; break;
                    case POST_LOAD_SONG_CONFIG:  post_load_name = "Song Config";  break;
                    case POST_LOAD_INST_EDIT:
                    default:                     post_load_name = "Inst Editor";  break;
                }
                char label[20];
                snprintf(label, sizeof(label), " %-12s", post_load_name);
                printBG(col(vs->x + vs->xsize), row(vs->y), "             ", COLORS.Text, COLORS.Background, S);
                printBG(col(vs->x + vs->xsize), row(vs->y), label, COLORS.Text, COLORS.Background, S);
            }
        }
        draw_status(S);
        status(S);
        printtitle(PAGE_TITLE_ROW_Y,"Global Configuration (Ctrl+F12)",COLORS.Text,COLORS.Background,S);
        print(row(2),col(14),"Autoload .ZT",COLORS.Text,S);
        print(row(28),col(14),"Autoload File",COLORS.Text,S);
        print(row(2),col(16),"Default Dir",COLORS.Text,S);
        print(row(2),col(18),"Record Velocity",COLORS.Text,S);
        print(row(2),col(19),"Autosave (sec)",COLORS.Text,S);
#ifdef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
        print(row(2),col(20),"Default View",COLORS.Text,S);
#endif
        print(row(2),col(21),"Row highlight minor",COLORS.Text,S);
        print(row(40),col(21),"Row highlight major",COLORS.Text,S);
        print(row(2),col(23),"Default Pat Len",COLORS.Text,S);
        // MIDI In Sync + Chase MIDI Tempo labels moved to F11 (Songconfig).
        print(row(2),col(24),"Post-Load Page",COLORS.Text,S);
//        print(row(2),col(25)," .ZT directory",COLORS.Text,S);

        //printtitle(32,"Current Global Settings",COLORS.Text,COLORS.Background,S);

        need_refresh = 0; updated=2;
        S->unlock();
    }
}

