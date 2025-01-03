#include "zt.h"
#include "Button.h"


void close_newsong_window(void) {
    Keys.flush();
    close_popup_window();
    fixmouse++;
    need_refresh++;
}

void make_new_song(void) {
    ztPlayer->stop();
    song->de_init();
    song->init();
    sprintf(szStatmsg,"Song cleared");
    statusmsg = szStatmsg;
    status_change = 1;
    reset_editor();
}

void BTNCLK_newsong_yes(void) {
    make_new_song();
    close_newsong_window();
}

void BTNCLK_newsong_no(void) {
    close_newsong_window();
}

CUI_NewSong::CUI_NewSong(void) {
    int tabindex=0;
    UI = new UserInterface;

    b_yes = new Button;
    UI->add_element(b_yes,tabindex++);
    b_yes->x = 32;
    b_yes->y = 23;
    b_yes->xsize = 7;
    b_yes->ysize = 1;
    b_yes->caption = "  Yes";
    b_yes->OnClick = (ActFunc)BTNCLK_newsong_yes;

    b_no = new Button;
    UI->add_element(b_no,tabindex++);
    b_no->x = 32+7+2;
    b_no->y = 23;
    b_no->xsize = 7;
    b_no->ysize = 1;
    b_no->caption = "  No";
    b_no->OnClick = (ActFunc)BTNCLK_newsong_no;

}

CUI_NewSong::~CUI_NewSong(void) {
    if (UI) delete UI; UI = NULL;
}

void CUI_NewSong::enter(void) {
    need_refresh = 1;
    UI->cur_element = 1;
}

void CUI_NewSong::leave(void) {
}

void CUI_NewSong::update()
{
    int key=0,act=0;;
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
                need_refresh++; 
                fixmouse++;
                break;
            case DIK_Y:
                make_new_song();
                act++;
                break;
            case DIK_N:
            case DIK_ESCAPE:
                act++;
                break;
        }
    }   
    if (act) {
        close_popup_window();
        fixmouse++;
        need_refresh++;
    }
}

void CUI_NewSong::draw(Drawable *S) {

    int popup_x = ((INTERNAL_RESOLUTION_X / 8) / 2) - 7 - 1 - 2 ;
    int popup_y = ((INTERNAL_RESOLUTION_Y / 8) / 2) - 2 - 1 ;

    b_yes->x = popup_x + 2 ;
    b_yes->y = popup_y + 3 ;
    b_no->x = b_yes->x + 7 + 2 ;
    b_no->y = b_yes->y ;

    if (S->lock()==0) {
        S->fillRect(col(popup_x),row(popup_y),col(popup_x + 20)-1,row(popup_y + 5)-1,COLORS.Background);
        printline(col(popup_x),row(popup_y + 4),148,20,COLORS.Lowlight,S);
        for (int i=0;i<5;i++) {
            printchar(col(popup_x),row(popup_y + i),145,COLORS.Highlight,S);
            printchar(col(popup_x + 19),row(popup_y + i),146,COLORS.Lowlight,S);
        } 
        printline(col(popup_x),row(popup_y),143,20,COLORS.Highlight,S);
        print(col(popup_x),row(popup_y + 1)," Clear Current Song",COLORS.Text,S);
        UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = 0;
        updated++;
    }
}
