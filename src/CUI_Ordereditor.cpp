#include "zt.h"
#include "OrderEditor.h"



CUI_Ordereditor::CUI_Ordereditor(void) {
    OrderEditor *oe;

    UI = new UserInterface;

    oe = new OrderEditor;
    UI->add_element(oe,0);
    oe->x = 2;
    oe->y = 14;
    oe->ysize = 32;
    oe->xsize = 9;
}

CUI_Ordereditor::~CUI_Ordereditor(void) {

}

void CUI_Ordereditor::enter(void) {
    need_refresh++;
    cur_state = STATE_ORDER;
}

void CUI_Ordereditor::leave(void) {

}

void CUI_Ordereditor::update() {
    int key=0;
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
    }
}

void CUI_Ordereditor::draw(Drawable *S) {
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(11,"Order List",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}