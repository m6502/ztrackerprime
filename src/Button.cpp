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
            case DIK_MOUSE_1_ON:
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
            case DIK_MOUSE_1_OFF:
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
            case DIK_UP: ret = -1; act++; break;
            case DIK_DOWN: ret = 1; act++; break;

            case DIK_SPACE:  //  / Same thing
            case DIK_RETURN: //  \ Same thingg
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
    if (active) 
        print(col(x),row(y),caption,COLORS.Highlight,S);
    else
        print(col(x),row(y),caption,COLORS.Text,S);

    if (state | updown) {
        printline(col(x),row(y-1),0x86,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+1),0x81,xsize,COLORS.Highlight,S);
//        printline(col(x),row(y+1),0x81,xsize,COLORS.Lowlight,S);

        printchar(col(x-1),row(y),0x84,COLORS.Lowlight,S);
        printchar(col(x+xsize),row(y),0x83,COLORS.Highlight,S);

//        printchar(col(x+xsize),row(y),0x83,COLORS.Lowlight,S);
    } else {
        printline(col(x),row(y-1),0x86,xsize,COLORS.Highlight,S);
        printline(col(x),row(y+1),0x81,xsize,COLORS.Lowlight,S);
        printchar(col(x-1),row(y),0x84,COLORS.Highlight,S);
        printchar(col(x+xsize),row(y),0x83,COLORS.Lowlight,S);
    }
    changed = 0;
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+2),row(y+2));
}



