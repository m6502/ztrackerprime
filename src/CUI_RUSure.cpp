#include "zt.h"
#include "Button.h"

void BTNCLK_rusure_no(void) {
    Keys.flush();
    close_popup_window();
    fixmouse++;
    need_refresh++;
}

void BTNCLK_rusure_yes(void) {
    Keys.flush();
    close_popup_window();
    fixmouse++;
    need_refresh++;
    if (UIP_RUSure->OnYes)
        UIP_RUSure->OnYes();
}


CUI_RUSure::CUI_RUSure(void) {

    int tabindex=0;
    UI = new UserInterface;

    int window_width = 40 * col(1);
    int window_height = 12 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--);
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--);


    button_yes = new Button;
    UI->add_element(button_yes,tabindex++);
    button_yes->x = (start_x / 8) + 12;
    button_yes->y = (start_y + (window_height / 2) ) / 8 +1; //23;
    button_yes->xsize = 7;
    button_yes->ysize = 1;
    button_yes->caption = "  Yes";
    button_yes->OnClick = (ActFunc)BTNCLK_rusure_yes;

    button_no = new Button;
    UI->add_element(button_no,tabindex++);
    button_no->x = (start_x / 8) + 12 + 7 + 2;
    button_no->y = (start_y + (window_height / 2) ) / 8 +1;
    button_no->xsize = 7;
    button_no->ysize = 1;
    button_no->caption = "  No";
    button_no->OnClick = (ActFunc)BTNCLK_rusure_no;
}

CUI_RUSure::~CUI_RUSure(void) {
    if (UI) delete UI; UI = NULL;
}

void CUI_RUSure::enter(void) {
    need_refresh = 1;
    UI->cur_element = 1;
}

void CUI_RUSure::leave(void) {
}

void CUI_RUSure::update() {
    int key=0;
//  act = Keys.size(); 
//  key = Keys.checkkey();
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
        switch(key) {
            case DIK_LEFT:
            case DIK_RIGHT:
                if (UI->cur_element) UI->cur_element = 0;
                    else UI->cur_element = 1;
                need_refresh++; fixmouse++;
                UI->full_refresh();
                break;
            case DIK_Y:
                //make_new_song();
                BTNCLK_rusure_yes();
                break;
            case DIK_N:
            case DIK_ESCAPE:
                BTNCLK_rusure_no();
                break;
        }
    }   
}

void CUI_RUSure::draw(Drawable *S) {
    int i;
    int window_width = 20 * col(1);
    int window_height = 6 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--);
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--);


    button_yes->x = (start_x / 8) + 2 ;
    button_yes->y = (start_y + (window_height / 2) ) / 8 +1; //23;
    button_no->x = (start_x / 8) + 2 + 7 + 2;
    button_no->y = (start_y + (window_height / 2) ) / 8 +1;

    if (S->lock()==0) {
        S->fillRect(start_x,start_y,start_x + window_width,start_y + window_height,COLORS.Background);
        printline(start_x,start_y + window_height - row(1) + 1,148,window_width / 8,COLORS.Lowlight,S);
        for (i= start_y / row(1);i< (start_y + window_height) / row(1);i++) {
            printchar(start_x,row(i),145,COLORS.Highlight,S);
            printchar(start_x + window_width - row(1) + 1,row(i),146,COLORS.Lowlight,S);
        } 
        printline(start_x,start_y,143,window_width / 8,COLORS.Highlight,S);
        print(col(textcenter(this->str)),start_y + row(1),this->str,COLORS.Text,S);
		    UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = 0;
        updated++;
    }
}
