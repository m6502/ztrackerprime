
#include "zt.h"
#include "OrderEditor.h"
#include "ableton_link.h"

CUI_Songconfig::CUI_Songconfig(void) {
    ValueSlider *vs;
    TextInput *ti;
    CheckBox *cb;
    Frame *fm;
    
//    CommentEditor *ce;

    int base_y = TRACKS_ROW_Y;

    last_bpm_seen = -1;

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
        ti->x = 20;
        ti->y = base_y;
        ti->xsize=ZTM_SONGTITLE_MAXLEN - 1;
        ti->length=ZTM_SONGTITLE_MAXLEN - 1;
        ti->str = &song->title[0];
    // END Test Slider
    /* Initialize BPM Slider */
        vs = new ValueSlider;
        UI->add_element(vs,1);
        vs->frame = 0;
        vs->x = 20; 
        vs->y = base_y+2; 
        vs->xsize=28;
        vs->min = 20;
        vs->max = 500;
        vs->value = song->bpm;
    // END Test Slider
    /* Initialize TPB Slider 2 */
        vs = new ValueSlider;
        UI->add_element(vs,2);
        vs->frame = 0;
        vs->x = 20; 
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
        fm->x = 20;
        fm->y = base_y+2;
        fm->xsize = 28;
        fm->ysize = 2;
    // END Midi Clock
    /* Initialize MIDI Clock Checkbox */
        cb = new CheckBox;
        UI->add_element(cb,3);
        cb->frame = 0;
        cb->x = 20;
        cb->y = base_y+5;
        cb->xsize = 3;
        cb->value = &song->flag_SendMidiClock;
    // END Midi Clock
    /* Initialize MIDI Stop/Start checkbox */
        cb = new CheckBox;
        UI->add_element(cb,4);
        cb->frame = 0;
        cb->x = 20;
        cb->y = base_y+6;
        cb->xsize = 3;
        cb->value = &song->flag_SendMidiStopStart;
    // END Midi Clock
        // Send MIDI Clock + MIDI Stop/Start checkboxes have no surrounding
        // Frame — its right border char rendered as a yellow stripe to the
        // right of the chips and the unframed style matches MIDI In Sync /
        // Chase MIDI Tempo below.

        // Row Highlight + Row Lowlight (the global zt_config_globals values,
        // surfaced here so song-edit tasks can tweak them without leaving F11).
        vs = new ValueSlider;
        UI->add_element(vs, 5);
        vs->frame = 0;
        vs->x = 20;
        vs->y = base_y + 8;
        vs->xsize = 28;
        vs->min = 1;
        vs->max = 64;
        vs->value = zt_config_globals.highlight_increment;

        vs = new ValueSlider;
        UI->add_element(vs, 6);
        vs->frame = 0;
        vs->x = 20;
        vs->y = base_y + 9;
        vs->xsize = 28;
        vs->min = 1;
        vs->max = 64;
        vs->value = zt_config_globals.lowlight_increment;

        fm = new Frame;
        UI->add_gfx(fm, 2);
        fm->x = 20;
        fm->y = base_y + 8;
        fm->xsize = 28;
        fm->ysize = 2;

        // MIDI In Sync (slave to incoming MIDI clock).
        cb = new CheckBox;
        UI->add_element(cb, 7);
        cb->frame = 0;
        cb->x = 20;
        cb->y = base_y + 11;
        cb->xsize = 3;
        cb->value = &zt_config_globals.midi_in_sync;

        // Chase MIDI Tempo.
        cb = new CheckBox;
        UI->add_element(cb, 8);
        cb->frame = 0;
        cb->x = 20;
        cb->y = base_y + 12;
        cb->xsize = 3;
        cb->value = &zt_config_globals.midi_in_sync_chase_tempo;

        // 4 / 8 audition advance: 0 = stay, 1 = one row, 2 = use EditStep.
        // Default 1 — the historical value-of-cur_step behaviour was
        // surprising for users who set EditStep > 1 to lay coarse grids
        // but still wanted single-row auditioning. Stored as a plain int
        // so zt.conf round-trips it cleanly.
        vs = new ValueSlider;
        UI->add_element(vs, 10);
        vs->frame = 0;
        vs->x = 20;
        vs->y = base_y + 13;
        vs->xsize = 3;
        vs->min = ZT_NAS_NONE;
        vs->max = ZT_NAS_EDITSTEP;
        vs->value = zt_config_globals.note_audition_step_mode;

        // Ableton Link
        cb = new CheckBox;
        UI->add_element(cb, 11);
        cb->frame = 0;
        cb->x = 20;
        cb->y = base_y + 16;
        cb->xsize = 3;
        cb->value = &zt_config_globals.ableton_link_enable;

        cb = new CheckBox;
        UI->add_element(cb, 12);
        cb->frame = 0;
        cb->x = 20;
        cb->y = base_y + 17;
        cb->xsize = 3;
        cb->value = &zt_config_globals.ableton_link_start_stop_sync;

        vs = new ValueSlider;
        UI->add_element(vs, 13);
        vs->frame = 0;
        vs->x = 20;
        vs->y = base_y + 18;
        vs->xsize = 28;
        vs->min = 0;
        vs->max = 500;
        vs->value = zt_config_globals.ableton_link_offset_ms;

        oe = new OrderEditor;
        UI->add_element(oe,9);
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
    last_bpm_seen = song->bpm;   // baseline the user-vs-external BPM disambiguator
    vs = (ValueSlider *)UI->get_element(2);
    vs->value = rev_tpb_tab[song->tpb];

    vs->force_f=1; 
    vs->force_v = song->tpb;

    if(!zt_config_globals.highlight_increment)
        zt_config_globals.highlight_increment = song->tpb;
    if(!zt_config_globals.lowlight_increment)
        zt_config_globals.lowlight_increment = song->tpb >> 1 / song->tpb / 2;

    // Reflect the live config value in the slider whenever this page
    // is entered (config could have been changed elsewhere — e.g. by
    // hand-editing zt.conf and reloading).
    vs = (ValueSlider *)UI->get_element(10);
    if (vs) vs->value = zt_config_globals.note_audition_step_mode;
    vs = (ValueSlider *)UI->get_element(13);
    if (vs) vs->value = zt_config_globals.ableton_link_offset_ms;

    // F11 toggle: pressing F11 while already on Songconfig flips focus
    // between OrderEditor (id 9) and Title (id 0) on every press, so the
    // user can keep tapping F11 to alternate. Fresh entry from another
    // page lands on the OrderEditor.
    bool same_page = (LastPage == this);
    cur_state = STATE_SONG_CONFIG;
    // Reset OrderEditor cursor so the focus indicator is always visible
    // at row 0 (otherwise stale cursor_y/list_start could leave it
    // scrolled off-screen). Only reset on a fresh entry.
    if (oe && !same_page) {
        oe->cursor_y = 0;
        oe->cursor_x = 0;
        oe->list_start = 0;
    }
    Keys.flush();
    int target;
    if (same_page) {
        target = (UI->cur_element == 0) ? 9 : 0;
    } else {
        target = 9;   // fresh entry → OrderEditor
    }
    UI->set_focus(target);
    UI->cur_element = target;
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
    if (vs->from_input) vs->from_input = 0;  // BPM slider takes any [min,max]; clamp already applied
    if (vs->value != last_bpm_seen) {
        song->bpm = vs->value;
        ztPlayer->set_speed();

        if(!zt_config_globals.highlight_increment)
            zt_config_globals.highlight_increment = song->tpb;
        if(!zt_config_globals.lowlight_increment)
            zt_config_globals.lowlight_increment = song->tpb >> 1 / song->tpb / 2;

        file_changed++;
    } else if (song->bpm != vs->value) {
        // song->bpm changed underneath us (the Ableton Link tempo pump in the
        // main loop follows the session tempo) -> move the slider to match so
        // the F11 BPM display tracks Ableton instead of snapping it back.
        vs->value = song->bpm;
        vs->need_redraw++;
        need_refresh++;
    }
    last_bpm_seen = vs->value;
    vs = (ValueSlider *)UI->get_element(2);
    if (vs->from_input) {
        // Typed-numeric path. vs->input_value holds the raw number the
        // user entered (preserved before clamping in Sliders.cpp). Snap
        // to the nearest valid TPB in tpb_tab[]={2,4,6,8,12,16,24,32,48}
        // so '8' -> TPB=8 (slider index 3), '5' -> TPB=4 (nearest), etc.
        // On ties, prefer the higher TPB (more granular, common in
        // tracker conventions).
        int typed = vs->input_value;
        int best_idx = 0;
        int best_diff = abs(tpb_tab[0] - typed);
        for (int i = 1; i <= 8; i++) {
            int diff = abs(tpb_tab[i] - typed);
            if (diff < best_diff || (diff == best_diff && tpb_tab[i] > typed)) {
                best_diff = diff;
                best_idx = i;
            }
        }
        vs->value = best_idx;
        vs->from_input = 0;
    }
    // Clamp slider index to the table range regardless of input source.
    if (vs->value < 0) vs->value = 0;
    if (vs->value > 8) vs->value = 8;
    if (tpb_tab[vs->value] != song->tpb) {
        song->tpb = tpb_tab[vs->value];
        ztPlayer->set_speed();
        vs->force_f=1; vs->force_v = song->tpb;

        if(!zt_config_globals.highlight_increment)
            zt_config_globals.highlight_increment = song->tpb;
        if(!zt_config_globals.lowlight_increment)
            zt_config_globals.lowlight_increment = song->tpb >> 1 / song->tpb / 2;

        file_changed++;
    }
    vs = (ValueSlider *)UI->get_element(5);
    if (vs && vs->from_input) vs->from_input = 0;
    if (vs && vs->value != zt_config_globals.highlight_increment) {
        zt_config_globals.highlight_increment = vs->value;
    } else if (vs) {
        vs->value = zt_config_globals.highlight_increment;
    }
    vs = (ValueSlider *)UI->get_element(13);
    if (vs && vs->from_input) vs->from_input = 0;
    if (vs && vs->value != zt_config_globals.ableton_link_offset_ms) {
        zt_config_globals.ableton_link_offset_ms = vs->value;
    } else if (vs) {
        vs->value = zt_config_globals.ableton_link_offset_ms;
    }

    vs = (ValueSlider *)UI->get_element(6);
    if (vs && vs->from_input) vs->from_input = 0;
    if (vs && vs->value != zt_config_globals.lowlight_increment) {
        zt_config_globals.lowlight_increment = vs->value;
    } else if (vs) {
        vs->value = zt_config_globals.lowlight_increment;
    }
    {

        static int prev_link = -1;
        static int prev_sync = -1;
        int link_now = zt_config_globals.ableton_link_enable;
        int sync_now = zt_config_globals.midi_in_sync;

        if (prev_link >= 0 && link_now != prev_link) {
            if (link_now && !zt_ableton_link_available()) {
                // Stub build: nothing to enable. Snap the toggle back off.
                zt_config_globals.ableton_link_enable = link_now = 0;
                statusmsg = (char*)"Ableton Link not compiled in this build";
            } else if (link_now) {
                // Disable MIDI In Sync (don't want both on at same time)
                zt_config_globals.midi_in_sync = 0;
                zt_config_globals.midi_in_sync_chase_tempo = 0;
                sync_now = 0;
                UI->get_element(7)->need_redraw++;
                UI->get_element(8)->need_redraw++;
                statusmsg = (char*)"Ableton Link enabled";
            } else {
                statusmsg = (char*)"Ableton Link disabled";
            }
            status_change = 1;
            need_refresh++;
        }

        if (prev_sync >= 0 && sync_now != prev_sync && sync_now) {
            // Mutual exclusion the other way: MIDI-In-Sync on kicks Link off.
            if (zt_config_globals.ableton_link_enable) {
                zt_config_globals.ableton_link_enable = link_now = 0;
                UI->get_element(11)->need_redraw++;   // cross-cleared checkbox
                statusmsg = (char*)"Ableton Link disabled (MIDI-In Sync took over)";
                status_change = 1;
                need_refresh++;
            }
        }

        prev_link = zt_config_globals.ableton_link_enable;
        prev_sync = zt_config_globals.midi_in_sync;

        // Live status: peer count / session tempo move asynchronously as peers
        // join, leave, or retempo. update() runs every frame but draw() only
        // repaints when need_refresh != 0 — so bump it when a value we display
        // actually changed (invariant: background state change -> need_refresh++).
        if (zt_config_globals.ableton_link_enable) {
            static size_t last_peers = (size_t)-1;
            static int    last_bpm10 = -1;
            size_t peers = zt_ableton_link_num_peers();
            int    bpm10 = (int)(zt_ableton_link_get_tempo() * 10.0 + 0.5);
            if (peers != last_peers || bpm10 != last_bpm10) {
                last_peers = peers;
                last_bpm10 = bpm10;
                need_refresh++;
            }
        }
    }
    vs = (ValueSlider *)UI->get_element(10);
    if (vs) {
        if (vs->from_input) vs->from_input = 0;
        if (vs->value < ZT_NAS_NONE)     vs->value = ZT_NAS_NONE;
        if (vs->value > ZT_NAS_EDITSTEP) vs->value = ZT_NAS_EDITSTEP;
        if (vs->value != zt_config_globals.note_audition_step_mode) {
            zt_config_globals.note_audition_step_mode = vs->value;
            switch (vs->value) {
                case ZT_NAS_NONE:
                    statusmsg = (char*)"Note audition (4/8): cursor stays on row";
                    break;
                case ZT_NAS_ONE:
                    statusmsg = (char*)"Note audition (4/8): cursor advances 1 row";
                    break;
                case ZT_NAS_EDITSTEP:
                    statusmsg = (char*)"Note audition (4/8): cursor advances by EditStep";
                    break;
            }
            status_change = 1;
        }
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
        // All labels right-align so text ends col 18 (1-char gap before
        // col-20 controls). Order List is on its own column (col 59)
        // and is intentionally NOT shifted.
        print(row(14),col(base_y),"Title",COLORS.Text,S);
        print(row(16),col(base_y+2),"BPM",COLORS.Text,S);
        print(row(16),col(base_y+3),"TPB",COLORS.Text,S);
        print(row(4),col(base_y+5),"Send MIDI Clock",COLORS.Text,S);
        print(row(4),col(base_y+6.),"MIDI Stop/Start",COLORS.Text,S);
        print(row(5),col(base_y+8),"Row Highlight ",COLORS.Text,S);
        print(row(6),col(base_y+9),"Row Lowlight ",COLORS.Text,S);
        printchar(row(20 + 27) + 1,col(base_y+8),0x84,COLORS.Highlight,S);
        printchar(row(20 + 27) + 1,col(base_y+9),0x84,COLORS.Highlight,S);
        print(row(4),col(base_y+11),"   MIDI In Sync",COLORS.Text,S);
        print(row(3),col(base_y+12),"Chase MIDI Tempo",COLORS.Text,S);
        // 4 / 8 audition step mode label. The slider value is rendered
        // wider than xsize so the legend goes on the row below to avoid
        // colliding with the on-slider digits.
        print(row(2),col(base_y+13),"4/8 Audition Step",COLORS.Text,S);
        print(row(20),col(base_y+14),"0=stay  1=row  2=editstep",COLORS.Text,S);

        // Ableton Link block. Labels right-align to col 18 like the rest.
        print(row(6),col(base_y+16),"Ableton Link",COLORS.Text,S);
        print(row(3),col(base_y+17),"Link Start/Stop",COLORS.Text,S);
        print(row(4),col(base_y+18),"Link Offset ms",COLORS.Text,S);
        {
            char linkstat[96];
            if (!zt_ableton_link_available()) {
                snprintf(linkstat, sizeof(linkstat), "(not compiled in)");
            } else if (zt_config_globals.ableton_link_enable) {
                size_t peers = zt_ableton_link_num_peers();
                snprintf(linkstat, sizeof(linkstat), "%d peer%s \xb3 %.1f BPM",
                         (int)peers, peers == 1 ? "" : "s", zt_ableton_link_get_tempo());
            } else {
                snprintf(linkstat, sizeof(linkstat), "off");
            }
            // Pad to a fixed width and draw with a background fill so a shorter
            // status (e.g. "off") fully erases the previous longer one
            // ("N peers ... BPM"). print() only paints glyph pixels — without
            // this the old text shows through the new, garbled. 28 cols span
            // col 24..51, clear of the Order List at col 59.
            char linkstat_field[40];
            snprintf(linkstat_field, sizeof(linkstat_field), "%-28s", linkstat);
            printBG(row(24),col(base_y+16),linkstat_field,COLORS.Highlight,COLORS.Background,S);
        }
        // Order List label aligned to the OE x-origin (col 59) — NOT shifted.
        print(row(59),col(11),"Order List",COLORS.Text,S);
        printchar(row(20 + 27) + 1,col(base_y+2),0x84,COLORS.Highlight,S);
        printchar(row(20 + 27) + 1,col(base_y+3),0x84,COLORS.Highlight,S);

        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
         S->unlock();
    }
}
