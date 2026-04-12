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

    
    
