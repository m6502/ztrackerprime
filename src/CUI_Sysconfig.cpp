#include "zt.h"
#include "Button.h"

MidiOutDeviceOpener *midioutdevlist;
MidiInDeviceOpener *midiindevlist;
Button *midiout_action_button;
Button *midiin_action_button;
void midi_out_sel(int dev);
void midi_in_sel(int dev);

void BTNCLK_GotoGlobalConfig(UserInterfaceElement *b) {
    (void)b;
    switch_page(UIP_Config);
    need_refresh++;
    doredraw++;
}

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

static const char *get_midi_out_action_caption()
{
    LBNode *selected = midioutdevlist ? midioutdevlist->getNode(midioutdevlist->getCurrentItemIndex()) : NULL;
    if (!selected) {
        return " Open device   ";
    }
    if (selected->int_data < 0) {
        return " Forget device ";
    }
    if (MidiOut->QueryDevice(selected->int_data)) {
        return " Close device  ";
    }
    return " Open device   ";
}

static const char *get_midi_in_action_caption()
{
    LBNode *selected = midiindevlist ? midiindevlist->getNode(midiindevlist->getCurrentItemIndex()) : NULL;
    if (!selected) {
        return " Open device   ";
    }
    if (selected->int_data < 0) {
        return " Forget device ";
    }
    if (MidiIn->QueryDevice(selected->int_data)) {
        return " Close device  ";
    }
    return " Open device   ";
}

static void sync_midi_action_buttons()
{
    if (midiout_action_button) {
        const char *next = get_midi_out_action_caption();
        if (!midiout_action_button->caption || strcmp(midiout_action_button->caption, next) != 0) {
            midiout_action_button->caption = (char *)next;
            midiout_action_button->need_redraw++;
            need_refresh++;
        }
    }
    if (midiin_action_button) {
        const char *next = get_midi_in_action_caption();
        if (!midiin_action_button->caption || strcmp(midiin_action_button->caption, next) != 0) {
            midiin_action_button->caption = (char *)next;
            midiin_action_button->need_redraw++;
            need_refresh++;
        }
    }
}

static void restore_list_selection(ListBox *listbox, const char *caption, int fallback_index)
{
    if (!listbox || listbox->num_elements <= 0) {
        return;
    }

    int target = -1;
    if (caption && caption[0]) {
        target = listbox->findItem((char *)caption);
    }
    if (target < 0) {
        target = fallback_index;
    }
    if (target < 0) {
        target = 0;
    }
    if (target >= listbox->num_elements) {
        target = listbox->num_elements - 1;
    }
    listbox->setCursor(target);
}

static void encode_dev_key(const char *str, char w[256])
{
    const char *r = str ? str : "";
    char *q = w;
    for(; *r != '\0'; r++, q++) {
        if((*r >= 'a' && *r <= 'z') || (*r >= 'A' && *r <= 'Z') || (*r >= '0' && *r <= '9')) {
            *q = *r;
        } else {
            *q = '_';
        }
    }
    *q = '\0';
}

static void forget_device_entries(const char *name, int is_output)
{
    if (!name || !name[0]) {
        return;
    }

    conf device_config((char *)"devices.conf");
    char key[256];
    char enc[256];
    encode_dev_key(name, enc);

    snprintf(key, sizeof(key), "latency_%s", enc);
    device_config.remove(key);
    snprintf(key, sizeof(key), "bank_%s", enc);
    device_config.remove(key);
    snprintf(key, sizeof(key), "alias_%s", enc);
    device_config.remove(key);

    const char *known_prefix = is_output ? "known_out_device_" : "known_in_device_";
    const char *open_prefix = is_output ? "open_out_device_" : "open_in_device_";
    for (int i = 0; i < MAX_MIDI_DEVS; ++i) {
        snprintf(key, sizeof(key), "%s%d", known_prefix, i);
        char *v = device_config.get(key);
        if (v && zcmp(v, (char *)name)) {
            device_config.remove(key);
        }
        snprintf(key, sizeof(key), "%s%d", open_prefix, i);
        v = device_config.get(key);
        if (v && zcmp(v, (char *)name)) {
            device_config.remove(key);
        }
    }

    device_config.save((char *)"devices.conf");
}

void BTNCLK_ForgetMidiOutDevice(UserInterfaceElement *b) {
    (void)b;
    LBNode *selected = midioutdevlist ? midioutdevlist->getNode(midioutdevlist->getCurrentItemIndex()) : NULL;
    if (!selected || !selected->caption) {
        return;
    }

    char selected_name[256];
    strncpy(selected_name, selected->caption, sizeof(selected_name) - 1);
    selected_name[sizeof(selected_name) - 1] = '\0';
    const int selected_index = midioutdevlist->getCurrentItemIndex();
    bool forgot = false;

    const int dev_id = selected->int_data;
    if (dev_id < 0) {
        forget_device_entries(selected->caption, 1);
        snprintf(szStatmsg, sizeof(szStatmsg), "Forgot MIDI-OUT device: %s", selected->caption);
        forgot = true;
    } else if (MidiOut->QueryDevice(dev_id)) {
        midi_out_sel(dev_id);
        snprintf(szStatmsg, sizeof(szStatmsg), "Closed MIDI-OUT device: %s", selected->caption);
    } else {
        midi_out_sel(dev_id);
        snprintf(szStatmsg, sizeof(szStatmsg), "Opened MIDI-OUT device: %s", selected->caption);
    }

    midioutdevlist->OnChange();
    if (forgot) {
        // Keep same index -> next row after deletion; clamp chooses previous if forgotten row was last.
        restore_list_selection(midioutdevlist, NULL, selected_index);
    } else {
        restore_list_selection(midioutdevlist, selected_name, selected_index);
    }
    midioutdevlist->need_redraw++;
    statusmsg = szStatmsg;
    status_change = 1;
    sync_midi_action_buttons();
    need_refresh++;
}

void BTNCLK_ForgetMidiInDevice(UserInterfaceElement *b) {
    (void)b;
    LBNode *selected = midiindevlist ? midiindevlist->getNode(midiindevlist->getCurrentItemIndex()) : NULL;
    if (!selected || !selected->caption) {
        return;
    }

    char selected_name[256];
    strncpy(selected_name, selected->caption, sizeof(selected_name) - 1);
    selected_name[sizeof(selected_name) - 1] = '\0';
    const int selected_index = midiindevlist->getCurrentItemIndex();
    bool forgot = false;

    const int dev_id = selected->int_data;
    if (dev_id < 0) {
        forget_device_entries(selected->caption, 0);
        snprintf(szStatmsg, sizeof(szStatmsg), "Forgot MIDI-IN device: %s", selected->caption);
        forgot = true;
    } else if (MidiIn->QueryDevice(dev_id)) {
        midi_in_sel(dev_id);
        snprintf(szStatmsg, sizeof(szStatmsg), "Closed MIDI-IN device: %s", selected->caption);
    } else {
        midi_in_sel(dev_id);
        snprintf(szStatmsg, sizeof(szStatmsg), "Opened MIDI-IN device: %s", selected->caption);
    }

    midiindevlist->OnChange();
    if (forgot) {
        restore_list_selection(midiindevlist, NULL, selected_index);
    } else {
        restore_list_selection(midiindevlist, selected_name, selected_index);
    }
    midiindevlist->need_redraw++;
    statusmsg = szStatmsg;
    status_change = 1;
    sync_midi_action_buttons();
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

#ifndef DISABLED_CONFIGURATION_VALUES
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
#endif

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
        vs->y = 49;
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
        cb->y = 51;
        cb->xsize = 5;
        cb->frame = 1;
		//cb->value = &(m->reverse_bank_select);

        ti = new AliasTextInput(ml);
        UI->add_element(ti,tabindex++);
        ti->frame = 1;
        ti->x = 18;
        ti->y = 53;
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

        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = " Open device   ";
        b->x = 4+21;
        b->y = 47;
        b->xsize = 15;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_ForgetMidiOutDevice;
        midiout_action_button = b;

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

        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = " Open device   ";
        b->x = 4+21+37;
        b->y = 47;
        b->xsize = 15;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_ForgetMidiInDevice;
        midiin_action_button = b;

        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = "   Go to page 2   ";
        b->xsize = 18;
        b->x = 2;
        b->y = 11;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_GotoGlobalConfig;

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
    sync_midi_action_buttons();
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
        printtitle(PAGE_TITLE_ROW_Y,"System Configuration (F12)",COLORS.Text,COLORS.Background,S);
        print(row(4),col(14),"     Prebuffer",COLORS.Text,S);
        print(row(4),col(16)," Panic on stop",COLORS.Text,S);
        print(row(4),col(18)," MIDI-IN Slave",COLORS.Text,S);
        print(row(4),col(20),"Auto-open MIDI",COLORS.Text,S);

        print(row(4),col(22),"   Full Screen",COLORS.Text,S);
        //print(row(4),col(24),"  Step Editing",COLORS.Text,S);
#ifndef DISABLED_CONFIGURATION_VALUES
        print(row(4),col(24),"    Key Repeat",COLORS.Text,S);
        print(row(4),col(26),"      Key Wait",COLORS.Text,S);
#endif
        print(row(4+37+8),col(14),"Skin Selection",COLORS.Text,S);

        print(row(4),col(30),"MIDI Out Device Selection",COLORS.Text,S);
        print(row(4+37),col(30),"MIDI In Device Selection",COLORS.Text,S);

        print(row(5),col(49),"Latency ",COLORS.Text,S);
        print(row(5),col(51),"Reverse Bank Select ",COLORS.Text,S);
        print(row(5),col(53),"Device Alias",COLORS.Text,S);
        
        need_refresh = 0; 
        updated=2;
        S->unlock();
    }
}

