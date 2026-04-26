
#include "zt.h"
#include "OrderEditor.h"

CUI_Songconfig::CUI_Songconfig(void) {
    ValueSlider *vs;
    TextInput *ti;
    CheckBox *cb;
    Frame *fm;
    
//    CommentEditor *ce;

    int base_y = TRACKS_ROW_Y;

    tpb_tab[0] = 2;
    tpb_tab[1] = 4;
    tpb_tab[2] = 6;
    tpb_tab[3] = 8;
    tpb_tab[4] = 12;
    tpb_tab[5] = 16;
    tpb_tab[6] = 24;
    tpb_tab[7] = 32;
    tpb_tab[8] = 48;

    rev_tpb_tab[2] = 0;
    rev_tpb_tab[4] = 1;
    rev_tpb_tab[6] = 2;
    rev_tpb_tab[8] = 3;
    rev_tpb_tab[12] = 4;
    rev_tpb_tab[16] = 5;
    rev_tpb_tab[24] = 6;
    rev_tpb_tab[32] = 7;
    rev_tpb_tab[48] = 8;
    
    UI = new UserInterface;
    /* Initialize Title TextInput */
        ti = new TextInput;
        UI->add_element(ti,0);
        ti->frame = 1;
        ti->x = 17;
        ti->y = base_y;
        ti->xsize=28;
        ti->length=28;
        ti->str = &song->title[0];
    // END Test Slider
    /* Initialize BPM Slider */
        vs = new ValueSlider;
        UI->add_element(vs,1);
        vs->frame = 0;
        vs->x = 17; 
        vs->y = base_y+2; 
        vs->xsize=28;
        vs->min = 60;
        vs->max = 240;
        vs->value = song->bpm;
    // END Test Slider
    /* Initialize TPB Slider 2 */
        vs = new ValueSlider;
        UI->add_element(vs,2);
        vs->frame = 0;
        vs->x = 17; 
        vs->y = base_y+3; 
        vs->xsize=28;
        vs->min = 0;
        vs->max = 8;
        vs->value = rev_tpb_tab[song->tpb];
        vs->force_f=1; vs->force_v = song->tpb;
    // END Test Slider
    /* Initialize Frame for those two above*/
        fm = new Frame;
        UI->add_gfx(fm,0);
        fm->x = 17;
        fm->y = base_y+2;
        fm->xsize = 28;
        fm->ysize = 2;
    // END Midi Clock
    /* Initialize MIDI Clock Checkbox */
        cb = new CheckBox;
        UI->add_element(cb,3);
        cb->frame = 0;
        cb->x = 17;
        cb->y = base_y+5;
        cb->xsize = 5;
        cb->value = &song->flag_SendMidiClock;
    // END Midi Clock
    /* Initialize MIDI Stop/Start checkbox */
        cb = new CheckBox;
        UI->add_element(cb,4);
        cb->frame = 0;
        cb->x = 17;
        cb->y = base_y+6;
        cb->xsize = 5;
        cb->value = &song->flag_SendMidiStopStart;
    // END Midi Clock
    /* Initialize Frame for those two above */
        fm = new Frame;
        UI->add_gfx(fm,1);
        fm->x = 17;
        fm->y = base_y+5; 
        fm->xsize = 5;
        fm->ysize = 2;
    // END Frame

        // Row highlight minor / major (the global zt_config_globals
        // values, surfaced here so song-edit tasks can tweak them
        // without leaving F11).
        vs = new ValueSlider;
        UI->add_element(vs, 6);
        vs->frame = 0;
        vs->x = 17;
        vs->y = base_y + 8;
        vs->xsize = 28;
        vs->min = 1;
        vs->max = 64;
        vs->value = zt_config_globals.highlight_increment;

        vs = new ValueSlider;
        UI->add_element(vs, 7);
        vs->frame = 0;
        vs->x = 17;
        vs->y = base_y + 9;
        vs->xsize = 28;
        vs->min = 1;
        vs->max = 64;
        vs->value = zt_config_globals.lowlight_increment;

        fm = new Frame;
        UI->add_gfx(fm, 2);
        fm->x = 17;
        fm->y = base_y + 8;
        fm->xsize = 28;
        fm->ysize = 2;

        // MIDI In Sync (slave to incoming MIDI clock).
        cb = new CheckBox;
        UI->add_element(cb, 8);
        cb->frame = 1;
        cb->x = 17;
        cb->y = base_y + 11;
        cb->xsize = 5;
        cb->value = &zt_config_globals.midi_in_sync;

        // Chase MIDI Tempo.
        cb = new CheckBox;
        UI->add_element(cb, 9);
        cb->frame = 1;
        cb->x = 17;
        cb->y = base_y + 12;
        cb->xsize = 5;
        cb->value = &zt_config_globals.midi_in_sync_chase_tempo;

        oe = new OrderEditor;
        UI->add_element(oe,10);
        oe->x = 59;
        oe->y = 13;
        oe->xsize = 9;
        oe->ysize = 32 ;
}

void CUI_Songconfig::enter(void) {
    ValueSlider *vs;
    need_refresh = 1;
    vs = (ValueSlider *)UI->get_element(1);
    vs->value = song->bpm;
    vs = (ValueSlider *)UI->get_element(2);
    vs->value = rev_tpb_tab[song->tpb];

    vs->force_f=1; 
    vs->force_v = song->tpb;

    if(!zt_config_globals.highlight_increment)
        zt_config_globals.highlight_increment = song->tpb;
    if(!zt_config_globals.lowlight_increment)
        zt_config_globals.lowlight_increment = song->tpb >> 1 / song->tpb / 2;
    
    cur_state = STATE_SONG_CONFIG;
    Keys.flush();
    UI->set_focus(5); // set focus to order editor
}

void CUI_Songconfig::leave(void) {

}

void CUI_Songconfig::update()
{
    // Reserve 7 rows at the bottom for the 55 px toolbar (ceil(55/8));
    // one extra row of safety was previously added but it left the order
    // list one row short of the F1 Help bottom edge.
    oe->ysize =  ((INTERNAL_RESOLUTION_Y/8) - oe->y - 7) ;

    ValueSlider *vs;
    TextInput *t;

    t = (TextInput *)UI->get_element(0);
    if (t->changed)
        file_changed++;

    UI->update();
    vs = (ValueSlider *)UI->get_element(1);
    if (vs->value != song->bpm) { 
        song->bpm = vs->value; 
        ztPlayer->set_speed();

        if(!zt_config_globals.highlight_increment)
            zt_config_globals.highlight_increment = song->tpb;
        if(!zt_config_globals.lowlight_increment)
            zt_config_globals.lowlight_increment = song->tpb >> 1 / song->tpb / 2;
       
        file_changed++;
    }
    vs = (ValueSlider *)UI->get_element(2);
    if (vs->value != rev_tpb_tab[song->tpb]) {
        song->tpb = tpb_tab[vs->value];
        ztPlayer->set_speed();
        vs->force_f=1; vs->force_v = song->tpb;

        if(!zt_config_globals.highlight_increment)
            zt_config_globals.highlight_increment = song->tpb;
        if(!zt_config_globals.lowlight_increment)
            zt_config_globals.lowlight_increment = song->tpb >> 1 / song->tpb / 2;

        file_changed++;
    }
    vs = (ValueSlider *)UI->get_element(6);
    if (vs && vs->value != zt_config_globals.highlight_increment) {
        zt_config_globals.highlight_increment = vs->value;
    } else if (vs) {
        vs->value = zt_config_globals.highlight_increment;
    }
    vs = (ValueSlider *)UI->get_element(7);
    if (vs && vs->value != zt_config_globals.lowlight_increment) {
        zt_config_globals.lowlight_increment = vs->value;
    } else if (vs) {
        vs->value = zt_config_globals.lowlight_increment;
    }
    {
        CheckBox *cb = (CheckBox *)UI->get_element(8);
        if (cb && cb->changed)
            zt_config_globals.midi_in_sync = *(cb->value);
        cb = (CheckBox *)UI->get_element(9);
        if (cb && cb->changed)
            zt_config_globals.midi_in_sync_chase_tempo = *(cb->value);
    }
    if (Keys.size()) {
        Keys.getkey();
    }
}

void CUI_Songconfig::draw(Drawable *S) {
    int base_y = TRACKS_ROW_Y;
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(PAGE_TITLE_ROW_Y,"Song Configuration (F11)",COLORS.Text,COLORS.Background,S);
        // Right-align Title/BPM/TPB labels so their right edge sits one
        // char before the textfield/slider start (col 17), matching the
        // tight "Send MIDI Clock" / "MIDI Stop/Start" pattern below.
        // "Title" = 5 chars → col 11; "BPM"/"TPB" = 3 chars → col 13.
        print(row(11),col(base_y),"Title",COLORS.Text,S);
        print(row(13),col(base_y+2),"BPM",COLORS.Text,S);
        print(row(13),col(base_y+3),"TPB",COLORS.Text,S);
        print(row(1),col(base_y+5),"Send MIDI Clock",COLORS.Text,S);
        print(row(1),col(base_y+6.),"MIDI Stop/Start",COLORS.Text,S);
        // Row highlight minor / major sliders share the BPM/TPB column.
        print(row(1),col(base_y+8),"Row hl minor   ",COLORS.Text,S);
        print(row(1),col(base_y+9),"Row hl major   ",COLORS.Text,S);
        printchar(row(17 + 27) + 1,col(base_y+8),0x84,COLORS.Highlight,S);
        printchar(row(17 + 27) + 1,col(base_y+9),0x84,COLORS.Highlight,S);
        // MIDI sync settings (moved from F12 Sysconfig + Ctrl+F12 Global Config).
        print(row(1),col(base_y+11),"  MIDI In Sync",COLORS.Text,S);
        print(row(1),col(base_y+12),"Chase MIDI Tempo",COLORS.Text,S);
        // Order List label aligned to the OE x-origin (col 59) so the
        // header sits flush over the "000" index column.
        print(row(59),col(11),"Order List",COLORS.Text,S);
        printchar(row(17 + 27) + 1,col(base_y+2),0x84,COLORS.Highlight,S);
        printchar(row(17 + 27) + 1,col(base_y+3),0x84,COLORS.Highlight,S);

        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
         S->unlock();
    }
}