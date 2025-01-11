#include "zt.h"
#include "Sliders.h"

extern int needaclear ;



// ------------------------------------------------------------------------------------------------
//
//
ValueSlider::ValueSlider(void) {

  force_v = 0;
  force_f = 0;
  changed = 0;
  ysize   = 1;
  newclick= 1;
  focus   = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
ValueSlider::ValueSlider(int fset) 
{
    force_v = 0;
    force_f = 0;
    changed = 0;
    ysize   = 1;
    newclick= 1;
    focus = fset;
}




// ------------------------------------------------------------------------------------------------
//
//
int ValueSlider::mouseupdate(int cur_element) 
{
    int newval;
    float f;
    KBKey key,act=0;
    key = Keys.checkkey();
    if (key) {
        switch(key) {
            case DIK_MOUSE_1_ON:
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+1))) {
                    mousestate = 1;
                    act++;
                    newclick=0; // unset click to focus
                }
                break;
            case DIK_MOUSE_1_OFF:
                if (mousestate) act++;
                mousestate=0;
                break;

                ///////////////////////////////////////////////////
                // Here is right click mouse focus

            case DIK_MOUSE_2_ON:
                if(focus)           // HERE we encapsulate the event in focus determination
                {                   // same thing for all the other varieties of value slider
                    if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+1))) {
                    newclick=1;
                    mousestate=1;
                    }
                }
                break;
            case DIK_MOUSE_2_OFF:
                if(focus)           // again here encapsulated. this is all we need to do to ignore focus
                {
                    mousestate=0;
                    break;
                }

                ///////////////////////////////////////////////////
        }
    }

    /////////////////////////////////////////////////////
    // Modified mousestate for click to focus

    if (mousestate) {
        if(!newclick){
            f = ((float)(LastX-col(this->x)) / (float)col(this->xsize));
            f = (f * (float)(max-min)) + min;
            if (f-floor(f) >= 0.5) f++;
            newval = (int) floor(f);
            if (newval<min) newval=min;
            if (newval>max) newval=max;
            if (newval != value) {
                        need_refresh++;
        need_redraw++;

                fixmouse++;
                changed++;
                value = newval;
            }
            this->need_redraw++;
        }
        return this->ID;
    }

    ///////////////////////////////////////////////////////
    if (act) {
        key = Keys.getkey();
        need_refresh++;
        need_redraw++;

        fixmouse++;
    }
    return cur_element;
}




// ------------------------------------------------------------------------------------------------
//
//
int ValueSlider::update() {
    KBKey key,act=0;
    int ret=0,pop=0,c;
    key = Keys.checkkey();
    unsigned char kstate = Keys.getstate();

    if (!UIP_SliderInput->checked){
        c = UIP_SliderInput->getresult();
        if (!UIP_SliderInput->canceled) {
            changed++;
            value = c;
        }
        if (window_stack.isempty())
            needaclear++; 
        need_refresh++;
        need_redraw++;

    }

    if (key) {
        switch(key) {

            case DIK_UP: ret = -1; act++; break;
            case DIK_DOWN: ret = 1; act++; break;
            case DIK_LEFT: if (kstate&KS_CTRL) value-=(max/8); else value--;  act++; changed++; break;
            case DIK_RIGHT: if (kstate&KS_CTRL) value+=(max/8); else value++;  act++; changed++; break;
            case DIK_HOME: value=min; act++; changed++; break;
            case DIK_END: value=max; act++; changed++; break;

            case DIK_0: 
                UIP_SliderInput->setfirst(0);
                pop++;
                break;
            case DIK_1:
                UIP_SliderInput->setfirst(1);
                pop++;
                break;
            case DIK_2:
                UIP_SliderInput->setfirst(2);
                pop++;
                break;
            case DIK_3:
                UIP_SliderInput->setfirst(3);
                pop++;
                break;
            case DIK_4:
                UIP_SliderInput->setfirst(4);
                pop++;
                break;
            case DIK_5:
                UIP_SliderInput->setfirst(5);
                pop++;
                break;
            case DIK_6:
                UIP_SliderInput->setfirst(6);
                pop++;
                break;
            case DIK_7:
                UIP_SliderInput->setfirst(7);
                pop++;
                break;
            case DIK_8:
                UIP_SliderInput->setfirst(8);
                pop++;
                break;
            case DIK_9:
                UIP_SliderInput->setfirst(9);
                pop++;
                break;
            case DIK_MINUS:
                UIP_SliderInput->setfirst(-1);
                pop++;
                break;

        }
        
        if (pop) {
            key = Keys.getkey();    
            need_refresh++;
        need_redraw++;

            popup_window(UIP_SliderInput);          
        }
        if (act) {
            key = Keys.getkey();
            need_refresh++;
        need_redraw++;

        }
    }
    if (value<min) value=min;
    if (value>max) value=max;
    return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
void ValueSlider::draw(Drawable *S, int active) {
    int cx,cy=y;
    TColor col;
    char str[32];
    changed = 0;
    for(cx=x;cx<x+xsize;cx++) {
        printBG(col(cx),row(cy)," ",COLORS.Text,COLORS.EditBG,S);
    }

    int v = value;
    if (force_f)
        v = force_v;
    if (v>=0)
        sprintf(str," %.3d",v);
    else
        sprintf(str,"%.3d",v);

    printBG(col(x+xsize),row(cy),str,COLORS.Text,COLORS.Background,S);
    
    if (active)
        col = COLORS.Highlight;
    else
        col = COLORS.EditText;

    printchar(col(x)+(((value-min)*(((xsize-1)*8)-1))/(max-min))+1,row(cy),155,col,S);//156
    if (frame) {
        printline(col(x),row(y-1),0x86,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+1),0x81,xsize,COLORS.Highlight,S);
        printchar(col(x-1),row(y),0x84,COLORS.Lowlight,S);
        printchar(col(x+xsize),row(y),0x83,COLORS.Highlight,S);
    }
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+5),row(y+1));
    changed = 0;
}





// ------------------------------------------------------------------------------------------------
//
//
ValueSliderOFF::ValueSliderOFF(void) {
    changed = 0;
    ysize = 1;
    newclick=1;
    focus=0;
}




// ------------------------------------------------------------------------------------------------
//
//
ValueSliderOFF::ValueSliderOFF(int fset) {
    changed = 0;
    ysize = 1;
    newclick=1;
    focus=fset;
}




// ------------------------------------------------------------------------------------------------
//
//
int ValueSliderOFF::mouseupdate(int cur_element) {
    int newval;
    float f;
    KBKey key,act=0;
    key = Keys.checkkey();
    if (key) {
        switch(key) {
            case DIK_MOUSE_1_ON:
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+1))) {
                    mousestate = 1;
                    act++;
                    newclick=0; // unset click to focus
                }
                break;
            case DIK_MOUSE_1_OFF:
                if (mousestate) act++;
                mousestate=0;
                break;

                ///////////////////////////////////////////////////
                // Here is right click mouse focus

            case DIK_MOUSE_2_ON:
                if(focus)
                {
                    if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+1))) {
                        newclick=1;
                        mousestate=1;
                    }
                }
                break;
            case DIK_MOUSE_2_OFF:
                if(focus)
                {
                    mousestate=0;
                }
                break;

                ///////////////////////////////////////////////////
        }
    }
    /////////////////////////////////////////
    // click to focus stuff

    if (mousestate) {
        if(!newclick){
            f = ((float)(LastX-col(this->x)) / (float)col(this->xsize));
            f = (f * (float)(max-min)) + min;
            if (f-floor(f) >= 0.5) f++;
            newval = (int) floor(f);
            if (newval<min) newval=min;
            if (newval>max) newval=max;
            if (newval != value) {
                need_refresh++;
                need_redraw++;
    
                fixmouse++;
                changed++;
                value = newval;
            }
        }
        return this->ID;
    }

    //////////////////////////////////

    if (act) {
        key = Keys.getkey();
        need_refresh++;
        need_redraw++;

        fixmouse++;
    }
    return cur_element;
}





// ------------------------------------------------------------------------------------------------
//
//
int ValueSliderOFF::update() {
    KBKey key,act=0;
    int ret=0,pop=0,c;
    key = Keys.checkkey();
    unsigned char kstate = Keys.getstate();

    if (!UIP_SliderInput->checked){
        c = UIP_SliderInput->getresult();
        if (!UIP_SliderInput->canceled) {
            changed++;
            value = c;
        }
        needaclear++; need_refresh++;
    }
    
    if (key) {
        switch(key) {
            case DIK_UP: ret = -1; act++; break;
            case DIK_DOWN: ret = 1; act++; break;
            case DIK_LEFT: if (kstate&KS_CTRL) value-=(max/8); else value--;  act++; changed++; break;
            case DIK_RIGHT: if (kstate&KS_CTRL) value+=(max/8); else value++;  act++; changed++; break;
            case DIK_HOME: value=min; act++; changed++; break;
            case DIK_END: value=max; act++; changed++; break;

            case DIK_0: 
                UIP_SliderInput->setfirst(0);
                pop++;
                break;
            case DIK_1:
                UIP_SliderInput->setfirst(1);
                pop++;
                break;
            case DIK_2:
                UIP_SliderInput->setfirst(2);
                pop++;
                break;
            case DIK_3:
                UIP_SliderInput->setfirst(3);
                pop++;
                break;
            case DIK_4:
                UIP_SliderInput->setfirst(4);
                pop++;
                break;
            case DIK_5:
                UIP_SliderInput->setfirst(5);
                pop++;
                break;
            case DIK_6:
                UIP_SliderInput->setfirst(6);
                pop++;
                break;
            case DIK_7:
                UIP_SliderInput->setfirst(7);
                pop++;
                break;
            case DIK_8:
                UIP_SliderInput->setfirst(8);
                pop++;
                break;
            case DIK_9:
                UIP_SliderInput->setfirst(9);
                pop++;
                break;
            case DIK_MINUS:
                UIP_SliderInput->setfirst(-1);
                pop++;
                break;
        }
        if (pop) {
            key = Keys.getkey();    
            if (cur_state != STATE_PEDIT_WIN) {
                need_refresh++;
                need_redraw++;

                popup_window(UIP_SliderInput);          
            }
        }
        if (act) {
            key = Keys.getkey();
            need_refresh++;
            need_redraw++;

        }
    }
    if (value<min) value=min;
    if (value>max) value=max;
    return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
void ValueSliderOFF::draw(Drawable *S, int active) {
    int cx,cy=y,v;
    TColor col;
    char str[32];
    changed = 0;
    
//    S->fillRect(col(x-1),row(y-1),col(x+xsize+7),row(y+1),RGB(0xA0,0xA0,0xFF));
    for(cx=x;cx<x+xsize;cx++) {
        printBG(col(cx),row(cy)," ",COLORS.Text,COLORS.EditBG,S);
    }
    if (value<0) {
        if (max>9999)
            sprintf(str,"OFF  "); 
        else
            sprintf(str,"OFF "); 
        v=0;
    } else {
        if (max>9999) 
            sprintf(str,"%.5d",value);
        else
        if (max>999) 
            sprintf(str,"%.4d",value);
        else
        if (max>99) 
            sprintf(str,"%.3d",value);
        else
            sprintf(str,"%.2d",value);
        v = value;
    }
    printBG(col(x+xsize+1),row(cy),str,COLORS.Text,COLORS.Background,S);
    if (active)
        col = COLORS.Highlight;
    else
        col = COLORS.EditText;
    printchar(col(x)+(((v-min)*(((xsize-1)*8)-1))/(max-min))+1,row(cy),155,col,S);//156
    if (frame) {
        printline(col(x),row(y-1),0x86,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+1),0x81,xsize,COLORS.Highlight,S);
        printchar(col(x-1),row(y),0x84,COLORS.Lowlight,S);
        printchar(col(x+xsize),row(y),0x83,COLORS.Highlight,S);
    }
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+7),row(y+1));
    changed = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
ValueSliderDL::ValueSliderDL(void) {
    changed = 0;
    ysize = 1;
    newclick=1;     // NEW Click set
    focus=0;
}




// ------------------------------------------------------------------------------------------------
//
//
ValueSliderDL::ValueSliderDL(int fset) {
    changed = 0;
    ysize = 1;
    newclick=1;     // NEW Click set
    focus=fset;
}




// ------------------------------------------------------------------------------------------------
//
//
int ValueSliderDL::mouseupdate(int cur_element) {
    int newval;
    float f;
    KBKey key,act=0;
    key = Keys.checkkey();
    if (key) {
        switch(key) {
            case DIK_MOUSE_1_ON:
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+1))) {
                    mousestate = 1;
                    act++;
                    newclick=0; // unset click to focus
                }
                break;
            case DIK_MOUSE_1_OFF:
                if (mousestate) act++;
                mousestate=0;
                break;

                ///////////////////////////////////////////////////
                // Here is right click mouse focus

            case DIK_MOUSE_2_ON:
                if(focus)
                {
                    if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+1))) {
                        newclick=1;
                        mousestate=1;
                    }
                }
                break;
            case DIK_MOUSE_2_OFF:
                if(focus)
                {
                    mousestate=0;
                }
                break;

                ///////////////////////////////////////////////////
        }
    }
    /////////////////////////////////////////
    // Click to focus

    if (mousestate) {
        if(!newclick)
        {
            f = ((float)(LastX-col(this->x)) / (float)col(this->xsize));
            f = (f * (float)(max-min)) + min;
            if (f-floor(f) >= 0.5) f++;
            newval = (int) floor(f);
            if (newval<min) newval=min;
            if (newval>max) newval=max;
            if (newval != value) {
                need_refresh++;
                need_redraw++;

                fixmouse++;
                changed++;
                value = newval;
            }
        }
        return this->ID;
    }

    /////////////////////////////////////////

    if (act) {
        key = Keys.getkey();
        need_refresh++;
        need_redraw++;

        fixmouse++;
    }
    return cur_element;
}





// ------------------------------------------------------------------------------------------------
//
//
int ValueSliderDL::update() {
    KBKey key,act=0;
    int ret=0,pop=0,c;
    key = Keys.checkkey();
    unsigned char kstate = Keys.getstate();
    if (!UIP_SliderInput->checked){
        c = UIP_SliderInput->getresult();
        if (!UIP_SliderInput->canceled) {
            changed++;
            value = c;
        }
        needaclear++; need_refresh++;
    }
    
    if (key) {
        switch(key) {
            case DIK_UP: ret = -1; act++; break;
            case DIK_DOWN: ret = 1; act++; break;
            case DIK_LEFT: if (kstate&KS_CTRL) value-=(max/8); else value--;  act++; changed++; break;
            case DIK_RIGHT: if (kstate&KS_CTRL) value+=(max/8); else value++;  act++; changed++; break;
            case DIK_HOME: value=min; act++; changed++; break;
            case DIK_END: value=max; act++; changed++; break;

            case DIK_0: 
                UIP_SliderInput->setfirst(0);
                pop++;
                break;
            case DIK_1:
                UIP_SliderInput->setfirst(1);
                pop++;
                break;
            case DIK_2:
                UIP_SliderInput->setfirst(2);
                pop++;
                break;
            case DIK_3:
                UIP_SliderInput->setfirst(3);
                pop++;
                break;
            case DIK_4:
                UIP_SliderInput->setfirst(4);
                pop++;
                break;
            case DIK_5:
                UIP_SliderInput->setfirst(5);
                pop++;
                break;
            case DIK_6:
                UIP_SliderInput->setfirst(6);
                pop++;
                break;
            case DIK_7:
                UIP_SliderInput->setfirst(7);
                pop++;
                break;
            case DIK_8:
                UIP_SliderInput->setfirst(8);
                pop++;
                break;
            case DIK_9:
                UIP_SliderInput->setfirst(9);
                pop++;
                break;
            case DIK_MINUS:
                UIP_SliderInput->setfirst(-1);
                pop++;
                break;
        }
        if (pop) {
            key = Keys.getkey();    
            if (cur_state != STATE_PEDIT_WIN) {
                need_refresh++;
                need_redraw++;

                popup_window(UIP_SliderInput);          
            }
        }
        if (act) {
            key = Keys.getkey();
            need_refresh++;
            need_redraw++;

        }
    }
    if (value<min) value=min;
    if (value>max) value=max;
    return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
void ValueSliderDL::draw(Drawable *S, int active) {
    int cx,cy=y;
    TColor col;
    char str[32];
    changed = 0;
    for(cx=x;cx<x+xsize;cx++) {
        printBG(col(cx),row(cy)," ",COLORS.Text,COLORS.EditBG,S);
    }
    if (value>999)
        sprintf(str,"INF");
    else
        sprintf(str,"%.3d",value);
    printBG(col(x+xsize+1),row(cy),str,COLORS.Text,COLORS.Background,S);
    if (active)
        col = COLORS.Highlight;
    else
        col = COLORS.EditText;
    printchar(col(x)+(((value-min)*(((xsize-1)*8)-1))/(max-min))+1,row(cy),155,col,S);//156
    if (frame) {
        printline(col(x),row(y-1),0x86,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+1),0x81,xsize,COLORS.Highlight,S);
        printchar(col(x-1),row(y),0x84,COLORS.Lowlight,S);
        printchar(col(x+xsize),row(y),0x83,COLORS.Highlight,S);
    }
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+5),row(y+1));
    changed = 0;
}



