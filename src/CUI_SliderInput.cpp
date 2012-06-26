#include "zt.h"

CUI_SliderInput::CUI_SliderInput(void) {
    TextInput *ti;
    UI = new UserInterface;

    memset(this->str,0,5);
    
    /* Initialize BPM Slider */
        ti = new TextInput;
        
        UI->add_element(ti,0);
        ti->frame = 1;
        ti->x     = 44;
        ti->y     = 22;
        ti->xsize = 5;
        ti->ysize = 1;
        ti->length = 5;
        this->canceled=1;
        ti->str = (unsigned char *)this->str;
        memset(this->str,0,5);

    this->state = 0;
    ti->cursor = 0;
    this->checked = 1;
}

CUI_SliderInput::~CUI_SliderInput(void) {
    if (UI) delete UI; UI = NULL;
}

void CUI_SliderInput::setfirst(int val) {
    TextInput *ti;
    ti = (TextInput *)UI->get_element(0);
    ti->cursor=1;
    sprintf(this->str,(val < 0)?"-":"%.1d",val);
}

void CUI_SliderInput::enter(void) {
    TextInput *ti;
    ti = (TextInput *)UI->get_element(0);
    this->result = 0;
    this->canceled = 1;
    checked = 1;
    this->state = 1;
    this->prev_state = cur_state;
    cur_state = STATE_SLIDER_INPUT;
}

void CUI_SliderInput::leave(void) {
    TextInput *ti;
    ti = (TextInput *)UI->get_element(0);
    this->result = atoi(this->str);
    this->checked = 0;
    this->state = 0;
    ti->cursor = 0;
    memset(this->str,0,5);
    cur_state = this->prev_state;
}

void CUI_SliderInput::update() {
    int key=0,act=0;;
//  act = Keys.size(); 
//  key = Keys.checkkey();
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
        switch(key) {
            case DIK_ESCAPE:
                this->canceled = 1;
                act++;
                break;
            case DIK_RETURN:
                this->canceled = 0;
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

void CUI_SliderInput::draw(Drawable *S) {
    int i;
    if (this->state) {
        if (S->lock()==0) {
            S->fillRect(col(30),row(20),col(50)-1,row(25)-1,COLORS.Background);
            printline(col(30),row(24),148,20,COLORS.Lowlight,S);

            for (i=20;i<25;i++) {
                printchar(col(30),row(i),145,COLORS.Highlight,S);
                printchar(col(49),row(i),146,COLORS.Lowlight,S);
            } 

            printline(col(30),row(20),143,20,COLORS.Highlight,S);

            print(col(30),row(22)," Enter Value:",COLORS.Text,S);
            UI->draw(S);
            S->unlock();
            need_refresh = need_popup_refresh = 0;
            updated++;
        }
    }
}

int CUI_SliderInput::getresult(void) {
    this->checked = 1;
    return this->result;
}