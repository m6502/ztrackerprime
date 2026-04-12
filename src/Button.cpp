#include "Button.h"
#include "zt.h"

// ------------------------------------------------------------------------------------------------
//
//
void Button::auto_update_anchor()
{
    if(anchor_type & ANCHOR_RIGHT)
        anchor_x = INTERNAL_RESOLUTION_X / 8 ;

    if(anchor_type & ANCHOR_DOWN)
        anchor_y = INTERNAL_RESOLUTION_Y / 8 ;

    if(anchor_type & (ANCHOR_LEFT | ANCHOR_RIGHT))
        x = anchor_x + anchor_offset_x ;
            
    if(anchor_type & (ANCHOR_UP | ANCHOR_DOWN))
        y = anchor_y + anchor_offset_y ;
}



// ------------------------------------------------------------------------------------------------
// <Manu> Character anchor - Same as text input, merge
//
void Button::auto_anchor_at_current_pos(int how)
{
    anchor_type = how ;

    if(anchor_type & ANCHOR_RIGHT) {
                
        anchor_x = INTERNAL_RESOLUTION_X / 8 ;
    }
    else if(anchor_type & ANCHOR_LEFT) {
                
        anchor_x = 0 ;
    }

    if(anchor_type & ANCHOR_DOWN) {
                
        anchor_y = INTERNAL_RESOLUTION_Y / 8 ;
    }
    else if(anchor_type & ANCHOR_UP) {
                
        anchor_y = 0 ;
    }

    anchor_offset_x = x - anchor_x ;
    anchor_offset_y = y - anchor_y ;
}



// ------------------------------------------------------------------------------------------------
//
//
Button::Button(void) {
    caption = NULL;
    OnClick = NULL;
    state = 0;
    updown = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
int Button::mouseupdate(int cur_element) {
    KBKey key,act=0;
    key = Keys.checkkey();
    if (mousestate) {
        if (checkmousepos(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
            if (state != 3) {
                state = 3;
                act++;
                changed++;
                fixmouse++;
                    need_refresh++;
            need_redraw++;

            }
        } else {
            if (state != updown) {
                state = updown;
                act++;
                changed++;
                fixmouse++;
                    need_refresh++;
            need_redraw++;

            }
        }
        
    }
    if (key) {
        switch(key) {
            case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT)):
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
                    mousestate = 1;
                    state = 3;
                    act++;
                        need_refresh++;
            need_redraw++;

                    changed++; 
                    fixmouse++;
                }
                break;
            case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT)):
                if (mousestate) {
                    act++;
                    if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
                            need_refresh++;
            need_redraw++;

                        changed++; 
                        fixmouse++;
                        if (this->OnClick)
                            this->OnClick(this);
                    }
                }
                state = updown;
                mousestate=0;
                break;
        }
    }
    if (act)  key = Keys.getkey();
    if (cur_element != this->ID && mousestate && act) {
            need_refresh++;
            need_redraw++;

        fixmouse++;
        return this->ID;
    }
    return cur_element;
}




// ------------------------------------------------------------------------------------------------
//
//
int Button::update()
{
    // ------------------------------------
    // Anchor code 
    // ------------------------------------

    auto_update_anchor() ;

    // ------------------------------------
    // ------------------------------------

    KBKey key,act=0;
    int ret=0;
    key = Keys.checkkey();
    if (state==2  && !last_cmd_keyjazz){
        changed++;
            need_refresh++;
            need_redraw++;

        if (this->OnClick)
            this->OnClick(this);
        state = updown;
    }
    if (key) {
        switch(key) {
            case SDLK_UP: ret = -1; act++; break;
            case SDLK_DOWN: ret = 1; act++; break;

            case SDLK_SPACE:  //  / Same thing
            case SDLK_RETURN: //  \ Same thingg
                if (!last_cmd_keyjazz) {
                    act++; changed++; 
                    state=2;
                    last_keyjazz = key;
                    last_cmd_keyjazz = 1;
                }
                break;
        }
        if (act) {
            Keys.getkey();
                need_refresh++;
            need_redraw++;

        }
    }
    return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
void Button::draw(Drawable *S, int active) {
    char clearbuf[256];
    int clear_len = xsize;
    if (clear_len < 1) clear_len = 1;
    if (clear_len > (int)sizeof(clearbuf) - 1) clear_len = (int)sizeof(clearbuf) - 1;
    memset(clearbuf, ' ', clear_len);
    clearbuf[clear_len] = '\0';

    // Clear only the caption row; border rows are handled explicitly below.
    printBG(col(x),row(y),clearbuf,COLORS.Background,COLORS.Background,S);

    TColor top_color;
    TColor bottom_color;
    TColor left_color;
    TColor right_color;
    if (state | updown) {
        top_color = COLORS.Lowlight;
        bottom_color = COLORS.Highlight;
        left_color = COLORS.Lowlight;
        right_color = COLORS.Highlight;
    } else {
        top_color = COLORS.Highlight;
        bottom_color = COLORS.Lowlight;
        left_color = COLORS.Highlight;
        right_color = COLORS.Lowlight;
    }
    printline(col(x),row(y-1),0x86,xsize,top_color,S);
    printline(col(x),row(y+1),0x81,xsize,bottom_color,S);
//        printline(col(x),row(y+1),0x81,xsize,COLORS.Lowlight,S);

    printchar(col(x-1),row(y),0x84,left_color,S);
    printchar(col(x+xsize),row(y),0x83,right_color,S);

    if (active)
        print(col(x),row(y),caption,COLORS.Highlight,S);
    else
        print(col(x),row(y),caption,COLORS.Text,S);
    changed = 0;
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+2),row(y+2));
    screenmanager.Update(col(x-1),row(y),col(x+xsize+2),row(y));
}



