#include "zt.h"
#include "Button.h"

// Instrument Editor options popup -- opened by pressing F3 again while already
// on the Instrument Editor (mirrors the F2-again CUI_PEParms popup). Pick a
// MIDI Out device from the in-dialog list, then "Create 16 Channels" generates
// 16 instruments (channels 1..16) for that device, named "<device> Channel
// 01".."<device> Channel 16", into the next empty slots. The same generation
// (using the current instrument's device) is also bound to Alt+G in the
// Instrument Editor (see CUI_InstEditor.cpp).

// Lightweight device picker: lists the open MIDI Out devices and just tracks
// the highlighted one. Unlike MidiOutDeviceSelector it does NOT touch the
// current instrument on select -- the Create button reads the highlight.
class IEGenDeviceList : public ListBox {
    public:
        IEGenDeviceList() {
            empty_message  = "No MIDI Out devices are open";
            is_sorted      = true;
            use_checks     = false;
            use_key_select = false;
            OnChange();
        }
        void OnChange() override {
            clear();
            for (int i = 0; i < MidiOut->GetNumOpenDevs(); i++) {
                int dev = MidiOut->GetDevID(i);
                if (dev < 0 || dev >= MAX_MIDI_DEVS) continue;
                if (MidiOut->outputDevices[dev] == NULL) continue;
                const char *alias = MidiOut->outputDevices[dev]->alias;
                const char *name  = MidiOut->outputDevices[dev]->szName;
                if (name == NULL) name = "";
                const char *disp = (alias != NULL && strlen(alias) > 1) ? alias : name;
                LBNode *p = insertItem((char *)disp);
                p->int_data = dev;
            }
        }
        void OnSelectChange() override {}
};

// The popup is a singleton (one UIP_IEParms), so the button callback reaches
// the live instance through this file-static pointer.
static CUI_IEParms *g_ieparms = NULL;

void BTNCLK_Create16Channels(UserInterfaceElement *) {
    if (g_ieparms) g_ieparms->do_create();
}

CUI_IEParms::CUI_IEParms(void) {
    UI = new UserInterface;
    g_ieparms = this;

    int window_width  = 48 * col(1);
    int window_height = 18 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for (; start_x % 8; start_x--) ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for (; start_y % 8; start_y--) ;

    // Device picker (focused first so arrows work immediately).
    dev_list = new IEGenDeviceList;
    UI->add_element(dev_list, 0);
    dev_list->x     = (start_x / 8) + 4;
    dev_list->y     = (start_y / 8) + 5;
    dev_list->xsize = 40;
    dev_list->ysize = 7;

    // "Create 16 Channels" button -- xsize >= caption length so the right
    // border doesn't clip the last character. Caption padded with spaces.
    btn_create16 = new Button;
    UI->add_element(btn_create16, 1);
    btn_create16->x = (start_x / 8) + 13;
    btn_create16->y = (start_y / 8) + 14;
    btn_create16->xsize = 22;
    btn_create16->ysize = 1;
    btn_create16->caption = " Create 16 Channels ";
    btn_create16->OnClick = (ActFunc)BTNCLK_Create16Channels;
}

void CUI_IEParms::enter(void) {
    need_refresh = 1;
    cur_state = STATE_IEDIT_WIN;
    g_ieparms = this;
    // Refresh the device list (devices may have opened/closed) and pre-select
    // the device currently on the active instrument, if any.
    if (dev_list) {
        dev_list->OnChange();
        if (song && cur_inst >= 0 && cur_inst < MAX_INSTS && song->instruments[cur_inst]) {
            unsigned int cur_dev = song->instruments[cur_inst]->midi_device;
            for (int i = 0; i < dev_list->num_elements; i++) {
                LBNode *n = dev_list->getNode(i);
                if (n && (unsigned int)n->int_data == cur_dev) { dev_list->setCursor(i); break; }
            }
        }
    }
    UI->cur_element = 0;
}

void CUI_IEParms::leave(void) {
    cur_state = STATE_IEDIT;
}

void CUI_IEParms::do_create(void) {
    int idx = dev_list ? dev_list->getCurrentItemIndex() : -1;
    LBNode *n = (dev_list && idx >= 0) ? dev_list->getNode(idx) : NULL;
    if (n) {
        inst_create_16_channels_for_device((unsigned int)n->int_data);  // sets statusmsg
    } else {
        sprintf(szStatmsg, "No MIDI Out device to pick (open one in F12)");
        statusmsg = szStatmsg;
    }
    close_popup_window();
    fixmouse++;
    need_refresh++;
}

void CUI_IEParms::update(void) {
    // Handle close / confirm keys before the widgets, so Enter always means
    // "create" (the listbox would otherwise swallow it) and Esc always closes.
    int peek = Keys.checkkey();
    if (peek == SDLK_ESCAPE || peek == SDLK_F3 ||
        peek == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT))) {
        Keys.getkey();
        close_popup_window();
        fixmouse++;
        need_refresh++;
        return;
    }
    if (peek == SDLK_RETURN) {
        Keys.getkey();
        do_create();
        return;
    }

    // Everything else (arrows move the device list, mouse drives the button
    // whose OnClick calls do_create) goes through the widgets.
    UI->update();
}

void CUI_IEParms::draw(Drawable *S) {
    int window_width  = 48 * col(1);
    int window_height = 18 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for (; start_x % 8; start_x--) ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for (; start_y % 8; start_y--) ;

    dev_list->x     = (start_x / 8) + 4;
    dev_list->y     = (start_y / 8) + 5;
    btn_create16->x = (start_x / 8) + 13;
    btn_create16->y = (start_y / 8) + 14;

    if (S->lock() == 0) {
        S->fillRect(start_x, start_y, start_x + window_width, start_y + window_height, COLORS.Background);
        printline(start_x, start_y + window_height - row(1) + 1, 148, window_width / 8, COLORS.Lowlight, S);
        for (int i = start_y / row(1); i < (start_y + window_height) / row(1); i++) {
            printchar(start_x, row(i), 145, COLORS.Highlight, S);
            printchar(start_x + window_width - row(1) + 1, row(i), 146, COLORS.Lowlight, S);
        }
        printline(start_x, start_y, 143, window_width / 8, COLORS.Highlight, S);
        print(col(textcenter("Create 16 MIDI Channels")), start_y + row(2), "Create 16 MIDI Channels", COLORS.Text, S);
        print(start_x + col(4), start_y + row(4), "Pick a MIDI Out device:", COLORS.Text, S);
        print(start_x + col(4), start_y + row(16),
              "Up/Dn: pick   Enter: create   Esc: cancel", COLORS.Lowlight, S);

        UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = need_popup_refresh = 0;
        updated++;
    }
}
