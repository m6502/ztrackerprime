#include "zt.h"
#include "Button.h"

MidiOutDeviceOpener *midioutdevlist;
MidiInDeviceOpener *midiindevlist;
Button *midiout_action_button;
Button *midiin_action_button;
void midi_out_sel(int dev);
void midi_in_sel(int dev);

void BTNCLK_GotoGlobalConfig(UserInterfaceElement*) {
    switch_page(UIP_Config);
    need_refresh++;
    doredraw++;
}

void BTNCLK_RefreshMidiOutDeviceList(UserInterfaceElement*) {
    delete MidiOut;
    MidiOut = new midiOut;
    
//    mididevlist->ysize = MidiOut->numMidiDevs-1;
    midioutdevlist->OnChange();
    midioutdevlist->need_redraw++;
    sprintf(szStatmsg,"MIDI-OUT device list refreshed");
    statusmsg = szStatmsg;
    need_refresh++; 
    
}
void BTNCLK_RefreshMidiInDeviceList(UserInterfaceElement*) {

    delete MidiIn;
    MidiIn = new midiIn;
    
//    mididevlist->ysize = MidiOut->numMidiDevs-1;
    midiindevlist->OnChange();
    midiindevlist->need_redraw++;
    sprintf(szStatmsg,"MIDI-IN device list refreshed");
    statusmsg = szStatmsg;
    need_refresh++; 
    
}

// All captions are exactly 15 chars (= button xsize) and centred so the
// visible text sits in the same column for every device state.
//   "Open device"   (11) -> "  Open device  " (2 + 11 + 2)
//   "Forget device" (13) -> " Forget device " (1 + 13 + 1)
//   "Close device"  (12) -> " Close device  " (1 + 12 + 2)
static const char *get_midi_out_action_caption()
{
    LBNode *selected = midioutdevlist ? midioutdevlist->getNode(midioutdevlist->getCurrentItemIndex()) : NULL;
    if (!selected) {
        return "  Open device  ";
    }
    if (selected->int_data < 0) {
        return " Forget device ";
    }
    if (MidiOut->QueryDevice(selected->int_data)) {
        return " Close device  ";
    }
    return "  Open device  ";
}

static const char *get_midi_in_action_caption()
{
    LBNode *selected = midiindevlist ? midiindevlist->getNode(midiindevlist->getCurrentItemIndex()) : NULL;
    if (!selected) {
        return "  Open device  ";
    }
    if (selected->int_data < 0) {
        return " Forget device ";
    }
    if (MidiIn->QueryDevice(selected->int_data)) {
        return " Close device  ";
    }
    return "  Open device  ";
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
    int base_y = TRACKS_ROW_Y + 3;  // content starts row 14, leaving row 12 for button + gaps on both sides
    UI = new UserInterface;

        // Tab index 0: page-navigation button. Row 12 gives a clean gap
        // above (row 11 empty, title at row 9) and below (row 13 empty,
        // Prebuffer at row 14). UP/DOWN cycle starts here.
        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = "   Go to page 2   ";
        b->xsize = 18;
        b->x = 4;   // align start with "MIDI Out Device Selection" label below
        b->y = 12;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_GotoGlobalConfig;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        // Left column controls at col 20 to match Global Config (Ctrl+F12).
        vs->x = 4 + 16;
        vs->y = base_y;
        vs->xsize = 15+1;
        vs->ysize = 1;
        vs->value = zt_config_globals.prebuffer_rows;
        vs->min = 1;
        vs->max = 32;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 16;
        cb->y = base_y + 2;   // Panic on stop
        cb->xsize = 3;
        cb->value = &zt_config_globals.auto_send_panic;
        cb->frame = 0;

        // MIDI In Sync (a.k.a. "MIDI-IN Slave"). The user-facing label
        // lives in F11 (Songconfig) now, but we keep the element here
        // off-screen so the tabindex/get_element(N) numbering elsewhere
        // doesn't shift. xsize=0 / no_tab_stop hide it from cycling.
        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 0;
        cb->y = 0;
        cb->xsize = 0;
        cb->value = &zt_config_globals.midi_in_sync;
        cb->frame = 0;
        cb->no_tab_stop = 1;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4 + 16;
        cb->y = base_y + 3;   // Auto-open MIDI — adjacent to Panic on stop
        cb->xsize = 3;
        cb->value = &zt_config_globals.auto_open_midi;
        cb->frame = 0;

        cb = new CheckBox;
        UI->add_element(cb,tabindex++); // Full Screen cb — update() reads via get_element(5)
        cb->x = 4+16;
        cb->y = base_y + 4;   // adjacent to Auto-open MIDI; matches F11 tight checkbox stack
        cb->xsize = 3;
        cb->value = &zt_config_globals.full_screen;
        cb->frame = 0;

        // Record Velocity (moved from Ctrl+F12 Global Config).
        cb = new CheckBox;
        UI->add_element(cb,tabindex++);
        cb->x = 4+16;
        cb->y = base_y + 5;
        cb->xsize = 3;
        cb->value = &zt_config_globals.record_velocity;
        cb->frame = 0;

#ifndef DISABLED_CONFIGURATION_VALUES
        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4+16;
        vs->y = base_y + 10;
        vs->xsize = 15+4;
        vs->ysize = 1;
        vs->value = zt_config_globals.key_repeat_time;
        vs->min = 1;
        vs->max = 32;

        vs = new ValueSlider;
        UI->add_element(vs,tabindex++);
        vs->x = 4+16;
        vs->y = base_y + 12;
        vs->xsize = 15+4;
        vs->ysize = 1;
        vs->value = zt_config_globals.key_wait_time;
        vs->min = 1;
        vs->max = 1000;
#endif

        sk = new SkinSelector;
        UI->add_element(sk,tabindex++);
        sk->x = 4+35 +10;
        sk->y = base_y + 1;   // "Skin Selection" label sits on the same row as "Prebuffer"; list top border one row below
        sk->xsize = 19+8;     // ends at col 76, matches MIDI In Device list end
        sk->ysize = 7;   // tight-fit around installed skin count, avoids empty black space

        // MIDI In column lives on the LEFT, MIDI Out on the RIGHT (+37
        // offset). Layout: Refresh (y=28), list (y=30-42), Open device
        // (y=45). MIDI Out also has Latency (y=47), Bank (y=49), Alias
        // (y=51). tabindex order: MIDI In group first, then MIDI Out.
        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = " Refresh";
        b->x = 4+26;
        b->y = 48 - 16 -2-2;
        b->xsize = 9;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_RefreshMidiInDeviceList;

        mi = new MidiInDeviceOpener;
        midiindevlist = mi;
        UI->add_element(mi,tabindex++);
        mi->x = 4;
        mi->y = 48 - 16-2;
        mi->xsize=35;
        mi->ysize = 13;

        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = "  Open device  ";
        b->x = 4+20;       // right edge ends at col 39 (= MIDI In list end)
        b->y = 45;
        b->xsize = 15;     // matches caption length so text never gets truncated
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_ForgetMidiInDevice;
        midiin_action_button = b;

        // MIDI Out column (right side, +37 offset).
        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = " Refresh";
        b->x = 4+26+37;
        b->y = 48 - 16 -2-2;
        b->xsize = 9;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_RefreshMidiOutDeviceList;

        ml = new MidiOutDeviceOpener;
        UI->add_element(ml,tabindex++);
        midioutdevlist = ml;
        ml->x = 4+37;
        ml->y = 48 - 16-2;
        ml->xsize=35;
        ml->ysize = 13;

        b = new Button;
        UI->add_element(b,tabindex++);
        b->caption = "  Open device  ";
        b->x = 4+20+37;   // align left edge with MIDI Out list (4+37=41) shifted by +20 chars to space the button
        b->y = 45;
        b->xsize = 15;
        b->ysize = 1;
        b->OnClick = (ActFunc)BTNCLK_ForgetMidiOutDevice;
        midiout_action_button = b;

        // Latency / Bank / Alias share the right half (col 41..76, matching
        // the MIDI Out list / Open device button width). Labels start at
        // col 41 (left-aligned with list); values follow each label and
        // extend to col 76.
        vs = new LatencyValueSlider(ml);
        UI->add_element(vs,tabindex++);
        vs->x = 49;        // after "Latency " label (cols 41..48)
        vs->y = 47;
        vs->xsize = 27;    // ends col 76 — matches list right edge
        vs->ysize = 1;
        vs->min = 0;
        vs->max = 255;

        cb = new BankSelectCheckBox(ml);
        UI->add_element(cb,tabindex++);
        cb->x = 61;        // after "Reverse Bank Select " label (cols 41..60)
        cb->y = 49;
        cb->xsize = 3;
        cb->frame = 0;

        ti = new AliasTextInput(ml);
        UI->add_element(ti,tabindex++);
        ti->frame = 1;
        ti->x = 54;        // after "Device Alias " label (cols 41..53)
        ti->y = 51;
        ti->xsize = 22;    // ends col 76 — matches list right edge
        ti->length = 41;   // buffer still fits a long alias; field scrolls horizontally

        ml->lvs = vs;  // link midi out list to latency value slider
        ml->bscb = cb; // link midi out list to bank select checkbox
        ml->al = ti;

        // CCizer folder picker. Appended at the END of tabindex so the
        // earlier UI->get_element(N) literal indices in update() (1=Prebuffer,
        // 5=Full Screen, etc.) keep working. Label drawn in draw() below.
        ti = new TextInput;
        UI->add_element(ti, tabindex++);
        ti->frame  = 1;
        ti->x      = 4 + 16;                          // matches left-column controls
        ti->y      = base_y + 7;                      // gap row 6 between Record Velocity (+5) and this
        ti->xsize  = 56;                              // ends ~col 76, matches MIDI Out list right edge
        ti->length = MAX_PATH;
        ti->str    = (unsigned char*)zt_config_globals.ccizer_folder;

        // SysEx folder picker (audit L13). Same row layout, one row
        // below the CCizer folder so the two paths sit together as a
        // "MIDI data folders" group. Empty value falls back through
        // CUI_SysExLibrarian::resolve_folder()'s default cascade.
        ti = new TextInput;
        UI->add_element(ti, tabindex++);
        ti->frame  = 1;
        ti->x      = 4 + 16;
        ti->y      = base_y + 8;
        ti->xsize  = 56;
        ti->length = MAX_PATH;
        ti->str    = (unsigned char*)zt_config_globals.syx_folder;
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
    char val[8];

    UI->update();
    sync_midi_action_buttons();
    // Indices match the tabindex order set in the constructor: 0=button,
    // 1=Prebuffer slider, 2=Panic, 3=MIDI-IN Slave, 4=Auto-open MIDI,
    // 5=Full Screen.
    vs = (ValueSlider *)UI->get_element(1);
    cb = (CheckBox*)UI->get_element(5);
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
    }
    if (Keys.size()) {
        Keys.getkey();
    }
}

void CUI_Sysconfig::draw(Drawable *S) {
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        status(S);
        printtitle(PAGE_TITLE_ROW_Y,"System Configuration (F12)",COLORS.Text,COLORS.Background,S);
        // Labels shifted +3 rows from legacy position: row 12 holds the
        // "Go to page 2" button, rows 11 and 13 are empty gaps.
        // Labels right-align so text ends at col 18 (1-char gap before
        // col-20 controls). Matches Global Config (Ctrl+F12).
        print(row(4),col(TRACKS_ROW_Y+3),"      Prebuffer",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+5),"  Panic on stop",COLORS.Text,S);
        // "MIDI-IN Slave" label intentionally omitted — see F11 (Songconfig).
        print(row(4),col(TRACKS_ROW_Y+6)," Auto-open MIDI",COLORS.Text,S);

        print(row(4),col(TRACKS_ROW_Y+7),"    Full Screen",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+8),"Record Velocity",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+10),"  CCizer Folder",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+11),"  SysEx Folder", COLORS.Text,S);
#ifndef DISABLED_CONFIGURATION_VALUES
        print(row(4),col(TRACKS_ROW_Y+13),"     Key Repeat",COLORS.Text,S);
        print(row(4),col(TRACKS_ROW_Y+15),"       Key Wait",COLORS.Text,S);
#endif
        print(row(4+37+8),col(TRACKS_ROW_Y+3),"Skin Selection",COLORS.Text,S);

        // MIDI In on the LEFT (col 4), MIDI Out on the RIGHT (col 4+37).
        print(row(4),col(28),"MIDI In Device Selection",COLORS.Text,S);
        print(row(4+37),col(28),"MIDI Out Device Selection",COLORS.Text,S);

        // Labels left-aligned to col 41 (= MIDI Out list left edge).
        print(row(4+37),col(47),"Latency",COLORS.Text,S);
        print(row(4+37),col(49),"Reverse Bank Select",COLORS.Text,S);
        print(row(4+37),col(51),"Device Alias",COLORS.Text,S);
        
        need_refresh = 0;
        updated=2;
        S->unlock();
    }
}

