#include "zt.h"
#include "InstEditor.h"
#include "Button.h"


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
void BTNCLK_InitInstruments(UserInterfaceElement *b) {
    InitInstruments();
    sprintf(szStatmsg,"All instruments updated on all devices");
    statusmsg = szStatmsg;
    need_refresh++; 
}
void BTNCLK_InitInstrumentsOne(UserInterfaceElement *b) {
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
    ie->y = 13;
    ie->xsize = 29;

    ie->ysize = MAX_INSTS ;

    ///////////////////////////////////////////////////
    // HERE we changed all the slider object constructors to have a (1) argument (for focus) -nic

    vso = new ValueSliderOFF(1); // Bank (ID: 1)
    UI->add_element(vso,tabindex++);
    vso->x = 36;    vso->y = 16;    vso->xsize = 14;    vso->ysize = 1; vso->min = -1;  vso->max = 0x3fff;    vso->value = -1; 
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=13; fm->xsize = 22; fm->ysize = 5; fm->type = 0;



    vso = new ValueSliderOFF(1); // Patch (ID: 2)
    UI->add_element(vso,tabindex++);
    vso->x = 58;    vso->y = 16;    vso->xsize = 16;    vso->ysize = 1; vso->min = -1;  vso->max = 127; vso->value = -1;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=13; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vs = new ValueSlider(1); // Default Volume (ID: 3)
    UI->add_element(vs,tabindex++);
    vs->x = 36; vs->y = 21; vs->xsize = 16; vs->ysize = 1;  vs->min = 0;    vs->max = 0x7f; vs->value = 0x7f;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=18; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vsd = new ValueSliderDL(1); // Default Length (ID: 4)
    UI->add_element(vsd,tabindex++);
    vsd->x = 58;    vsd->y = 21;    vsd->xsize = 16;    vsd->ysize = 1; vsd->min = 1;   vsd->max = 1000;    vsd->value = 0;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=18; fm->xsize = 22; fm->ysize = 5; fm->type = 0;

    

    
    vs = new ValueSlider(1); // Global Volume (ID: 5)
    UI->add_element(vs,tabindex++);
    vs->x = 36; vs->y = 26; vs->xsize = 16; vs->ysize = 1;  vs->min = 0;    vs->max = 0x7f; vs->value = 0x7f;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=23; fm->xsize = 22; fm->ysize = 5; fm->type = 0;
    



    vs = new ValueSlider(1); // Transpose (ID: 6)
    UI->add_element(vs,tabindex++);
    vs->x = 58; vs->y = 26; vs->xsize = 16; vs->ysize = 1;  vs->min = -127; vs->max = 127;  vs->value = 0;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=23; fm->xsize = 22; fm->ysize = 5; fm->type = 0;




    vs = new ValueSlider(1); // Channel (ID: 7)
    UI->add_element(vs,tabindex++);
    vs->x = 36; vs->y = 31; vs->xsize = 16; vs->ysize = 1;  vs->min = 1;    vs->max = 16;   vs->value = 0;
    fm = new Frame;
    UI->add_gfx(fm,gfxindex++);
    fm->x=35; fm->y=28; fm->xsize = 22; fm->ysize = 7; fm->type = 0;
    
    ////////////////////////////////////////////////////////////////////////////

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 58;
    b->y = 29;
    b->xsize = 20;
    b->ysize = 1;
    b->caption = "  Unmute Retrigger";
    b->OnClick = (ActFunc)BTNCLK_ToggleNoteRetrig;

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 58;
    b->y = 31;
    b->xsize = 20;
    b->ysize = 1;
    b->caption = "   Channel Volume";
    b->OnClick = (ActFunc)BTNCLK_ToggleChanVol; 

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 58;
    b->y = 33;
    b->xsize = 20;
    b->ysize = 1;
    b->caption = "    Tracker Mode";
    b->OnClick = (ActFunc)BTNCLK_ToggleTrackerMode; 
    

    trackerModeButton = b;

    fm = new Frame;  //frame for buttons
    UI->add_gfx(fm,gfxindex++);
    fm->x=57; fm->y=28; fm->xsize = 22; fm->ysize = 7; fm->type = 0;





    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 35;
    b->y = 36;
    b->xsize = 15;
    b->ysize = 1;
    b->caption = " Update Device";
    b->OnClick = (ActFunc)BTNCLK_InitInstrumentsOne;

    
    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 35+16;
    b->y = 36;
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
    mds->y=38;
    mds->xsize = 44;
    mds->ysize = 13;
    
    reset = 0;
}

CUI_InstEditor::~CUI_InstEditor(void) {
    if (UI) delete UI; UI= NULL;
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
    int set_note = 0xff;
    int act = 0;
    InstEditor *ie;

    UI->update();

    ie = (InstEditor *)UI->get_element(0);

    
    
    // <Manu> Refrescamos la pantalla cuando cambia la línea -----------

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
            kstate = Keys.getstate();
        }

        /*if (kstate & KS_ALT && kstate & KS_CTRL) {
            switch(key) {
                case DIK_T: break;
            }
        }*/

        switch(kstate) {
            case KS_CTRL:
                switch(key) {
                    case DIK_1: song->instruments[cur_inst]->channel = 0; act++; break;
                    case DIK_2: song->instruments[cur_inst]->channel = 1; act++; break;
                    case DIK_3: song->instruments[cur_inst]->channel = 2; act++; break;
                    case DIK_4: song->instruments[cur_inst]->channel = 3; act++; break;
                    case DIK_5: song->instruments[cur_inst]->channel = 4; act++; break;
                    case DIK_6: song->instruments[cur_inst]->channel = 5; act++; break;
                    case DIK_7: song->instruments[cur_inst]->channel = 6; act++; break;
                    case DIK_8: song->instruments[cur_inst]->channel = 7; act++; break;
                    case DIK_9: song->instruments[cur_inst]->channel = 8; act++; break;
                    case DIK_0: song->instruments[cur_inst]->channel = 9; act++; break;
                }
                break;
            case KS_ALT:
                    switch(key) {
                    case DIK_1: dev_sel(0,mds); act++; break;
                    case DIK_2: dev_sel(1,mds); act++; break;
                    case DIK_3: dev_sel(2,mds); act++; break;
                    case DIK_4: dev_sel(3,mds); act++; break;
                    case DIK_5: dev_sel(4,mds); act++; break;
                    case DIK_6: dev_sel(5,mds); act++; break;
                    case DIK_7: dev_sel(6,mds); act++; break;
                    case DIK_8: dev_sel(7,mds); act++; break;
                    case DIK_9: dev_sel(8,mds); act++; break;
                    case DIK_0: dev_sel(9,mds); act++; break;
                    case DIK_T: BTNCLK_ToggleTrackerMode(this->trackerModeButton); act++; break;
                    }
                    break;
            case KS_NO_SHIFT_KEYS:
                switch(key) {
                    case DIK_MULTIPLY:
                        if (base_octave<9) {
                            base_octave++;
                            need_refresh++; 
                        }
                        break;
                    case DIK_DIVIDE:
                        if (base_octave>0) {
                            base_octave--;
                            need_refresh++; 
                        }
                        break;
                    case DIK_PGUP:
                        ie->cursor--;
                        if (ie->cursor<0)
                            ie->list_start--;
                        act++; break;
                    case DIK_PGDN: 
                        ie->cursor++;
                        if (ie->cursor>=ie->ysize)
                            ie->list_start++;
                        act++; break;

                        case DIK_Q: set_note = 12*base_octave;         break;
                        case DIK_2: set_note = (12*base_octave)+1;     break;
                        case DIK_W: set_note = (12*base_octave)+2;     break;
                        case DIK_3: set_note = (12*base_octave)+3;     break;
                        case DIK_E: set_note = (12*base_octave)+4;     break;
                        case DIK_R: set_note = (12*base_octave)+5;     break;
                        case DIK_5: set_note = (12*base_octave)+6;     break;
                        case DIK_T: set_note = (12*base_octave)+7;     break;
                        case DIK_6: set_note = (12*base_octave)+8;     break;
                        case DIK_Y: set_note = (12*base_octave)+9;     break;
                        case DIK_7: set_note = (12*base_octave)+10;    break;
                        case DIK_U: set_note = (12*base_octave)+11;    break;
                        case DIK_I: set_note = (12*base_octave)+12;    break;
                        case DIK_9: set_note = (12*base_octave)+1+12;  break;
                        case DIK_O: set_note = (12*base_octave)+2+12;  break;
                        case DIK_0: set_note = (12*base_octave)+3+12;  break;
                        case DIK_P: set_note = (12*base_octave)+4+12;  break;
                        case SDLK_LEFTBRACKET: set_note = (12*base_octave)+5+12;  break;
                        case SDLK_RIGHTBRACKET: set_note = (12*base_octave)+6+12;  break;

                        case DIK_Z: set_note = 12*(base_octave-1);     break;
                        case DIK_S: set_note = (12*(base_octave-1))+1; break;
                        case DIK_X: set_note = (12*(base_octave-1))+2; break;
                        case DIK_D: set_note =(12*(base_octave-1))+3;  break;
                        case DIK_C: set_note =(12*(base_octave-1))+4;  break;
                        case DIK_V: set_note = (12*(base_octave-1))+5; break;
                        case DIK_G: set_note = (12*(base_octave-1))+6; break;
                        case DIK_B: set_note = (12*(base_octave-1))+7; break;
                        case DIK_H: set_note = (12*(base_octave-1))+8; break;
                        case DIK_N: set_note = (12*(base_octave-1))+9; break;
                        case DIK_J: set_note = (12*(base_octave-1))+10;break; 
                        case DIK_M: set_note = (12*(base_octave-1))+11;break;

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
                MidiOut->noteOff(song->instruments[cur_inst]->midi_device,jazz[key].note,jazz[key].chan,0x0,0);
                jazz[key].note = 0x80;
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
                jazz[key].note = set_note;
                jazz[key].chan = song->instruments[cur_inst]->channel;
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

    ie->ysize = 39 + ((INTERNAL_RESOLUTION_Y-480)/8);
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
        printtitle(PAGE_TITLE_ROW_Y,"Instrument Editor",COLORS.Text,COLORS.Background,S);
        printBG(col(36),row(14),"Bank",COLORS.Text,COLORS.Background,S);
        printBG(col(58),row(14),"Patch",COLORS.Text,COLORS.Background,S);
        printBG(col(36),row(19),"Default Volume",COLORS.Text,COLORS.Background,S);
        printBG(col(58),row(19),"Default Length",COLORS.Text,COLORS.Background,S);
        printBG(col(36),row(24),"Global Volume",COLORS.Text,COLORS.Background,S);
        printBG(col(58),row(24),"Transpose",COLORS.Text,COLORS.Background,S);
        printBG(col(36),row(29),"Channel",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; 
        updated=2;
        S->unlock();
    }
}