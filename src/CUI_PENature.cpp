#include "zt.h"
#include <stdlib.h>
#include <time.h>

CUI_PENature::CUI_PENature(void) {
    ValueSlider *vs;
    UI = new UserInterface;
    srand((unsigned)time( NULL ));
    int window_width = 40 * col(1);
    int window_height = 12 * row(1);
    int start_x = (RESOLUTION_X / 2) - (window_width / 2);
    for(;start_x % 8;start_x--);
    int start_y = (RESOLUTION_Y / 2) - (window_height / 2);
    for(;start_y % 8;start_y--);
    
    /* Initialize Naturalization percent Slider */
        vs = new ValueSlider;
        UI->add_element(vs,0);
        vs->frame = 1;
        vs->x = (start_x / 8) + 12; 
        vs->y = (start_y + (window_height / 2) ) / 8; 
        vs->xsize=(window_width / 8) - 19;
        vs->min = 0;
        vs->max = 100;
        vs->value = cur_naturalization_percent;
}

CUI_PENature::~CUI_PENature(void) {
    if (UI) delete UI; UI = NULL;
}



void CUI_PENature::enter(void) {
    ValueSlider *vs;
    need_refresh = 1;
    vs = (ValueSlider *)UI->get_element(0);
    vs->value = cur_naturalization_percent;
}

void CUI_PENature::leave(void) {
    
}

void CUI_PENature::update() {
    int key=0;
    int i,j,iadd;
    event*e;

    ValueSlider *vs;
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
        if (key == DIK_ESCAPE || (key == DIK_RETURN) || key==DIK_MOUSE_2_ON) {
            if (key == DIK_RETURN) {
                vs = (ValueSlider *)UI->get_element(0);
                cur_naturalization_percent = vs->value;
                for(i=select_track_start;i<=select_track_end;i++) {
                    if (select_row_end - select_row_start > 0) {
                        for(j=select_row_start;j<=select_row_end;j++) {
                            e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j);
                            if (e && (e->note<0x80 || e->vol<0x80)) {
                                iadd = -1;
                                if (e->inst<MAX_INSTS)
                                    iadd = song->instruments[e->inst]->default_volume;
                                if (e->vol<0x80) 
                                    iadd = e->vol;
                                if (iadd!=-1) {
                                    float temp;
                                    float nat;
                                    temp = rand();
                                    temp /= RAND_MAX;
                                    temp *= iadd;
                                    nat = cur_naturalization_percent;
                                    nat /=100;
                                    temp *= nat;
                                    iadd -=temp;
                                    //iadd = iadd - (iadd * ((rand() / RAND_MAX) * (cur_naturalization_percent / 100)));
                                    if (iadd > 0x7F) iadd = 0x7F;
                                    if (iadd < 0) iadd = 0;
                                    song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,-1,-1,iadd,-1,-1,-1);
                                }
                            }
                        }
                    }
                }
            }
            close_popup_window();
            fixmouse++;
            need_refresh++;
        }
    }
}

void CUI_PENature::draw(Drawable *S) {
    int i;
    if (S->lock()==0) {
        int window_width = 40 * col(1);
        int window_height = 12 * row(1);
        int start_x = (RESOLUTION_X / 2) - (window_width / 2);
        for(;start_x % 8;start_x--);
        int start_y = (RESOLUTION_Y / 2) - (window_height / 2);
        for(;start_y % 8;start_y--);

        S->fillRect(start_x,start_y,start_x + window_width,start_y + window_height,COLORS.Background);
        printline(start_x,start_y + window_height - row(1) + 1,148,window_width / 8, COLORS.Lowlight,S);
        for (i = start_y / row(1); i < (start_y + window_height) / row(1);i++) {
            printchar(start_x,row(i),145,COLORS.Highlight,S);
            printchar(start_x + window_width - row(1) + 1,row(i),146,COLORS.Lowlight,S);
        }
        printline(start_x,start_y,143,window_width / 8,COLORS.Highlight,S);
        print(col(textcenter("Naturalization Amount")),start_y + row(2),"Naturalization Amount",COLORS.Text,S);
        print(start_x + col(1), start_y + window_height / 2,"  Percent:",COLORS.Text,S);
        UI->draw(S);
        S->unlock();
        need_refresh = need_popup_refresh = 0;
        updated++;
    }
}

