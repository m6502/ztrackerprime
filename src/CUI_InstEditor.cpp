#include "zt.h"
#include "InstEditor.h"
#include "Button.h"
#include "keyjazz_map.h"


void InitInstruments(void) {
    for (int i=0;i<MAX_INSTS;i++) {
        MidiOut->progChange(song->instruments[i]->midi_device,song->instruments[i]->patch,song->instruments[i]->bank,song->instruments[i]->channel);
        SDL_Delay(5);
    }
}
void InitInstrumentsOneDev(int dev) {
    for (int i=0;i<MAX_INSTS;i++) {
        if (song->instruments[i]->midi_device == dev)
            MidiOut->progChange(dev,song->instruments[i]->patch,song->instruments[i]->bank,song->instruments[i]->channel);
        SDL_Delay(5);
    }
}
void BTNCLK_InitInstruments(UserInterfaceElement*) {
    InitInstruments();
    sprintf(szStatmsg,"All instruments updated on all devices");
    statusmsg = szStatmsg;
    need_refresh++; 
}
void BTNCLK_InitInstrumentsOne(UserInterfaceElement*) {
    unsigned int dev = song->instruments[cur_inst]->midi_device;
    if (dev < MidiOut->numOuputDevices) {
        InitInstrumentsOneDev(dev);
        sprintf(szStatmsg,"All instruments attached to %s updated",MidiOut->outputDevices[dev]->szName);
    } else {
        sprintf(szStatmsg,"The current instrument is not attached to a valid MIDI Out device");
    }
    statusmsg = szStatmsg;
    need_refresh++; 
}

// A slot is "available" to receive a generated channel instrument if it
// carries no real user data: no MIDI device, blank name, default patch / bank,
// and no CCizer bank. We deliberately IGNORE the channel field -- the
// instrument constructor pre-seeds channels 1..16 onto slots 0..15 as a
// convenience, so a strict isempty() (which counts that as data) would treat a
// fresh song's slots 1..15 as "used" and scatter the 16 instruments past slot
// 16. Ignoring channel lets a fresh song fill slots 0..15 consecutively.
static bool inst_slot_available(instrument *in) {
    // Default values mirror module.cpp's ZTM_INST_DEFAULT_* (those macros are
    // local to module.cpp): no device = 0xff, patch = -1, bank = -1.
    if (!in) return false;
    if (in->midi_device != 0xff) return false;
    if (in->patch       != -1)   return false;
    if (in->bank        != -1)   return false;
    if (in->ccizer_bank[0] != '\0') return false;
    for (int i = 0; i < ZTM_INSTTITLE_MAXLEN - 1; i++)
        if (in->title[i] != ' ') return false;
    return true;
}

// Fill the next up-to-16 empty instrument slots with the given MIDI device, on
// channels 1..16, named "<device> Channel 01".."Channel 16". Non-destructive:
// only writes into available slots (never overwrites). Returns the number of
// instruments created.
int inst_create_16_channels_for_device(unsigned int dev) {
    if (!song) return 0;
    if (dev >= MidiOut->numOuputDevices || MidiOut->outputDevices[dev] == NULL) {
        sprintf(szStatmsg, "Pick a MIDI Out device first");
        statusmsg = szStatmsg; need_refresh++;
        return 0;
    }

    // Match the device-list display: prefer a user alias, else the device name.
    const char *alias   = MidiOut->outputDevices[dev]->alias;
    const char *name    = MidiOut->outputDevices[dev]->szName;
    const char *devname = (alias != NULL && strlen(alias) > 1) ? alias : name;

    int created = 0;
    for (int i = 0; i < MAX_INSTS && created < 16; i++) {
        instrument *in = song->instruments[i];
        if (!inst_slot_available(in)) continue;

        in->midi_device = (unsigned char)dev;
        in->channel     = (unsigned char)created;   // 0..15 == MIDI channel 1..16

        // "<device> Channel NN". The title field is a fixed, space-padded
        // buffer (the UI expects spaces, not a short C string), so build into a
        // temp and copy. Device part is capped so " Channel NN" always fits.
        char nm[ZTM_INSTTITLE_MAXLEN];
        snprintf(nm, sizeof(nm), "%.13s Channel %02d", devname, created + 1);
        memset(in->title, ' ', ZTM_INSTTITLE_MAXLEN);
        size_t n = strlen(nm);
        if (n > ZTM_INSTTITLE_MAXLEN - 1) n = ZTM_INSTTITLE_MAXLEN - 1;
        memcpy(in->title, nm, n);
        in->title[ZTM_INSTTITLE_MAXLEN - 1] = 0;
        in->MarkAsUsed();
        created++;
    }

    if (created == 0)
        sprintf(szStatmsg, "No empty instrument slots available");
    else if (created < 16)
        sprintf(szStatmsg, "Created %d channels for %s (no more empty slots)", created, devname);
    else
        sprintf(szStatmsg, "Created 16 channels for %s", devname);
    statusmsg = szStatmsg; need_refresh++;
    return created;
}

// Convenience wrapper for the Alt+G shortcut: use the current instrument's
// device. (The F3-again popup lets you pick any open device directly.)
int inst_create_16_channels_for_current_device(void) {
    if (!song || cur_inst < 0 || cur_inst >= MAX_INSTS || !song->instruments[cur_inst])
        return 0;
    return inst_create_16_channels_for_device(song->instruments[cur_inst]->midi_device);
}

void BTNCLK_ToggleNoteRetrig(UserInterfaceElement *b) {
    Button *btn;
    btn = (Button *)b;
    btn->updown = !btn->updown;
    if (btn->updown)
        song->instruments[cur_inst]->flags |= INSTFLAGS_REGRIGGER_NOTE_ON_UNMUTE;
    else
        song->instruments[cur_inst]->flags &= (0xFF-INSTFLAGS_REGRIGGER_NOTE_ON_UNMUTE);
    need_refresh++;     
}

void BTNCLK_ToggleChanVol(UserInterfaceElement *b) {
    Button *btn;
    btn = (Button *)b;
    need_refresh++; 
    btn->updown = !btn->updown;
    if (btn->updown)
        song->instruments[cur_inst]->flags |= INSTFLAGS_CHANVOL;
    else
        song->instruments[cur_inst]->flags &= (0xFF-INSTFLAGS_CHANVOL);
    need_refresh++;     
}

void BTNCLK_ToggleTrackerMode(UserInterfaceElement *b) {
    Button *btn;
    btn = (Button *)b;
    need_refresh++; 
    btn->updown = !btn->updown;
    if (btn->updown)
        song->instruments[cur_inst]->flags |= INSTFLAGS_TRACKERMODE;
    else
        song->instruments[cur_inst]->flags &= (0xFF-INSTFLAGS_TRACKERMODE);
    need_refresh++;     
}

    MidiOutDeviceSelector *mds;

CUI_InstEditor::CUI_InstEditor(void) {

//    MidiDevSelectList *msl;
    ValueSlider *vs;
    ValueSliderDL *vsd;
    ValueSliderOFF *vso;
    Frame *fm;  
    Button *b;

    this->trackerModeButton = NULL;

    int tabindex = 0;
    int gfxindex = 0;

    UI = new UserInterface;
    ie = new InstEditor;
    UI->add_element(ie,tabindex++);
    ie->x = 5;
    ie->y = TRACKS_ROW_Y + 1;
    ie->xsize = 29;

    ie->ysize = MAX_INSTS ;

    ///////////////////////////////////////////////////
    // HERE we changed all the slider object constructors to have a (1) argument (for focus) -nic

    vso = new ValueSliderOFF(1); // Bank (ID: 1)
    UI->add_element(vso,tabindex++);
    vso->x = 36;    vso->y = 15;    vso->xsize = 13;    vso->ysize = 1; vso->min = -1;  vso->max = 0x3fff;    vso->value = -1;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=12; fm->xsize = 22; fm->ysize = 5; fm->type = 0;



    vso = new ValueSliderOFF(1); // Patch (ID: 2)
    UI->add_element(vso,tabindex++);
    vso->x = 58;    vso->y = 15;    vso->xsize = 16;    vso->ysize = 1; vso->min = -1;  vso->max = 127; vso->value = -1;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=12; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vs = new ValueSlider(1); // Default Volume (ID: 3)
    UI->add_element(vs,tabindex++);
    vs->x = 36; vs->y = 20; vs->xsize = 16; vs->ysize = 1;  vs->min = 0;    vs->max = 0x7f; vs->value = 0x7f;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=17; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vsd = new ValueSliderDL(1); // Default Length (ID: 4)
    UI->add_element(vsd,tabindex++);
    vsd->x = 58;    vsd->y = 20;    vsd->xsize = 16;    vsd->ysize = 1; vsd->min = 1;   vsd->max = 1000;    vsd->value = 0;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=17; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vs = new ValueSlider(1); // Global Volume (ID: 5)
    UI->add_element(vs,tabindex++);
    vs->x = 36; vs->y = 25; vs->xsize = 16; vs->ysize = 1;  vs->min = 0;    vs->max = 0x7f; vs->value = 0x7f;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=22; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vs = new ValueSlider(1); // Transpose (ID: 6)
    UI->add_element(vs,tabindex++);
    vs->x = 58; vs->y = 25; vs->xsize = 16; vs->ysize = 1;  vs->min = -127; vs->max = 127;  vs->value = 0;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=22; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vs = new ValueSlider(1); // Channel (ID: 7)
    UI->add_element(vs,tabindex++);
    vs->x = 36; vs->y = 30; vs->xsize = 16; vs->ysize = 1;  vs->min = 1;    vs->max = 16;   vs->value = 0;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=27; fm->xsize = 22; fm->ysize = 7; fm->type = 0;

    ////////////////////////////////////////////////////////////////////////////

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 58;
    b->y = 28;
    b->xsize = 20;
    b->ysize = 1;
    b->caption = "  Unmute Retrigger";
    b->OnClick = (ActFunc)BTNCLK_ToggleNoteRetrig;

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 58;
    b->y = 30;
    b->xsize = 20;
    b->ysize = 1;
    b->caption = "   Channel Volume";
    b->OnClick = (ActFunc)BTNCLK_ToggleChanVol;

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 58;
    b->y = 32;
    b->xsize = 20;
    b->ysize = 1;
    b->caption = "    Tracker Mode";
    b->OnClick = (ActFunc)BTNCLK_ToggleTrackerMode;


    trackerModeButton = b;

    fm = new Frame;  //frame for buttons
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=27; fm->xsize = 22; fm->ysize = 7; fm->type = 0;





    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 35;
    b->y = 35;
    b->xsize = 15;
    b->ysize = 1;
    b->caption = " Update Device";
    b->OnClick = (ActFunc)BTNCLK_InitInstrumentsOne;


    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 35+16;
    b->y = 35;
    b->xsize = 12;
    b->ysize = 1;
    b->caption = " Update All";
    b->OnClick = (ActFunc)BTNCLK_InitInstruments;

/*
    msl = new MidiDevSelectList;
    UI->add_element(msl,tabindex++);
    msl->x=35;
    msl->y=38;
    msl->xsize = 44;
    msl->ysize = 5;
*/
    mds = new MidiOutDeviceSelector;
    UI->add_element(mds, tabindex++);
    mds->x=35;
    mds->y=37;
    mds->xsize = 44;
    mds->ysize = 15;
    
    reset = 0;
}

void CUI_InstEditor::enter(void) {
    need_refresh = 1;
    cur_state = STATE_IEDIT;
    InstEditor *ie;
    ie = (InstEditor *)UI->get_element(0);
    if (cur_inst != ie->list_start+ie->cursor) {
        if (cur_inst<ie->ysize) {
            ie->cursor = cur_inst;
            ie->list_start = 0;
        } else {
            ie->list_start = cur_inst - ie->ysize+1;
            ie->cursor = cur_inst - ie->list_start;
        }
        if (ie->cursor<0) ie->cursor=0;
        if (ie->cursor>=ie->ysize) ie->cursor=ie->ysize-1;
        if (ie->list_start<0) ie->list_start = 0;
        if (ie->list_start>=MAX_INSTS-ie->ysize) ie->list_start=MAX_INSTS-ie->ysize;
    }
    UI->enter();
}
void CUI_InstEditor::leave(void) {
}

void CUI_InstEditor::update() {
    int key = 0,kstate=0;
    unsigned int scancode = 0;
    int set_note = 0xff;
    int act = 0;
    InstEditor *ie;

    UI->update();

    ie = (InstEditor *)UI->get_element(0);

    
    
    // <Manu> Refrescamos la pantalla cuando cambia la línea ----------- [EN: refresh the screen when the line changes]

    if(!ztPlayer->playing) ie->last_cur_row = -1 ;
    else {

      if( ztPlayer->cur_row != ie->last_cur_row ) {

        ie->last_cur_row = ztPlayer->cur_row ;

        need_refresh = 1;
        UI->full_refresh();
      }
    }

    // -----------------------------------------------------------------

      if(Keys.size() || midiInQueue.size()) {

        if (Keys.size()) {
            key = Keys.getkey();
            scancode = Keys.getcode();
            kstate = Keys.getstate();
        }
        // `text_cursor < 24` only means "name field is in edit state". It
        // does NOT mean the name field is currently focused -- ie's
        // text_cursor stays whatever it was last set to even after the
        // user Tabs to another widget (channel slider, midi-device list,
        // etc.). So also gate on `cur_element == 0`, the tabindex of the
        // InstEditor list itself, otherwise pressing Q on the channel
        // slider sees text_cursor==0 and suppresses keyjazz.
        const bool editing_name = (ie && ie->text_cursor < 24 &&
                                   UI->cur_element == 0);

        switch(kstate) {
            case KS_CTRL:
                switch(key) {
                    case SDLK_1: song->instruments[cur_inst]->channel = 0; act++; break;
                    case SDLK_2: song->instruments[cur_inst]->channel = 1; act++; break;
                    case SDLK_3: song->instruments[cur_inst]->channel = 2; act++; break;
                    case SDLK_4: song->instruments[cur_inst]->channel = 3; act++; break;
                    case SDLK_5: song->instruments[cur_inst]->channel = 4; act++; break;
                    case SDLK_6: song->instruments[cur_inst]->channel = 5; act++; break;
                    case SDLK_7: song->instruments[cur_inst]->channel = 6; act++; break;
                    case SDLK_8: song->instruments[cur_inst]->channel = 7; act++; break;
                    case SDLK_9: song->instruments[cur_inst]->channel = 8; act++; break;
                    case SDLK_0: song->instruments[cur_inst]->channel = 9; act++; break;
                }
                break;
            case KS_ALT:
                    switch(key) {
                    case SDLK_1: dev_sel(0,mds); act++; break;
                    case SDLK_2: dev_sel(1,mds); act++; break;
                    case SDLK_3: dev_sel(2,mds); act++; break;
                    case SDLK_4: dev_sel(3,mds); act++; break;
                    case SDLK_5: dev_sel(4,mds); act++; break;
                    case SDLK_6: dev_sel(5,mds); act++; break;
                    case SDLK_7: dev_sel(6,mds); act++; break;
                    case SDLK_8: dev_sel(7,mds); act++; break;
                    case SDLK_9: dev_sel(8,mds); act++; break;
                    case SDLK_0: dev_sel(9,mds); act++; break;
                    case SDLK_T: BTNCLK_ToggleTrackerMode(this->trackerModeButton); act++; break;
                    // Alt+G -- "Generate 16": create 16 instruments for the
                    // current instrument's MIDI device, one per channel 1..16,
                    // into the next empty slots. Same action as the F3-again
                    // popup's button.
                    case SDLK_G: inst_create_16_channels_for_current_device(); act++; break;
                    }
                    break;
            case KS_NO_SHIFT_KEYS:
                switch(key) {
                    case SDLK_KP_MULTIPLY:
                        if (editing_name) break;
                        if (base_octave<9) {
                            base_octave++;
                            need_refresh++; 
                        }
                        break;
                    case SDLK_KP_DIVIDE:
                        if (editing_name) break;
                        if (base_octave>0) {
                            base_octave--;
                            need_refresh++; 
                        }
                        break;
                    case SDLK_PAGEUP:
                        ie->cursor--;
                        if (ie->cursor<0)
                            ie->list_start--;
                        act++; break;
                    case SDLK_PAGEDOWN: 
                        ie->cursor++;
                        if (ie->cursor>=ie->ysize)
                            ie->list_start++;
                        act++; break;
                }
                if (!editing_name) {
                    // Shared keyjazz table (keyjazz_map.h): tracker vs piano
                    // layout toggles in one place for every audition path.
                    KeyjazzLayout kjl = zt_config_globals.keyjazz_piano_layout ? KJ_PIANO : KJ_TRACKER;
                    int kjoff = keyjazz_offset(scancode, kjl);
                    if (kjoff != KJ_NOT_A_NOTE) set_note = 12*base_octave + kjoff;
                }
            break;
        }

        int pvol = -1;

keepgoing:

        if (midiInQueue.size()>0) {
            int dw;
            dw = midiInQueue.pop();
            switch( dw&0xFF ) {
            case 0x80: // Note off
                key = (dw&0xFF00)>>8 ;
                key+=0xFF;
                if (jazz_note_is_active(key)) {
                    const mbuf st = jazz_get_state(key);
                    MidiOut->noteOff(song->instruments[cur_inst]->midi_device,st.note,st.chan,0x0,0);
                    jazz_clear_state(key);
                }
                break;
            case 0x90: // Note on
                set_note = key = (dw&0xFF00)>>8 ;
                pvol = (dw&0xFF0000)>>16;
                key+=0xFF;
                break;
            default:
                MidiOut->send(song->instruments[cur_inst]->midi_device, dw);
                break;
            }
        }
        
        if (act || set_note) {
            if (set_note < 0x80) {
                int uvol;
                uvol = song->instruments[cur_inst]->default_volume;
                if (pvol != -1)
                    uvol = pvol;
                if (song->instruments[cur_inst]->global_volume != 0x7F && uvol>0) {
                    uvol *= song->instruments[cur_inst]->global_volume;
                    uvol /= 0x7f;
                }
                set_note += song->instruments[cur_inst]->transpose; if (set_note>0x7f) set_note = 0x7f;
                MidiOut->noteOn(song->instruments[cur_inst]->midi_device,set_note,song->instruments[cur_inst]->channel,uvol,MAX_TRACKS,0);
                jazz_set_state(key, set_note, song->instruments[cur_inst]->channel);
            } else {
                need_refresh = 1;
                UI->full_refresh();
                int ocs = mds->cur_sel;
                int oys = mds->y_start;
                mds->OnChange();
                mds->cur_sel = ocs; mds->y_start = oys;
            }
        }
        if (midiInQueue.size()>0)
            goto keepgoing;
        if (ie->cursor<0) ie->cursor=0;
        if (ie->cursor>=ie->ysize) ie->cursor=ie->ysize-1;
        if (ie->list_start<0) ie->list_start = 0;
        if (ie->list_start>=MAX_INSTS-ie->ysize) ie->list_start=MAX_INSTS-ie->ysize;
    }

}


void CUI_InstEditor::draw(Drawable *S)
{
    ValueSlider *vs;
    ValueSliderDL *vsd;
    ValueSliderOFF *vso;
    Button *b;

    ie = (InstEditor *)UI->get_element(0);

    ie->ysize = 41 + ((INTERNAL_RESOLUTION_Y-480)/8);
    if(ie->ysize > MAX_INSTS) ie->ysize = MAX_INSTS ;

    if (ie->cursor+ie->list_start != cur_inst) {
        cur_inst = ie->cursor+ie->list_start;
        int ocs = mds->cur_sel;
        int oys = mds->y_start;
        mds->OnChange();
        mds->cur_sel = ocs; mds->y_start = oys;
        UI->full_refresh();
    }
    

    vso = (ValueSliderOFF*)UI->get_element(1); // bank
    if (vso->changed && !reset) {
        song->instruments[cur_inst]->bank = vso->value;
        MidiOut->progChange(song->instruments[cur_inst]->midi_device,song->instruments[cur_inst]->patch,song->instruments[cur_inst]->bank,song->instruments[cur_inst]->channel);
    } else
        vso->value = song->instruments[cur_inst]->bank;
    vso = (ValueSliderOFF*)UI->get_element(2); // patch
    if (vso->changed && !reset)  {
        song->instruments[cur_inst]->patch = vso->value;
        MidiOut->progChange(song->instruments[cur_inst]->midi_device,song->instruments[cur_inst]->patch,song->instruments[cur_inst]->bank,song->instruments[cur_inst]->channel);
    } else
        vso->value = song->instruments[cur_inst]->patch;
    vs = (ValueSlider*)UI->get_element(3);
    if (vs->changed && !reset)
        song->instruments[cur_inst]->default_volume = vs->value;
    else
        vs->value = song->instruments[cur_inst]->default_volume;
    vsd = (ValueSliderDL*)UI->get_element(4);
    if (vsd->changed && !reset)
        song->instruments[cur_inst]->default_length = vsd->value;
    else
        vsd->value = song->instruments[cur_inst]->default_length;

    vs = (ValueSlider*)UI->get_element(5);
    if (vs->changed && !reset) {
        song->instruments[cur_inst]->global_volume = vs->value;
    } else {
        vs->value = song->instruments[cur_inst]->global_volume;
    }
    vs = (ValueSlider*)UI->get_element(6);
    if (vs->changed && !reset) {
        song->instruments[cur_inst]->transpose = vs->value;
    } else {
        vs->value = song->instruments[cur_inst]->transpose;
    }


    vs = (ValueSlider*)UI->get_element(7);
    if (vs->changed && !reset) {
        song->instruments[cur_inst]->channel = vs->value - 1;
    } else {
        vs->value = song->instruments[cur_inst]->channel + 1;
    }

    b=(Button*)UI->get_element(8);
    if (song->instruments[cur_inst]->flags & INSTFLAGS_REGRIGGER_NOTE_ON_UNMUTE)
        b->updown = 1;
    else
        b->updown = 0;
    if (!b->mousestate && b->state < 2) b->state = b->updown;

    b=(Button*)UI->get_element(9);
    if (song->instruments[cur_inst]->flags & INSTFLAGS_CHANVOL)
        b->updown = 1;
    else
        b->updown = 0;
    if (!b->mousestate && b->state < 2) b->state = b->updown;

    b=(Button*)UI->get_element(10);
    if (song->instruments[cur_inst]->flags & INSTFLAGS_TRACKERMODE)
        b->updown = 1;
    else
        b->updown = 0;
    if (!b->mousestate && b->state < 2) b->state = b->updown;

    reset = 0;
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        status(S);
        printtitle(PAGE_TITLE_ROW_Y,"Instrument Editor (F3)",COLORS.Text,COLORS.Background,S);
        // CCizer bank attached to the focused instrument. Show the
        // basename (or "(none)") so the user can see which CC layout
        // was assigned via Shift+F3 -> B. Per-instrument data persists
        // in the .zt CCBN chunk.
        {
            const char *bank = (cur_inst >= 0 && cur_inst < MAX_INSTS &&
                                song->instruments[cur_inst] &&
                                song->instruments[cur_inst]->ccizer_bank[0])
                                   ? song->instruments[cur_inst]->ccizer_bank
                                   : "(none)";
            char ccline[72];
            snprintf(ccline, sizeof(ccline), "CCizer Bank: %-50.50s", bank);
            printBG(col(36), row(11), ccline, COLORS.Text, COLORS.Background, S);
        }
        printBG(col(36),row(13),"Bank",COLORS.Text,COLORS.Background,S);
        printBG(col(58),row(13),"Patch",COLORS.Text,COLORS.Background,S);
        printBG(col(36),row(18),"Default Volume",COLORS.Text,COLORS.Background,S);
        printBG(col(58),row(18),"Default Length",COLORS.Text,COLORS.Background,S);
        printBG(col(36),row(23),"Global Volume",COLORS.Text,COLORS.Background,S);
        printBG(col(58),row(23),"Transpose",COLORS.Text,COLORS.Background,S);
        printBG(col(36),row(28),"Channel",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; 
        updated=2;
        S->unlock();
    }
}
