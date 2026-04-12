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
    int base_y = TRACKS_ROW_Y;  // Align with Pattern Editor content start
    UI = new UserInterface;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4 + 15;
        vs->y = base_y;
        vs->xsize = 15+1;
        vs->ysize = 1;
        vs->value = zt_config_globals.prebuffer_rows;
        vs->min = 1;
        vs->max = 32;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 15;
        cb->y = base_y + 2;
        cb->xsize = 5;
        cb->value = &zt_config_globals.auto_send_panic;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 15;
        cb->y = base_y + 4;
        cb->xsize = 5;
        cb->value = &zt_config_globals.midi_in_sync;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 15;
        cb->y = base_y + 6;
        cb->xsize = 5;
        cb->value = &zt_config_globals.auto_open_midi;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++); // id:4
        cb->frame = 0;
        cb->x = 4+15;
        cb->y = base_y + 8;
        cb->xsize = 5;
        cb->value = &zt_config_globals.full_screen;
        cb->frame = 1;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4+15;
        vs->y = base_y + 10;
        vs->xsize = 15+4;
        vs->ysize = 1;
        vs->value = zt_config_globals.key_repeat_time;
        vs->min = 1;
        vs->max = 32;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4+15;
        vs->y = base_y + 12;
        vs->xsize = 15+4;
        vs->ysize = 1;
        vs->value = zt_config_globals.key_wait_time;
        vs->min = 1;
        vs->max = 1000;

        // Right column: additional settings
        int rx = 4+35+3;  // right column X

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++); // zoom
        vs->x = rx+15;
        vs->y = base_y;
        vs->xsize = 10;
        vs->ysize = 1;
        vs->value = (int)zt_config_globals.zoom;
        vs->min = 1;
        vs->max = 8;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++); // highlight
        vs->x = rx+15;
        vs->y = base_y + 2;
        vs->xsize = 10;
        vs->ysize = 1;
        vs->value = zt_config_globals.highlight_increment;
        vs->min = 1;
        vs->max = 32;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++); // lowlight
        vs->x = rx+15;
        vs->y = base_y + 4;
        vs->xsize = 10;
        vs->ysize = 1;
        vs->value = zt_config_globals.lowlight_increment;
        vs->min = 1;
        vs->max = 32;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++); // default pattern length
        vs->x = rx+15;
        vs->y = base_y + 6;
        vs->xsize = 10;
        vs->ysize = 1;
        vs->value = zt_config_globals.pattern_length;
        vs->min = 32;
        vs->max = 256;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++); // record velocity
        cb->x = rx+15;
        cb->y = base_y + 8;
        cb->xsize = 5;
        cb->value = &zt_config_globals.record_velocity;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++); // centered editing
        cb->x = rx+15;
        cb->y = base_y + 10;
        cb->xsize = 5;
        cb->value = &zt_config_globals.centered_editing;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++); // step editing
        cb->x = rx+15;
        cb->y = base_y + 12;
        cb->xsize = 5;
        cb->value = &zt_config_globals.step_editing;
        cb->frame = 1;

        // Skin selector — right side, below settings, above MIDI In
        int midi_y = base_y + 15;
        sk = new SkinSelector;
        UI->add_element(sk,tabindex++);
        sk->x = rx + 2;
        sk->y = midi_y;
        sk->xsize = CHARS_X - rx - 4;
        sk->ysize = CHARS_Y - midi_y - 10;

        // MIDI Out device list — left side
        ml = new MidiOutDeviceOpener;
        UI->add_element(ml,tabindex++);
        midioutdevlist = ml;
        ml->x = 4;
        ml->y = midi_y;
        ml->xsize = rx - 4;
        ml->ysize = CHARS_Y - midi_y - 10;

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
        mi->x = 4;
        mi->y = midi_y + CHARS_Y - midi_y - 8;
        mi->xsize = rx - 4;
        mi->ysize = 6;

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
    vs = (ValueSlider *)UI->get_element(0); // prebuffer
    if (vs->changed) {
        zt_config_globals.prebuffer_rows = vs->value;
        ztPlayer->prebuffer = (96/song->tpb) * zt_config_globals.prebuffer_rows;
    }

    // Key repeat/wait
    vs = (ValueSlider *)UI->get_element(5);
    if (vs->changed) zt_config_globals.key_repeat_time = vs->value;
    vs = (ValueSlider *)UI->get_element(6);
    if (vs->changed) zt_config_globals.key_wait_time = vs->value;

    // Right column: zoom, highlight, lowlight, pattern length
    vs = (ValueSlider *)UI->get_element(7);
    if (vs->changed) zt_config_globals.zoom = (float)vs->value;
    vs = (ValueSlider *)UI->get_element(8);
    if (vs->changed) zt_config_globals.highlight_increment = vs->value;
    vs = (ValueSlider *)UI->get_element(9);
    if (vs->changed) zt_config_globals.lowlight_increment = vs->value;
    vs = (ValueSlider *)UI->get_element(10);
    if (vs->changed) zt_config_globals.pattern_length = vs->value;

    // Fullscreen toggle
    cb = (CheckBox*)UI->get_element(4);
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
        printtitle(PAGE_TITLE_ROW_Y,"System Configuration (F12)",COLORS.Text,COLORS.Background,S);
        print(row(4),col(TRACKS_ROW_Y),"     Prebuffer",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+2)," Panic on stop",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+4)," MIDI-IN Slave",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+6),"Auto-open MIDI",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+8),"   Full Screen",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+10),"    Key Repeat",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+12),"      Key Wait",COLORS.Text,S);

        // Right column labels
        int rx = 4+35+3;
        print(row(rx),col(TRACKS_ROW_Y),"          Zoom",COLORS.Text,S);
        print(row(rx),col(TRACKS_ROW_Y+2),"     Highlight",COLORS.Text,S);
        print(row(rx),col(TRACKS_ROW_Y+4),"      Lowlight",COLORS.Text,S);
        print(row(rx),col(TRACKS_ROW_Y+6),"   Pattern Len",COLORS.Text,S);
        print(row(rx),col(TRACKS_ROW_Y+8),"Record Velocity",COLORS.Text,S);
        print(row(rx),col(TRACKS_ROW_Y+10),"Centered Edit",COLORS.Text,S);
        print(row(rx),col(TRACKS_ROW_Y+12),"  Step Editing",COLORS.Text,S);
        int midi_y = TRACKS_ROW_Y + 15;
        print(row(rx+2),col(midi_y - 1),"Skin Selection",COLORS.Text,S);

        print(row(4),col(midi_y - 1),"MIDI Out Device Selection",COLORS.Text,S);

        print(row(5),col(CHARS_Y - 7),"Latency ",COLORS.Text,S);
        print(row(5),col(CHARS_Y - 5),"Reverse Bank Select ",COLORS.Text,S);
        print(row(5),col(CHARS_Y - 3),"Device Alias",COLORS.Text,S);
        
        need_refresh = 0; 
        updated=2;
        S->unlock();
    }
}

