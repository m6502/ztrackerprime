#include "zt.h"

CUI_PEParms::CUI_PEParms(void) {
    ValueSlider *vs;
	CheckBox *cb;
    UI = new UserInterface;

    int window_width = 54 * col(1);
    int window_height = 20 * row(1);
    int start_x = (RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--);
    int start_y = (RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--);
    
    /* Initialize BPM Slider */
        vs = new ValueSlider;
        UI->add_element(vs,0);
        vs->frame = 1;
        vs->x = (start_x / 8) + 14;
        vs->y = (start_y / 8) + 6; 
        vs->xsize=window_width/8 - 20;
        vs->min = 0;
        vs->max = 32;
        vs->value = cur_step;   

        vs = new ValueSlider;
        UI->add_element(vs,1);
        vs->frame = 1;
        vs->x = (start_x / 8) + 14;
        vs->y = (start_y / 8) + 8; 
        vs->xsize=window_width/8 - 20;
        vs->min = 32;
        vs->max = 999;
        vs->value = song->patterns[cur_edit_pattern]->length;

        vs = new ValueSlider;
        UI->add_element(vs,2);
        vs->frame = 1;
        vs->x = (start_x / 8) + 14;
        vs->y = (start_y / 8) + 10; 
        vs->xsize=window_width/8 - 20;
        vs->min = 1;
        vs->max = 32;
        vs->value = zt_config_globals.highlight_increment;

        vs = new ValueSlider;
        UI->add_element(vs,3);
        vs->frame = 1;
        vs->x = (start_x / 8) + 14;
        vs->y = (start_y / 8) + 12; 
        vs->xsize=window_width/8 - 20;
        vs->min = 1;
        vs->max = 32;
        vs->value = zt_config_globals.lowlight_increment;

        cb = new CheckBox;
        UI->add_element(cb,4);
        cb->frame = 0;
        cb->x = (start_x / 8) + 14;
        cb->y = (start_y / 8) + 14;
        cb->xsize = 5;
        cb->value = &zt_config_globals.centered_editing;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,5);
        cb->frame = 0;
        cb->x = (start_x / 8) + 14 + 16;
        cb->y = (start_y / 8) + 14;
        cb->xsize = 5;
        cb->value = &zt_config_globals.step_editing;
        cb->frame = 1;

        cb = new CheckBox;
        UI->add_element(cb,6);
        cb->frame = 0;
        cb->x = (start_x / 8) + 14 + 32;
        cb->y = (start_y / 8) + 14;
        cb->xsize = 5;
        cb->value = &zt_config_globals.record_velocity;
        cb->frame = 1;

        vs = new ValueSlider;
        UI->add_element(vs,7);
        vs->frame = 1;
        vs->x = (start_x / 8) + 14;
        vs->y = (start_y / 8) + 16;
        vs->xsize=window_width/8 - 20;
        vs->min=1;
        vs->max = 32;
        vs->value = zt_config_globals.control_navigation_amount;

}

CUI_PEParms::~CUI_PEParms(void) {
    if (UI) delete UI; UI = NULL;
}



void CUI_PEParms::enter(void) {
    ValueSlider *vs;
	CheckBox *cb;

    need_refresh = 1;
    vs = (ValueSlider *)UI->get_element(0);
    vs->value = cur_step;
    cur_state = STATE_PEDIT_WIN;
    vs = (ValueSlider *)UI->get_element(1);
    vs->value = song->patterns[cur_edit_pattern]->length;
    vs = (ValueSlider *)UI->get_element(2);
    vs->value = zt_config_globals.highlight_increment;
    vs = (ValueSlider *)UI->get_element(3);
    vs->value = zt_config_globals.lowlight_increment;
	cb = (CheckBox *)UI->get_element(4);
	cb->value = &zt_config_globals.centered_editing;
	cb = (CheckBox *)UI->get_element(5);
	cb->value = &zt_config_globals.step_editing;
	cb = (CheckBox *)UI->get_element(6);
	cb->value = &zt_config_globals.record_velocity;
    vs = (ValueSlider *)UI->get_element(7);
    vs->value = zt_config_globals.control_navigation_amount;
}

void CUI_PEParms::leave(void) {
    cur_state = STATE_PEDIT;
}

void CUI_PEParms::update() {
    int key=0;
    ValueSlider *vs;
	CheckBox *cb;

    UI->update();
    vs = (ValueSlider *)UI->get_element(0);
    if (vs->changed)
        cur_step = vs->value;
    if (Keys.size()) {
        key = Keys.getkey();
        if (key == DIK_ESCAPE || (key == DIK_RETURN) || key==DIK_F2 || key==DIK_MOUSE_2_ON) {
            if (key == DIK_RETURN) {
                vs = (ValueSlider *)UI->get_element(1);
                song->patterns[cur_edit_pattern]->resize(vs->value);
            }
            close_popup_window();
            fixmouse++;
            need_refresh++;
        }
    }
    vs = (ValueSlider *)UI->get_element(2);
    if(vs->changed)
        zt_config_globals.highlight_increment = vs->value;
    vs = (ValueSlider *)UI->get_element(3);
    if(vs->changed)
        zt_config_globals.lowlight_increment = vs->value;
	cb = (CheckBox *)UI->get_element(4);
	if(cb->changed)
		zt_config_globals.centered_editing = *(cb->value);
	cb = (CheckBox *)UI->get_element(5);
	if(cb->changed)
		zt_config_globals.step_editing = *(cb->value);
	cb = (CheckBox *)UI->get_element(6);
	if(cb->changed)
		zt_config_globals.record_velocity = *(cb->value);

    vs = (ValueSlider *)UI->get_element(7);
    if(vs->changed)
        zt_config_globals.control_navigation_amount = vs->value;

}

void CUI_PEParms::draw(Drawable *S) {
    int i;
    int window_width = 54 * col(1);
    int window_height = 20 * row(1);
    int start_x = (RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--);
    int start_y = (RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--);

    if (S->lock()==0) {
        S->fillRect(start_x,start_y,start_x + window_width,start_y + window_height,COLORS.Background);
        printline(start_x,start_y + window_height - row(1) + 1,148,window_width / 8, COLORS.Lowlight,S);
        for (i = start_y / row(1); i < (start_y + window_height) / row(1);i++) {
            printchar(start_x,row(i),145,COLORS.Highlight,S);
            printchar(start_x + window_width - row(1) + 1,row(i),146,COLORS.Lowlight,S);
        }
        printline(start_x,start_y,143,window_width / 8,COLORS.Highlight,S);
        print(col(textcenter("Pattern Editor Options")),start_y + row(2),"Pattern Editor Options",COLORS.Text,S);
        print(start_x + col(2),start_y + row(6),"      Step:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(8),"Pat length:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(10)," HighLight:",COLORS.Text,S);
        print(start_x + col(2),start_y + row(12),"  LowLight:",COLORS.Text,S);
	print(start_x + col(2),start_y + row(14),"  Centered:",COLORS.Text,S);
	print(start_x + col(20),start_y + row(14),"StepEdit:",COLORS.Text,S);
    print(start_x + col(36),start_y + row(14),"RecVeloc:",COLORS.Text,S);
	print(start_x + col(2),start_y + row(16),"   Speedup:",COLORS.Text,S);
        UI->full_refresh();
        UI->draw(S);
        S->unlock();
        need_refresh = need_popup_refresh = 0;
        updated++;
    }
}

