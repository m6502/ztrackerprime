#include "zt.h"
#include "Button.h"

MidiOutDeviceOpener *midioutdevlist;
MidiInDeviceOpener *midiindevlist;

void BTNCLK_RefreshMidiOutDeviceList(UserInterfaceElement *b) {

    delete MidiOut;
    MidiOut = new midiOut;
    
//    mididevlist->ysize = MidiOut->numMidiDevs-1;
    midioutdevlist->OnChange();
    midioutdevlist->need_redraw++;
    sprintf(szStatmsg,"MIDI-OUT device list refreshed");
    statusmsg = szStatmsg;
    need_refresh++; 
    
}
void BTNCLK_RefreshMidiInDeviceList(UserInterfaceElement *b) {

    delete MidiIn;
    MidiIn = new midiIn;
    
//    mididevlist->ysize = MidiOut->numMidiDevs-1;
    midiindevlist->OnChange();
    midiindevlist->need_redraw++;
    sprintf(szStatmsg,"MIDI-IN device list refreshed");
    statusmsg = szStatmsg;
    need_refresh++; 
    
}

CUI_Sysconfig::CUI_Sysconfig(void) {
    MidiOutDeviceOpener *ml;
    MidiInDeviceOpener *mi;
    ValueSlider *vs;
    CheckBox *cb;
    Button *b;
    SkinSelector *sk;
    TextInput *ti;

    int tabindex=0;
    UI = new UserInterface;
    // MIDI Devices

    // MIDI Devices

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4 + 15;
        vs->y = 14;
        vs->xsize = 15+1;
        vs->ysize = 1;
        vs->value = zt_config_globals.prebuffer_rows;
        vs->min = 1;
        vs->max = 32;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 15;
        cb->y = 16;
        cb->xsize = 5;
        cb->value = &zt_config_globals.auto_send_panic;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 15;
        cb->y = 18;
        cb->xsize = 5;
        cb->value = &zt_config_globals.midi_in_sync;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 15;
        cb->y = 20;
        cb->xsize = 5;
        cb->value = &zt_config_globals.auto_open_midi;
        cb->frame = 1;
    
        cb = new CheckBox;
        UI->add_element(cb,tabindex++); // id:4
        cb->frame = 0;
        cb->x = 4+15;
        cb->y = 22;
        cb->xsize = 5;
        cb->value = &zt_config_globals.full_screen;
        cb->frame = 1;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4+15;
        vs->y = 24;
        vs->xsize = 15+4;
        vs->ysize = 1;
        vs->value = zt_config_globals.key_repeat_time;
        vs->min = 1;
        vs->max = 32;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4+15;
        vs->y = 26;
        vs->xsize = 15+4;
        vs->ysize = 1;
        vs->value = zt_config_globals.key_wait_time;
        vs->min = 1;
        vs->max = 1000;

        sk = new SkinSelector;
        UI->add_element(sk,tabindex++);
        sk->x = 4+35 +10;
        sk->y = 16;
        sk->xsize = 19+4;
        sk->ysize = 10;

        ml = new MidiOutDeviceOpener;
        UI->add_element(ml,tabindex++);
        midioutdevlist = ml;
        ml->x = 4; 
        ml->y = 50 - 16-2; 
        ml->xsize=35;
        ml->ysize = 13;

		//MidiOutputDevice *m;

		//m = (MidiOutputDevice*)(MidiOut->outputDevices[MidiOut->devlist_head->key]);
        vs = new LatencyValueSlider(ml);
        UI->add_element(vs,tabindex++);
        vs->x = 13;
        vs->y = 47;
        vs->xsize = 21;
        vs->ysize = 1;
		
        //vs->value = MidiOut->outputDevices[0]->delay_ticks; //m->delay_ticks;
        vs->min = 0;
        vs->max = 255;
        //ml->update();

        cb = new BankSelectCheckBox(ml);
        UI->add_element(cb,tabindex++);
        cb->frame = 0;
        cb->x = 25;
        cb->y = 49;
        cb->xsize = 5;
        cb->frame = 1;
		//cb->value = &(m->reverse_bank_select);

        ti = new AliasTextInput(ml);
        UI->add_element(ti,tabindex++);
        ti->frame = 1;
        ti->x = 18;
        ti->y = 51;
        ti->xsize=43;
        ti->length=42;

        ml->lvs = vs;  // link midi out list to latency value slider
        ml->bscb = cb; // link midi out list to bank select checkbox
        ml->al = ti;

        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = " Refresh";
        b->x = 4+26;
        b->y = 50 - 16 -2-2;
        b->xsize = 9;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_RefreshMidiOutDeviceList; 

        //Frame *f;
        //f = new Frame;
        //UI->add_gfx(f,0);
        //f->x = 4;
        //f->y = 50-4;
        //f->ysize=7;
        //f->xsize = 35;
        //f->type = 0;
        
        mi = new MidiInDeviceOpener;
        midiindevlist = mi;
        UI->add_element(mi,tabindex++);
        mi->x = 4+37; 
        mi->y = 50 - 16-2; 
        mi->xsize=35;
        mi->ysize = 13;

        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = " Refresh";
        b->x = 4+26+37;
        b->y = 50 - 16 -2-2;
        b->xsize = 9;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_RefreshMidiInDeviceList; 


}

CUI_Sysconfig::~CUI_Sysconfig(void) {
    if (UI) delete UI; UI = NULL;
}

void CUI_Sysconfig::enter(void) {
    need_refresh = 1;
    cur_state = STATE_SYSTEM_CONFIG;
    UI->enter();
}

void CUI_Sysconfig::leave(void) {

}

int attempt_fullscreen_toggle();
extern bool bIsFullscreen;

void CUI_Sysconfig::update() {
    ValueSlider *vs;
    CheckBox *cb;
    int key=0;
    char val[8];

    UI->update();
    vs = (ValueSlider *)UI->get_element(0);
    cb = (CheckBox*)UI->get_element(4);
    if (vs->changed) {
        zt_config_globals.prebuffer_rows = vs->value;
        ztPlayer->prebuffer = (96/song->tpb) * zt_config_globals.prebuffer_rows; // 96ppqn, so look ahead is 1 beat
        sprintf(val,"%d",zt_config_globals.prebuffer_rows);
//        Config.set("prebuffer_rows",&val[0],0);
    }
    int i = 0;
    if (bIsFullscreen) i = 1;
    if ( * cb->value != i) {
        attempt_fullscreen_toggle();
        attempt_fullscreen_toggle();
    }
    if (Keys.size()) {
        key = Keys.getkey();
    }
}

void CUI_Sysconfig::draw(Drawable *S) {
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        status(S);
        printtitle(11,"System Configuration",COLORS.Text,COLORS.Background,S);
        print(row(4),col(14),"     Prebuffer",COLORS.Text,S);
        print(row(4),col(16)," Panic on stop",COLORS.Text,S);
        print(row(4),col(18)," MIDI-IN Slave",COLORS.Text,S);
        print(row(4),col(20),"Auto-open MIDI",COLORS.Text,S);

        print(row(4),col(22),"   Full Screen",COLORS.Text,S);
        //print(row(4),col(24),"  Step Editing",COLORS.Text,S);
        print(row(4),col(24),"    Key Repeat",COLORS.Text,S);
        print(row(4),col(26),"      Key Wait",COLORS.Text,S);
        print(row(4+37+8),col(14),"Skin Selection",COLORS.Text,S);

        print(row(4),col(30),"MIDI Out Device Selection",COLORS.Text,S);
        print(row(4+37),col(30),"MIDI In Device Selection",COLORS.Text,S);

        print(row(5),col(47),"Latency ",COLORS.Text,S);
        print(row(5),col(49),"Reverse Bank Select ",COLORS.Text,S);
        print(row(5),col(51),"Device Alias",COLORS.Text,S);
        
        need_refresh = 0; 
        updated=2;
        S->unlock();
    }
}

