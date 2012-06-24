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
    Button *b;
    int tabindex=0;
    UI = new UserInterface;

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 32;
    b->y = 23;
    b->xsize = 7;
    b->ysize = 1;
    b->caption = "  Yes";
    b->OnClick = (ActFunc)BTNCLK_newsong_yes;

    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 32+7+2;
    b->y = 23;
    b->xsize = 7;
    b->ysize = 1;
    b->caption = "  No";
    b->OnClick = (ActFunc)BTNCLK_newsong_no;

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

void CUI_NewSong::update() {
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
    int i;
    if (S->lock()==0) {
        S->fillRect(col(30),row(20),col(50)-1,row(25)-1,COLORS.Background);
        printline(col(30),row(24),148,20,COLORS.Lowlight,S);
        for (i=20;i<25;i++) {
            printchar(col(30),row(i),145,COLORS.Highlight,S);
            printchar(col(49),row(i),146,COLORS.Lowlight,S);
        } 
        printline(col(30),row(20),143,20,COLORS.Highlight,S);
        print(col(30),row(21)," Clear Current Song",COLORS.Text,S);
        UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = 0;
        updated++;
    }
}
