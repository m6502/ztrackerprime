#include "zt.h"
#include "Button.h"

// Instrument Editor options popup -- opened by pressing F3 again while already
// on the Instrument Editor (mirrors the F2-again CUI_PEParms popup). One
// action: create 16 instruments for the current instrument's MIDI device, one
// per channel 1..16, into the next empty slots. The same action is also bound
// to Alt+G in the Instrument Editor (see CUI_InstEditor.cpp).

void BTNCLK_Create16Channels(UserInterfaceElement *) {
    inst_create_16_channels_for_current_device();  // writes its own statusmsg
    close_popup_window();
    need_refresh++;
}

CUI_IEParms::CUI_IEParms(void) {
    UI = new UserInterface;

    int window_width  = 46 * col(1);
    int window_height = 11 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for (; start_x % 8; start_x--) ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for (; start_y % 8; start_y--) ;

    btn_create16 = new Button;
    UI->add_element(btn_create16, 0);
    btn_create16->x = (start_x / 8) + 14;
    btn_create16->y = (start_y / 8) + 7;
    btn_create16->xsize = 18;
    btn_create16->ysize = 1;
    btn_create16->caption = " Create 16 Channels";
    btn_create16->OnClick = (ActFunc)BTNCLK_Create16Channels;
}

void CUI_IEParms::enter(void) {
    need_refresh = 1;
    cur_state = STATE_IEDIT_WIN;
}

void CUI_IEParms::leave(void) {
    cur_state = STATE_IEDIT;
}

void CUI_IEParms::update(void) {
    int key = 0;

    UI->update();   // drives the Create-16 button (its OnClick closes the popup)

    if (Keys.size()) {
        key = Keys.getkey();
        if (key == SDLK_ESCAPE || key == SDLK_F3 ||
            key == ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT))) {
            close_popup_window();
            fixmouse++;
            need_refresh++;
        } else if (key == SDLK_RETURN) {
            // Enter = confirm, same as clicking the button.
            inst_create_16_channels_for_current_device();
            close_popup_window();
            fixmouse++;
            need_refresh++;
        }
    }
}

void CUI_IEParms::draw(Drawable *S) {
    int window_width  = 46 * col(1);
    int window_height = 11 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for (; start_x % 8; start_x--) ;
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for (; start_y % 8; start_y--) ;

    btn_create16->x = (start_x / 8) + 14;
    btn_create16->y = (start_y / 8) + 7;

    // Resolve the current instrument's MIDI device name for display.
    const char *devname = NULL;
    if (song && cur_inst >= 0 && cur_inst < MAX_INSTS && song->instruments[cur_inst]) {
        unsigned int dev = song->instruments[cur_inst]->midi_device;
        if (dev < MidiOut->numOuputDevices) {
            const char *alias = MidiOut->outputDevices[dev]->alias;
            const char *name  = MidiOut->outputDevices[dev]->szName;
            devname = (alias != NULL && strlen(alias) > 1) ? alias : name;
        }
    }

    if (S->lock() == 0) {
        S->fillRect(start_x, start_y, start_x + window_width, start_y + window_height, COLORS.Background);
        printline(start_x, start_y + window_height - row(1) + 1, 148, window_width / 8, COLORS.Lowlight, S);
        for (int i = start_y / row(1); i < (start_y + window_height) / row(1); i++) {
            printchar(start_x, row(i), 145, COLORS.Highlight, S);
            printchar(start_x + window_width - row(1) + 1, row(i), 146, COLORS.Lowlight, S);
        }
        printline(start_x, start_y, 143, window_width / 8, COLORS.Highlight, S);
        print(col(textcenter("Instrument Editor Options")), start_y + row(2), "Instrument Editor Options", COLORS.Text, S);

        char line[128];
        if (devname) {
            snprintf(line, sizeof(line), "Device: %.30s", devname);
            print(start_x + col(3), start_y + row(4), line, COLORS.Text, S);
            print(start_x + col(3), start_y + row(5), "Fills the next 16 empty slots, ch 1-16.", COLORS.Lowlight, S);
        } else {
            print(start_x + col(3), start_y + row(4), "No MIDI device on this instrument.", COLORS.Text, S);
            print(start_x + col(3), start_y + row(5), "Assign one (Alt+1..0) then reopen.", COLORS.Lowlight, S);
        }
        print(start_x + col(3), start_y + row(9), "Enter / click to create, Esc to cancel.", COLORS.Lowlight, S);

        UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = need_popup_refresh = 0;
        updated++;
    }
}
