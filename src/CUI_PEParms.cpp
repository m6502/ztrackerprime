#include "zt.h"

CUI_PEParms::CUI_PEParms(void) {

    UI = new UserInterface;

    

    int window_width = 54 * col(1);
    int window_height = 20 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--);
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--);
    
    /* Initialize BPM Slider */
        vs_step = new ValueSlider;
        UI->add_element(vs_step,0);
        vs_step->frame = 1;
        vs_step->x = (start_x / 8) + 14;
        vs_step->y = (start_y / 8) + 6; 
        vs_step->xsize=window_width/8 - 20;
        vs_step->min = 0;
        vs_step->max = 32;
        vs_step->value = cur_step;   

        vs_pat_length = new ValueSlider;
        UI->add_element(vs_pat_length,1);
        vs_pat_length->frame = 1;
        vs_pat_length->x = (start_x / 8) + 14;
        vs_pat_length->y = (start_y / 8) + 8; 
        vs_pat_length->xsize=window_width/8 - 20;
        vs_pat_length->min = 32;
        vs_pat_length->max = 999;
        vs_pat_length->value = song->patterns[cur_edit_pattern]->length;

        vs_highlight = new ValueSlider;
        UI->add_element(vs_highlight,2);
        vs_highlight->frame = 1;
        vs_highlight->x = (start_x / 8) + 14;
        vs_highlight->y = (start_y / 8) + 10; 
        vs_highlight->xsize=window_width/8 - 20;
        vs_highlight->min = 1;
        vs_highlight->max = 32;
        vs_highlight->value = zt_config_globals.highlight_increment;

        vs_lowlight = new ValueSlider;
        UI->add_element(vs_lowlight,3);
        vs_lowlight->frame = 1;
        vs_lowlight->x = (start_x / 8) + 14;
        vs_lowlight->y = (start_y / 8) + 12; 
        vs_lowlight->xsize=window_width/8 - 20;
        vs_lowlight->min = 1;
        vs_lowlight->max = 32;
        vs_lowlight->value = zt_config_globals.lowlight_increment;

        cb_centered = new CheckBox;
        UI->add_element(cb_centered,4);
        cb_centered->frame = 0;
        cb_centered->x = (start_x / 8) + 14;
        cb_centered->y = (start_y / 8) + 14;
        cb_centered->xsize = 5;
        cb_centered->value = &zt_config_globals.centered_editing;
        cb_centered->frame = 1;

        cb_stepedit = new CheckBox;
        UI->add_element(cb_stepedit,5);
        cb_stepedit->frame = 0;
        cb_stepedit->x = (start_x / 8) + 14 + 16;
        cb_stepedit->y = (start_y / 8) + 14;
        cb_stepedit->xsize = 5;
        cb_stepedit->value = &zt_config_globals.step_editing;
        cb_stepedit->frame = 1;

        cb_recveloc = new CheckBox;
        UI->add_element(cb_recveloc,6);
        cb_recveloc->frame = 0;
        cb_recveloc->x = (start_x / 8) + 14 + 32;
        cb_recveloc->y = (start_y / 8) + 14;
        cb_recveloc->xsize = 5;
        cb_recveloc->value = &zt_config_globals.record_velocity;
        cb_recveloc->frame = 1;

        vs_speedup = new ValueSlider;
        UI->add_element(vs_speedup,7);
        vs_speedup->frame = 1;
        vs_speedup->x = (start_x / 8) + 14;
        vs_speedup->y = (start_y / 8) + 16;
        vs_speedup->xsize=window_width/8 - 20;
        vs_speedup->min=1;
        vs_speedup->max = 32;
        vs_speedup->value = zt_config_globals.control_navigation_amount;

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

    int window_width = 54 * col(1);
    int window_height = 20 * row(1);
    int start_x = (INTERNAL_RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--);
    int start_y = (INTERNAL_RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--);

    vs_step->x = (start_x / 8) + 14;
    vs_step->y = (start_y / 8) + 6; 
    vs_pat_length->x = (start_x / 8) + 14;
    vs_pat_length->y = (start_y / 8) + 8; 
    vs_highlight->x = (start_x / 8) + 14;
    vs_highlight->y = (start_y / 8) + 10; 
    vs_lowlight->x = (start_x / 8) + 14;
    vs_lowlight->y = (start_y / 8) + 12; 
    cb_centered->x = (start_x / 8) + 14;
    cb_centered->y = (start_y / 8) + 14;
    cb_stepedit->x = (start_x / 8) + 14 + 16;
    cb_stepedit->y = (start_y / 8) + 14;
    cb_recveloc->x = (start_x / 8) + 14 + 32;
    cb_recveloc->y = (start_y / 8) + 14;
    vs_speedup->x = (start_x / 8) + 14;
    vs_speedup->y = (start_y / 8) + 16;


    if (S->lock()==0) {
        S->fillRect(start_x,start_y,start_x + window_width,start_y + window_height,COLORS.Background);
        printline(start_x,start_y + window_height - row(1) + 1,148,window_width / 8, COLORS.Lowlight,S);
        for (int i = start_y / row(1); i < (start_y + window_height) / row(1);i++) {
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

