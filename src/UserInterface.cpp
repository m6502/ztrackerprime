/*************************************************************************
 *
 * FILE  UserInterface.cpp
 * $Id: UserInterface.cpp,v 1.73 2002/06/12 00:22:35 zonaj Exp $
 *
 * DESCRIPTION 
 *   User interface functions.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2000-2001, Christopher Micali <micali@concentric.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their
 *    contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS´´ AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******/
#include "zt.h"

/* OO UI code - YUM - Can you tell this is the first time i've done this? */

int needaclear = 0;




// ------------------------------------------------------------------------------------------------
//
//
void dev_sel(int dev, MidiOutDeviceSelector *mds ) 
{
  LBNode *p = mds->getNode(dev);
  
  if (p) {

    dev = mds->findItem(p->caption);
    if (dev < MidiOut->GetNumOpenDevs()) song->instruments[cur_inst]->midi_device = MidiOut->GetDevID(dev);
  }
}




// ------------------------------------------------------------------------------------------------
//
//
void midi_out_sel(int dev) {
    int a;
    char *errmsg;
    if ((unsigned int)dev < MidiOut->numOuputDevices) {
        if (MidiOut->QueryDevice(dev)) {
            MidiOut->RemDevice(dev);
            sprintf(szStatmsg,"Closed[out]: %s",(MidiOut->outputDevices[dev]->alias != NULL)?MidiOut->outputDevices[dev]->alias:MidiOut->outputDevices[dev]->szName);
        } else {
            if (a = MidiOut->AddDevice(dev)) {
                switch(a) {
                case MIDIERR_NODEVICE:
                    errmsg = "No device (mapper open?)";
                    break;
                case MMSYSERR_NOMEM:
                    errmsg = "Out of memory (reboot)";
                    break;
                case MMSYSERR_ALLOCATED:
                    errmsg = "Device busy";
                    break;
                default:
                    errmsg = "Unknown error";
                    break;
                }
                sprintf(szStatmsg,"Could not open %s (%d: %s)",(MidiOut->outputDevices[dev]->alias != NULL)?MidiOut->outputDevices[dev]->alias:MidiOut->outputDevices[dev]->szName,a,errmsg);
                statusmsg = szStatmsg;
                status_change = 1;
            } else {
                sprintf(szStatmsg,"Opened[out]: %s",(MidiOut->outputDevices[dev]->alias != NULL)?MidiOut->outputDevices[dev]->alias:MidiOut->outputDevices[dev]->szName);
            }
        }
    }
}




// ------------------------------------------------------------------------------------------------
//
//
void midi_in_sel(int dev) {
    int a;
    char *errmsg;
    if ((unsigned int)dev < MidiIn->numMidiDevs) {
        if (MidiIn->QueryDevice(dev)) {
            MidiIn->RemDevice(dev);
            sprintf(szStatmsg,"Closed[in]: %s",MidiIn->midiInDev[dev]->szName);
        } else {
            if (a = MidiIn->AddDevice(dev)) {
                switch(a) {
                case MIDIERR_NODEVICE:
                    errmsg = "No device (mapper open?)";
                    break;
                case MMSYSERR_NOMEM:
                    errmsg = "Out of memory (reboot)";
                    break;
                case MMSYSERR_ALLOCATED:
                    errmsg = "Device busy";
                    break;
                default:
                    errmsg = "Unknown error";
                    break;
                }
                sprintf(szStatmsg,"Could not open %s (%d: %s)",MidiIn->midiInDev[dev]->szName,a,errmsg);
                statusmsg = szStatmsg;
                status_change = 1;
            } else {
                sprintf(szStatmsg,"Opened[in]: %s",MidiIn->midiInDev[dev]->szName);
            }
        }
    }
}





// ------------------------------------------------------------------------------------------------
//
//
UserInterface::UserInterface(void) {
    num_elems = cur_element = 0;
    UIEList = NULL;
    GFXList = NULL;
    this->dontmess = 0;
    
}




// ------------------------------------------------------------------------------------------------
//
//
UserInterface::~UserInterface(void) {
    reset();
}




// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::reset(void) {
    UserInterfaceElement *e;
    while (UIEList) {
        e = UIEList->next;
        delete UIEList;
        UIEList = e;
    }
    while (GFXList) {
        e = GFXList->next;
        delete GFXList;
        GFXList = e;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::set_redraw(int ID) {
    UserInterfaceElement *e = UIEList;
    while(e) {
        if (e->ID == ID) {
            e->need_redraw = 1;
            break;
        }
        e = e->next;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::full_refresh() {
    UserInterfaceElement *e = UIEList;
    while(e) {
        e->need_redraw = 1;
        e = e->next;
    }
    e = this->GFXList;
    while(e) {
        e->need_redraw = 1;
        e = e->next;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::enter(void) {
    UserInterfaceElement *e = UIEList;
    while(e) {
        e->enter();
        e = e->next;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::set_focus(int id) {
    cur_element = id;
    this->full_refresh();
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::update()
{
  KBKey key,act=0, kstate;
  int a, new_redraw = 0;
  int o;

  UserInterfaceElement *e = UIEList ;

  while(e) {

    o = cur_element;
    cur_element = e->mouseupdate(cur_element);
    
    if (o!=cur_element) full_refresh();
    
    if (e->ID == cur_element) {

      a = e->update();
      
      if(a) {

        set_redraw(cur_element); new_redraw++;
      
        if (a==1) cur_element++;
        else cur_element--;
      }
    }

    e = e->next;
  }


  if (!dontmess) {

    key = Keys.checkkey();
    kstate = Keys.getstate();
    
    if (key == DIK_TAB) SDL_Delay(1);

    if (key) {

      ////////////////////////////////////////////////////////////////////////
      // MODIFIED FOR SHIFT-TABBING

      if(kstate == KS_SHIFT) {

        switch(key)
        {
          case DIK_TAB:
            set_redraw(cur_element); new_redraw++;
            cur_element--;
            /*
            if(cur_element <= 1 || cur_element >= num_elems)
            {
            cur_element = num_elems-1;
            }
            else
            {
            cur_element--;
            }*/
            act++;
            break;
        } ;
      }
      else {
        
        if (kstate == KS_NO_SHIFT_KEYS) {

          switch(key)
          {
            case DIK_TAB:
              set_redraw(cur_element); new_redraw++;
              cur_element++; act++; 
              break;
          } ;
        }
      }

      //////////////////////////////////////////////////////////////////////////
    }
    
    if (act) {

      key = Keys.getkey();
      need_refresh++;
    }
  }

  if (cur_element == num_elems) cur_element = 0 ;
  else {
 
    if (cur_element < 0 || cur_element > num_elems) cur_element = num_elems-1 ;
  }
  
  if (new_redraw) set_redraw(cur_element);
}




// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::draw(Drawable *S) {
    UserInterfaceElement *e = UIEList;
    if (needaclear) {
        S->fillRect(col(1),row(12),INTERNAL_RESOLUTION_X,424,COLORS.Background);
        screenmanager.Update(col(1),row(12),INTERNAL_RESOLUTION_X,424);
    }
    needaclear=0;
    while(e) {
        if (e->need_redraw) {
            e->draw(S,(e->ID == cur_element));
            e->need_redraw = 0;
        }
        e = e->next;
    }
    e = GFXList;
    while(e) {
        if (e->need_redraw) {
            e->draw(S,0);
            e->need_redraw = 0;
        }
        e = e->next;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::add_element(UserInterfaceElement *uie, int ID) {
        uie->next = UIEList;
        UIEList = uie;
        uie->ID = ID;
        num_elems++;
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterface::add_gfx(UserInterfaceElement *uie, int ID) {
        uie->next = GFXList;
        GFXList = uie;
        uie->ID = ID;
}



// ------------------------------------------------------------------------------------------------
//
//
UserInterfaceElement* UserInterface::get_element(int ID) {
    UserInterfaceElement *e = UIEList;
    while(e) {
        if (e->ID == ID)
            return e;
        e=e->next;
    }
    return NULL;
}



// ------------------------------------------------------------------------------------------------
//
//
UserInterfaceElement* UserInterface::get_gfx(int ID) {
    UserInterfaceElement *e = GFXList;
    while(e) {
        if (e->ID == ID)
            return e;
        e=e->next;
    }
    return NULL;
}



// ------------------------------------------------------------------------------------------------
//
//
UserInterfaceElement::UserInterfaceElement(void) {
    changed = 1; x = 0; y = 0; xsize = 0; ysize = 1; mousestate=0; frame=1;
    next = NULL;
    need_redraw = 1;
}



// ------------------------------------------------------------------------------------------------
//
//
UserInterfaceElement::~UserInterfaceElement(void) {
}



// ------------------------------------------------------------------------------------------------
//
//
void UserInterfaceElement::enter(void) {
    
}



// ------------------------------------------------------------------------------------------------
//
//
int UserInterfaceElement::mouseupdate(int cur_element) 
{
    KBKey key,act=0;
    key = Keys.checkkey();
    if (key) {
        switch(key) {
            case DIK_MOUSE_1_ON:
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
                    mousestate = 1;
                    act++;
                }
                break;
            case DIK_MOUSE_1_OFF:
                if (mousestate) act++;
                mousestate=0;
                break;
        }
    }
    if (act)  key = Keys.getkey();
    if (cur_element != this->ID && mousestate && act) {
        need_refresh++;
        need_redraw++;
        return this->ID;
    }

    return cur_element;
}




// ------------------------------------------------------------------------------------------------
//
//
TextInput::TextInput(void) {
    cursor = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
TextInput::~TextInput(void) {
}




// ------------------------------------------------------------------------------------------------
//
//
int TextInput::update() {
    KBKey key,act=0,ret=0;
    signed int retletter =0;
    int i;
    key = Keys.checkkey();
    unsigned char kstate = Keys.getstate();
    if (key) {
        switch(key) {
            case DIK_UP: ret = -1; act++; break;
            case DIK_DOWN: ret = 1; act++; break;
            case DIK_LEFT: cursor--;  act++; break;
            case DIK_RIGHT: if (str[cursor]) cursor++; act++; break;
            case DIK_A: retletter=1; act++; break;
            case DIK_B: retletter=2; act++; break;
            case DIK_C: retletter=3; act++; break;
            case DIK_D: retletter=4; act++; break;
            case DIK_E: retletter=5; act++; break;
            case DIK_F: retletter=6; act++; break;
            case DIK_G: retletter=7; act++; break;
            case DIK_H: retletter=8; act++; break;
            case DIK_I: retletter=9; act++; break;
            case DIK_J: retletter=10; act++; break;
            case DIK_K: retletter=11; act++; break;
            case DIK_L: retletter=12; act++; break;
            case DIK_M: retletter=13; act++; break;
            case DIK_N: retletter=14; act++; break;
            case DIK_O: retletter=15; act++; break;
            case DIK_P: retletter=16; act++; break;
            case DIK_Q: retletter=17; act++; break;
            case DIK_R: retletter=18; act++; break;
            case DIK_S: retletter=19; act++; break;
            case DIK_T: retletter=20; act++; break;
            case DIK_U: retletter=21; act++; break;
            case DIK_V: retletter=22; act++; break;
            case DIK_W: retletter=23; act++; break;
            case DIK_X: retletter=24; act++; break;
            case DIK_Y: retletter=25; act++; break;
            case DIK_Z: retletter=26; act++; break;
            case DIK_0: retletter=0xFF; act++; break;
            case DIK_1: retletter=0xFF; act++; break;
            case DIK_2: retletter=0xFF; act++; break;
            case DIK_3: retletter=0xFF; act++; break;
            case DIK_4: retletter=0xFF; act++; break;
            case DIK_5: retletter=0xFF; act++; break;
            case DIK_6: retletter=0xFF; act++; break;
            case DIK_7: retletter=0xFF; act++; break;
            case DIK_8: retletter=0xFF; act++; break;
            case DIK_9: retletter=0xFF; act++; break;
            case DIK_SPACE: retletter=0xff; act++; break;
            case DIK_PERIOD: retletter=0xff; act++; break;
            case DIK_COMMA: retletter=0xff; act++; break;
            case DIK_BACKSLASH: retletter=0xff; act++; break;
            case DIK_MINUS: retletter=0xff; act++; break;
            case DIK_EQUALS: retletter=0xff; act++; break;
            case DIK_BACKSPACE: retletter=0xFFF; act++; break;
            case DIK_DELETE: retletter=0xFFF; act++; break;
            case DIK_SEMICOLON: retletter = 0xfff; act++; break;
        }
        if (act) {
            key = Keys.getkey();
            need_refresh++;
            need_redraw++;

            if (retletter) {
                this->changed = 1;
                if (retletter != 0xFFF) { // Regular Key
                    for(i=length-1;i>cursor;i--) {
                        str[i] = str[i-1];
                    }
                    if (cursor<length) {
                        if (retletter != 0xff) {
                            if (kstate&KS_SHIFT) 
                                str[cursor] = ((int)retletter + 'A' - 1);
                        else    
                                str[cursor] = ((int)retletter + 'a' - 1);
                        } else {    
                            switch(key) {
                                case DIK_SEMICOLON: if (kstate&KS_SHIFT)  str[cursor] = ':'; else str[cursor] = ';'; break;
                                case DIK_SPACE:  str[cursor] = ' '; break;
                                case DIK_PERIOD: str[cursor] = '.'; break;
                                case DIK_COMMA:  str[cursor] = ','; break;
                                case DIK_BACKSLASH:  str[cursor] = '\\'; break;
                                case DIK_MINUS:  str[cursor] = '-'; break;
                                case DIK_EQUALS: str[cursor] = '='; break;
                                case DIK_0: str[cursor] = '0'; break;
                                case DIK_1: str[cursor] = '1'; break;
                                case DIK_2: str[cursor] = '2'; break;
                                case DIK_3: str[cursor] = '3'; break;
                                case DIK_4: str[cursor] = '4'; break;
                                case DIK_5: str[cursor] = '5'; break;
                                case DIK_6: str[cursor] = '6'; break;
                                case DIK_7: str[cursor] = '7'; break;
                                case DIK_8: str[cursor] = '8'; break;
                                case DIK_9: str[cursor] = '9'; break;
                            }
                        }
                        cursor++;
                    }
                } else { // Is backspace/del
                    if (key==DIK_BACKSPACE)
                        cursor--;
                    if (key!=DIK_BACKSPACE || (cursor>=0)) {  /* FIX THIS SHIT! */
                        if (key == DIK_BACKSPACE && cursor == length-1) {
                            str[length-2] = str[length-1];
                            str[length-1] = '\0';
                        } else {
                            str[length-1] = '\0';
                            for(i=cursor;i<length-1;i++)
                                str[i] = str[i+1];
                        }
                    }
                } // End RetLetter
            }
        }
        if (cursor<0) cursor=0;
        if (cursor>length-1) cursor=length-1;
    }
    return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
void TextInput::draw(Drawable *S, int active) {
    int cx,cy=y,instr=1,i=0;
    char s[2];
    TColor f,b;
    s[1]=0;
    for(cx=x;cx<x+xsize;cx++) {
        if (instr) {
            if (str[i])
                s[0] = str[i];
            else {
                instr=0;
                s[0] = ' ';
            }
        }   
        b = COLORS.EditBG;
        f = COLORS.EditText;
        if (active && i==cursor) {
                f = COLORS.EditBG;
                b = COLORS.Highlight;
        }
        printBG(col(cx),row(cy),&s[0],f,b,S); i++;
    }
    changed = 0;
    if (frame) {
        printline(col(x),row(y-1),0x86,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+1),0x81,xsize,COLORS.Highlight,S);
        printchar(col(x-1),row(y),0x84,COLORS.Lowlight,S);
        printchar(col(x+xsize),row(y),0x83,COLORS.Highlight,S);
    }
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+1));

    this->changed = 0;
}





// ------------------------------------------------------------------------------------------------
//
//
CheckBox::CheckBox(void) {
    value = NULL;
}



// ------------------------------------------------------------------------------------------------
//
//
CheckBox::~CheckBox(void) {
}




// ------------------------------------------------------------------------------------------------
//
//
int CheckBox::mouseupdate(int cur_element) {
    KBKey key,act=0;
    key = Keys.checkkey();
    if (key) {
        switch(key) {
            case DIK_MOUSE_1_ON:
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
                    mousestate = 1;
                    act++;
                }
                break;
            case DIK_MOUSE_1_OFF:
                if (mousestate) {
                    act++;
                    if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
                        if (*value)
                            *value = 0;
                        else
                            *value = 1;
                        need_refresh++;
                        need_redraw++;

                        changed++; 
                        fixmouse++;
                    }
                }
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
int CheckBox::update() {
    KBKey key,act=0;
    int ret=0;
    key = Keys.checkkey();
    if (key) {
        switch(key) {
            case DIK_UP: ret = -1; act++; break;
            case DIK_DOWN: ret = 1; act++; break;
            case DIK_SPACE: 
                if (*value)
                    *value = 0;
                else
                    *value = 1;
                act++; changed++; break;
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
void CheckBox::draw(Drawable *S, int active) {
    int cx,cy=y;
    TColor f,b;
    char *str;
    for(cx=x;cx<x+xsize;cx++) {
        printBG(col(cx),row(cy)," ",COLORS.Text,COLORS.EditBG,S);
    }
    if (*value)
        str = "On";
    else
        str = "Off";
    if (active) {
        f = COLORS.EditBG;
        b = COLORS.Highlight; 
    } else {
        f = COLORS.EditText;
        b = COLORS.EditBG;
    }
    printBG(col(x),col(y),str,f,b,S);
    if (frame) {
        printline(col(x),row(y-1),0x86,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+1),0x81,xsize,COLORS.Highlight,S);
        printchar(col(x-1),row(y),0x84,COLORS.Lowlight,S);
        printchar(col(x+xsize),row(y),0x83,COLORS.Highlight,S);
    }
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+1));
    changed = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
Frame::Frame(void) {
    type = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
Frame::~Frame(void) {
}




// ------------------------------------------------------------------------------------------------
//
//
int Frame::update() {
    return 0; // This doesnt get key actions (it's never active)
}




// ------------------------------------------------------------------------------------------------
//
//
void Frame::draw(Drawable *S, int active) {
    int cy;

    if (type) {

        printline(col(x),row(y-1),0x81,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+ysize),0x86,xsize,COLORS.Highlight,S);
        for (cy=y;cy<y+ysize;cy++) {
            printchar(col(x-1),row(cy),0x83,COLORS.Lowlight,S);
            printchar(col(x+xsize),row(cy),0x84,COLORS.Highlight,S);
        }
    } else {

        printline(col(x),row(y-1),0x86,xsize,COLORS.Lowlight,S);
        printline(col(x),row(y+ysize),0x81,xsize,COLORS.Highlight,S);
        for (cy=y;cy<y+ysize;cy++) {
            printchar(col(x-1),row(cy),0x84,COLORS.Lowlight,S);
            printchar(col(x+xsize),row(cy),0x83,COLORS.Highlight,S);
        }
    }
}











// ------------------------------------------------------------------------------------------------
//
//
GfxButton::GfxButton(void) {
    OnClick = NULL;
    state = 0;
    bmDefault = NULL;
    bmOnMouseOver = NULL;
    bmOnClick = NULL;
    StuffKey = -1;
    StuffKeyState = KS_NO_SHIFT_KEYS;
}




// ------------------------------------------------------------------------------------------------
//
//
GfxButton::~GfxButton(void) {
    if (bmDefault)
        delete bmDefault;
    if (bmOnMouseOver)
        delete bmOnMouseOver;
    if (bmOnClick)
        delete bmOnClick;
/*
    if (bmDefault)
        RELEASEINT(bmDefault);
    if (bmOnMouseOver)
        RELEASEINT(bmOnMouseOver);
    if (bmOnClick)
        RELEASEINT(bmOnClick);
*/
  }




// ------------------------------------------------------------------------------------------------
//
//
int GfxButton::mouseupdate(int cur_element) {
    KBKey key,act=0,bustit=0;
    key = Keys.checkkey();
    if (mousestate) {
        if (checkmousepos(this->x,this->y,this->x+this->xsize,this->y+this->ysize)) {
            if (state != 3) {
                state = 3;
                act++;
                changed++;
                fixmouse++;
                    need_refresh++;
            need_redraw++;

            }
        } else {
            if (state != 0) {
                state = 0;
                act++;
                changed++;
                fixmouse++;
                    need_refresh++;
            need_redraw++;

            }
        }
        
    } else {
        if (bmOnMouseOver) {
            if (checkmousepos(this->x,this->y,this->x+this->xsize,this->y+this->ysize)) {
                if (state == 0) {
                    state = 1;
                    act++;
                    changed++;
                    fixmouse++;
                        need_refresh++;
            need_redraw++;

                }
            } else {
                if (state == 1) {
                    state = 0;
                    act++;
                    changed++;
                    fixmouse++;
                        need_refresh++;
            need_redraw++;

                }
            }       
        }
    }
    if (key) {
        switch(key) {
            case DIK_MOUSE_1_ON:
                if (checkclick(this->x,this->y,this->x+this->xsize,this->y+this->ysize)) {
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
                    state = 0;
                    if (checkclick(this->x,this->y,this->x+this->xsize,this->y+this->ysize)) {
                            need_refresh++;
            need_redraw++;

                        changed++; 
                        fixmouse++;
                        bustit = 1;
                        if (this->OnClick)
                            this->OnClick(this);
                        if (this->bmOnMouseOver)
                            state = 1; else
                            state = 2;
                    }
                }
                mousestate = 0;
                break;
        }
    }
    if (act) {
        key = Keys.getkey();
        if (StuffKey != -1 && bustit)
            Keys.insert(StuffKey,StuffKeyState);
    }
    if (cur_element != this->ID && mousestate && act) {
            need_refresh++;
            need_redraw++;
        //fixmouse++;
        return this->ID;
    }
    return cur_element;
}

// ------------------------------------------------------------------------------------------------
//
//
int GfxButton::update() {
    return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void GfxButton::draw(Drawable *S, int active) {

//  if (active) 
//      print(col(x),row(y),caption,COLORS.Highlight,S);
//  else
//      print(col(x),row(y),caption,COLORS.Text,S);

    switch(state) {
        case 1: // MouseOver
            if (this->bmOnMouseOver)
                S->copy(this->bmOnMouseOver,x,y);
            break;
        case 3: // Click
            if (this->bmOnClick)
                S->copy(this->bmOnClick,x,y);
            break;
        default: // Default
            if (this->bmDefault)
                S->copy(this->bmDefault,x,y);
            break;
    }
    screenmanager.Update(x,y,x+xsize,y+ysize);
    changed = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
VUPlay::VUPlay() {
    for(int i=0; i < 64; i++)
        latency[i].e.note = latency[i].longevity = latency[i].init_longevity = latency[i].init_vol = 0;
    starttrack = 0;
}

// ------------------------------------------------------------------------------------------------
//
//
VUPlay::~VUPlay() {

}

// ------------------------------------------------------------------------------------------------
//
//
int VUPlay::update() {

    KBKey key,act=0;
    int kstate;
    key = Keys.checkkey();
    kstate = Keys.getstate();
    int SPEED = 2; // speed of fading

    switch(key)
    {
    case DIK_DOWN:
        if(starttrack < MAX_TRACKS - 32)
            starttrack++;
        break;
    case DIK_UP:
        if(starttrack > 0)
            starttrack--;
        break;
    default:
        break;
    };

    if (ztPlayer->playing)
    {
        if (this->cur_row != ztPlayer->playing_cur_row) {
            for(int i = 0; i < 64; i++)
                if(latency[i].longevity - SPEED > 0)
                    latency[i].longevity -=SPEED;
                else
                    latency[i].longevity = 0;
        }
    }
    need_refresh++;
    need_redraw++;

    return(0);
}




// ------------------------------------------------------------------------------------------------
//
//
void VUPlay::draw(Drawable *S, int active) 
{
  char str[72];
  int ctrack;
  TColor color;
  event *e;
  int pattern;
  int x = 4;
  int y = 15;
  int tracksize = 128;
  int vol_len;
  int i;
  
  if(ztPlayer->playing && (this->cur_row < ztPlayer->cur_row - 2 || 
    this->cur_row > ztPlayer->cur_row)) {
  
    pattern = ztPlayer->playing_cur_pattern;
    cur_row = ztPlayer->playing_cur_row;
    cur_order = ztPlayer->playing_cur_order;
    
    sprintf(str," Tk  Instrument               Note  Vol  FX    Length ................");
    
    printBG(col(x),row(y - 1),str,COLORS.Text,COLORS.Background,S);
    color = COLORS.EditText;

    for (ctrack = starttrack; ctrack < starttrack + num_channels; ctrack++) {

      e = song->patterns[pattern]->tracks[ctrack]->get_event(cur_row);
      vol_len = this->draw_one_row(str,e, ctrack);
      printBG(col(x),row(y + 1 + ctrack - starttrack),str,color,(ctrack%2)?COLORS.EditBGlow:COLORS.Black,S);
      
      // Print the bar
      
      if(vol_len > 0) {

        //strcpy(str,"                ");
        strcpy(str,"");

//        for(i = 0; i < 17; str[i++] = '\0') ;
//        for(i = 0; i < vol_len; str[i++] = '=');
        
        // <Manu> Cambio estas dos lineas guarrisimas comentadas por estos dos fors()
       
        for(i = 0; i < 17; i++) {
        
          str[i] = '\0' ;
        }
        
        for(i = 0; i < vol_len; i++) {
        
          str[i] = '=' ;
        }
        
        printBG(col(x+54),row(y + 1 + ctrack - starttrack),str,COLORS.Highlight,COLORS.EditBGhigh,S);
      }
    }
  }
  else {
    
    if(! ztPlayer->playing) {
      
      color = COLORS.EditText;

      for (ctrack = starttrack; ctrack < starttrack + num_channels; ctrack++) {
        
        e = NULL;
        this->draw_one_row(str,e, ctrack);
        printBG(col(x),row(y + 1 + ctrack - starttrack),str,color,(ctrack%2)?COLORS.EditBGlow:COLORS.Black,S);
      }
    }
  }
  
  // Display the outline
  
  S->drawHLine((y+num_channels+1)*8,(x)*8,(x+71)*8,COLORS.Highlight);
  S->drawVLine((x+71)*8,y*8,(y+num_channels)*8,COLORS.Highlight); // why doesn't this work ?
  
  screenmanager.UpdateWH(col(x),row(y),col(x+71),row(y+28));//num_channels)); why oens'tthis work either
}




// ------------------------------------------------------------------------------------------------
//
//
int VUPlay::draw_one_row(char str[72], event *e, int track) {

    char *w;
    int measure;
    char c[2] = { (char)'=', (char)0 };

    //c = 155;
    measure = 0;
    sprintf(str,"");
    // New Data
    if(e && e->note <128) {
        latency[track].e = *e;
        latency[track].longevity = latency[track].init_longevity = e->length ? e->length : song->instruments[e->inst]->default_length;
        latency[track].init_vol = (e->vol < 128) ? e->vol : song->instruments[e->inst]->default_volume;
        measure = latency[track].init_vol / 16 * 127;
    }
    else if(e)
    {
        if(e->note > 0x80)
            latency[track].longevity = 0;
        if(e->effect_data)
            latency[track].e.effect_data = e->effect_data;
    }

    // Let's display the Information !
    if( e == NULL || !latency[track].longevity) // || e->note > 127)
        sprintf(str,"\
 %.2d                                                                   ", track);
    else
        if(latency[track].e.note)
        {
            sprintf(str,"\
 %.2d  %s  %.2d  %.3d  %.5d  %.5d",
track,
song->instruments[latency[track].e.inst]->title,
latency[track].e.note,
(latency[track].e.vol < 128)?latency[track].e.vol:song->instruments[latency[track].e.inst]->default_volume,
latency[track].e.effect_data, latency[track].longevity);

        // add some bars...

        strcat(str," ");
        if(!measure) {
          
          // <Manu> Sin cast daba un warning
          //measure = ((float)latency[track].init_vol - (float)((float)(1 - (float)((float)latency[track].init_vol * (float)((float)latency[track].longevity / (float)latency[track].init_longevity )))* (float)latency[track].init_vol) / 10);
            measure = (int) ((float)latency[track].init_vol - (float)((float)(1 - (float)((float)latency[track].init_vol * (float)((float)latency[track].longevity / (float)latency[track].init_longevity )))* (float)latency[track].init_vol) / 10);
            measure /= 100;
        }
        
        // we really want to be safe about this unscientific calculation

        if(measure > 16)
            measure = 16;
        if(measure < 0)
            measure = 0;
        //for(; measure--; strcat(str, c)); // 155 = 9B
    }

    // Cleanup
    for(w = &str[strlen(str)]; w != &str[71]; w++)
        *w = ' ';
    *w = '\0';

    return(measure);
}




// ------------------------------------------------------------------------------------------------
//
//
BarGraph::BarGraph() {
    value = maxval = max = trackmaxval = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
BarGraph::~BarGraph() {
}



// ------------------------------------------------------------------------------------------------
//
//
int BarGraph::update() {
    return 0;
}



// ------------------------------------------------------------------------------------------------
//
//
void BarGraph::draw(Drawable *S, int active) {
    int howfar,maxlen;

    howfar = value*(xsize-1)/max;
    maxlen = maxval*(xsize-1)/max;
    for (int cy=y;cy<y+ysize;cy++) {
        S->drawHLine(cy,x,x+xsize-1,COLORS.LCDLow);
        S->drawHLine(cy,x,x+howfar,COLORS.LCDMid);
    }
    if (this->trackmaxval)
        S->drawVLine(x+maxlen,y,y+ysize-1,COLORS.LCDHigh);
    S->drawHLine(y,x,x+xsize,COLORS.Lowlight);
    S->drawVLine(x,y,y+ysize-1,COLORS.Lowlight);
    S->drawHLine(y+ysize-1,x,x+xsize,COLORS.Highlight);
    S->drawVLine(x+xsize,y,y+ysize-1,COLORS.Highlight);
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+1));
    this->changed = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
void BarGraph::setvalue(int val) {
    if (this->value == val) return;
    if (val>max) val = max;
    if (trackmaxval) {
        if (this->value<val) 
            maxval = val;
    }
    this->value = val;
    need_refresh++;
    need_redraw++;
    this->changed = 1;
}





// ------------------------------------------------------------------------------------------------
//
//
LCDDisplay::LCDDisplay() {
    xsize = 0;
    ysize = 8*3;
}



// ------------------------------------------------------------------------------------------------
//
//
LCDDisplay::~LCDDisplay() {
}



// ------------------------------------------------------------------------------------------------
//
//
int LCDDisplay::update() {
    return 0;
}



// ------------------------------------------------------------------------------------------------
//
//
void LCDDisplay::draw(Drawable *S, int active) {
    if (!xsize)
        xsize = length*8*3;
    printLCD(x+1,y+1,istr,S);
    x-=2;
    S->drawHLine(y,x,x+xsize,COLORS.Lowlight);
    S->drawVLine(x,y,y+ysize-1,COLORS.Lowlight);
    S->drawHLine(y+ysize,x,x+xsize,COLORS.Highlight);
    S->drawVLine(x+xsize,y,y+ysize-1,COLORS.Highlight);
    x+=2;
    screenmanager.UpdateWH(x,y,xsize,ysize);
    this->changed = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
void LCDDisplay::setstr(char *s) {
    str = istr;
    strcpy(istr,s);
    need_redraw++;
    need_refresh++;
    changed = 1;
}




// ------------------------------------------------------------------------------------------------
//
//
AboutBox::AboutBox() {
    xsize = 0;
    ysize = 8*3;
}




// ------------------------------------------------------------------------------------------------
//
//
AboutBox::~AboutBox() {
}




// ------------------------------------------------------------------------------------------------
//
//
int AboutBox::update() {
    need_refresh++;
    this->need_redraw++;
    return 0;
}






// ------------------------------------------------------------------------------------------------
//
//
void AboutBox::draw(Drawable *S, int active) {
/*
    if (!xsize)
        xsize = length*8*3;
    printLCD(x+1,y+1,str,S);
    x-=2;
    S->drawHLine(y,x,x+xsize,COLORS.Lowlight);
    S->drawVLine(x,y,y+ysize-1,COLORS.Lowlight);
    S->drawHLine(y+ysize,x,x+xsize,COLORS.Highlight);
    S->drawVLine(x+xsize,y,y+ysize-1,COLORS.Highlight);
    x+=2;
*/  
    q += S->surface->pitch;
    int i=q;
    Uint32 *b = (Uint32 *)S->surface->pixels;
    for (int iy = col(y); iy < col(y+ysize+1); iy++)    
        for (int ix = col(x); ix < col(x+xsize); ix++)  {  
            int bpp = S->surface->format->BytesPerPixel;
            Uint8 *p = (Uint8 *)S->surface->pixels + iy * S->surface->pitch + ix * bpp;
            *(Uint32 *)p = i++;
        }
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+1));
    frm.type=0;
    frm.x=x; frm.y=y; frm.xsize=xsize; frm.ysize=ysize+1;
    frm.draw(S,0);
    this->changed = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
TextBox::TextBox(int ro) 
{
  readonly = ro;
  mousestate = soff = startline = 0;
  bEof = false;
  bUseColors = 1;
  bWordWrap = false;
}


// ------------------------------------------------------------------------------------------------
//
//
TextBox::~TextBox() {

}





// ------------------------------------------------------------------------------------------------
//
//
int TextBox::mouseupdate(int cur_element) 
{
  KBKey key,act=0;
  key = Keys.checkkey();

  if (!bMouseIsDown) mousestate = 0;
  
  if(mousestate) {
    
    int i = (LastY/8) - this->y ;
    int s = startl + (starti-i) ;

    if (s!=startline) {

      if (! (s>startline && bEof)) {

        act++;
        startline = s;
        if (startline<0) startline = 0 ;
      }
    }
  }


  if(key) {

    switch(key) 
    {
    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------

    case DIK_MOUSE_1_ON:

      if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize+1))) {
        int i = (MousePressY/8) - this->y;
        mousestate = 1;
        startl = startline;
        starti = i;
        act++;
      }

      break;

    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------

    case DIK_MOUSE_1_OFF:

      if (mousestate) {
        act++;
      }

      mousestate=0;

      break;

    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------

    default:

      break ;
    } ;
  }



  if(act) {

    key=Keys.getkey() ;

    need_refresh++;
    need_redraw++;
    return(this->ID) ;
  }
  
  return(cur_element) ;
}



// ------------------------------------------------------------------------------------------------
//
//
int TextBox::update() 
{
  KBKey key,act=0;
  int ret=0;

  key = Keys.checkkey();

  if (key) {

    switch(key) {
      //case SDLK_m
    case DIK_UP  : act++; startline--;  break;
    case DIK_DOWN: act++; if (!bEof) startline++;  break;
    case DIK_PGUP: act++; startline-=16;  break;
    case DIK_PGDN: act++; if (!bEof) startline+=16;  break;
    case DIK_LEFT: act++; soff--; break;
    case DIK_RIGHT:act++; soff++; break;
    case DIK_HOME: if (soff>0) soff=0; else startline=0; act++; break;
    case DIK_SPACE:  //  / Same thing
    case DIK_RETURN: //  \ Same thingg
      break;
    }
    if (act) {

      Keys.getkey() ;
      need_refresh++ ;
      need_redraw++ ;
    }
  }
  
  if (startline<0) startline = 0;
  if (soff<0) soff=0;
  
  return(ret) ;
}




// ------------------------------------------------------------------------------------------------
//
//
int nextline(char *str, int p) {
    while(str[p]) {
        if (str[p]=='\n')
            return p+1;
        p++;
    }
    return -1;
}




// ------------------------------------------------------------------------------------------------
//
//
void TextBox::draw(Drawable *S, int active) 
{
  int line=0, done = 0, sc=0,d=0, cx;
  TColor use = COLORS.EditText;

  //    for(int cy=col(y); cy<col(y+ysize+1); cy++) 
  //        S->drawHLine(cy, row(x), row(x+xsize), 0);
  S->fillRect(row(x),col(y),row(x+xsize)-1,col(y+ysize+1)-1,CurrentSkin->Colors.EditBG);
  int l=0; bEof = false;
  
  if (!text) goto skipme;

  while (l<startline) {

    d = nextline(this->text, sc);
    
    if (d==-1) {

      l = startline;
      done = 1;
    } 
    else sc = d;
    l++;
  }
  
  /*if (str[c] == '|' && str[c+1] != '|') {
  c++;
  switch(toupper(str[c])) {
  case 'H':
  use = COLORS.Highlight;
  break;
  case 'T':
  use = COLORS.Text;
  break;
  case 'U':
  use = col;
  break;
  }
  c+=2;
  } else if (str[c] == '|' && str[c+1] == '|') c++;
  */
  while(line <= ysize && !done && text[sc]) {
    d=0;
    cx = 0;
    /*
    for(int p=0;p<soff;p++)
    if (text[sc] && text[sc]!='\n')
    sc++;
    */
    int p = 0;
  
    while(text[sc] && !d) {
    
      if (text[sc]=='\n') d = 1;
      else
        switch(text[sc]) {
    
  case '|': 
  
    if (!bUseColors) goto printer;

    if (text[sc+1] == '|') goto printer;
    else {
    
      sc++;
    
      switch(toupper(text[sc]))
      {
          // --------------------------------------------------------------------
          // --------------------------------------------------------------------

      case 'H':
        use = COLORS.Highlight;
        break;

          // --------------------------------------------------------------------
          // --------------------------------------------------------------------
    
      case 'L':
        use = COLORS.Lowlight;
        break;
      
          // --------------------------------------------------------------------
          // --------------------------------------------------------------------
    
      case 'U':
        use = COLORS.EditText;
        break;
      
          // --------------------------------------------------------------------
          // --------------------------------------------------------------------
    
      default:
        break ;
      } ;

      sc++;
    }

    break;

    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------

    case '\r': 
      break;

    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------

    case '\t': 
      p++; 
      if (p>=soff) cx+=2; 
      break;

    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------

      default:
      printer:
  
        p++;
  
        if (p>=soff)  {
    
          if (cx<x+xsize-4) printchar(col(x + 1 +cx), row(y + line), text[sc], use, S);
          else {
      
            if (bWordWrap) d++;
          }
    
          cx++;
        }
  
      }
      sc++;
    }
  
    if (!text[sc]) done = 1;
    line++;
  } 

skipme:
  if (done) bEof = true;
  frm.type=0;
  frm.x=x; frm.y=y; frm.xsize=xsize; frm.ysize=ysize+1;
  frm.draw(S,0);
  screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+ysize+1));
  changed = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
ListBox::ListBox() 
{
  items = NULL;
  clear();
  is_sorted = false;
  use_key_select = true;
  empty_message = NULL;
  use_checks = false;
  check_on = 251;
  check_off = 250;
  color_itemsel = &COLORS.EditBG;
  color_itemnosel = &COLORS.EditText;
}




// ------------------------------------------------------------------------------------------------
//
//
ListBox::~ListBox() {
    clear();
}




// ------------------------------------------------------------------------------------------------
//
//
int ListBox::mouseupdate(int cur_element) 
{
  KBKey key,act=0;
  key = Keys.checkkey();
  int old_cur_sel = cur_sel;
  int old_y_start = y_start;

  color_itemsel = &COLORS.EditBG;
  color_itemnosel = &COLORS.EditText;
  
  if (!bMouseIsDown) mousestate = 0;
  
  if (mousestate) {

    int i = (LastY/8) - this->y ;

    if (cur_sel+y_start != i) {

      if (i<0) {

        if (y_start >0 ) {

          setCursor(y_start-1);
          // y_start--;
          act++;
        }
      } 
      else {

        if (i>ysize && cur_sel==ysize && y_start<num_elements-ysize-1) {
          //                y_start++;
          setCursor(y_start+cur_sel+1);
          act++;
        } 
        else {

          if (i<=ysize && i+y_start<num_elements && i+y_start>=0) {
            setCursor(i+y_start);
            act++;
          }
        }
      }
    }
  }
  
  if(key) {
    
    switch(key) 
    {
    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------

    case DIK_MOUSE_1_ON:

      if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize+1))) {
      
        int i = (MousePressY/8) - this->y;
        
        if (cur_element == this->ID && i==this->cur_sel) {
        
          LBNode *p = this->getNode(this->cur_sel+this->y_start);
          if(p) OnSelect(p) ;
        }

        mousestate = 1;

        if (i<=ysize && i<num_elements) setCursor(i+y_start);                    
        act++;
      }
      
      break;

    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------

    case DIK_MOUSE_1_OFF:

      if (mousestate) act++;

      mousestate=0;
      break;

    // ------------------------------------------------------------------------
    // ------------------------------------------------------------------------

    default:

      break ;
    } ;
  }

  if (cur_sel != old_cur_sel || y_start != old_y_start) OnSelectChange() ;


  if (act) {

    key = Keys.getkey() ;

    need_refresh++;
    need_redraw++;

    return(this->ID) ;
  }
  
  return(cur_element) ;
}





// ------------------------------------------------------------------------------------------------
//
//
int ListBox::sortstr(char *s1, char *s2) {
    int i=0;
    if (!s1 || !s2)
        return 0;
    while(s1[i] && s2[i]) {
        if (tolower(s1[i]) < tolower(s2[i])) // s1 comes before, order is correct
            return 1;
        if (tolower(s1[i]) > tolower(s2[i])) // s2 comes before, flip order
            return -1;
        i++;
    }
    if (!s1[i] && s2[i]) // s1 shorter than s2, order is ok
        return 1;
    if (s1[i] && !s2[i]) // s2 shorter than s1, flip order
        return -1;
    return 0; // they are the same
}




// ------------------------------------------------------------------------------------------------
//
//
void ListBox::strc(char *dst, char *src) {
    int i=0;
    while (i<xsize && src[i]) {
        dst[i] = src[i];
        i++;
    }
}




// ------------------------------------------------------------------------------------------------
//
//
void ListBox::clear(void) {
    while(items) {
        LBNode *p = items->next;
        delete items;
        items = p;
    }
    items = NULL;
    y_start = num_elements = cur_sel = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
LBNode *ListBox::getNode(int index) {
    if (index<0) return NULL;
    int i=0;
    LBNode *p = items;
    while (p) {
        if (index == i)
            return p;
        p = p->next;
        i++;
    }
    return NULL;
}




// ------------------------------------------------------------------------------------------------
//
//
char *ListBox::getItem(int index) {
    if (index<0) return NULL;
    LBNode *p = getNode(index);
    if (p)
        return p->caption;
    else
        return NULL;
}




// ------------------------------------------------------------------------------------------------
//
//
int ListBox::findItem(char *text) {
    int i=0;
    LBNode *p = items;
    while( p ) {
        if (zcmp(p->caption, text))
            return i;
        p = p->next;
        i++;
    }
    return -1;
}




// ------------------------------------------------------------------------------------------------
//
//
LBNode * ListBox::insertItem(char *text) {
    LBNode *p = new LBNode;
//    p->caption = strdup(text);
    if (text) {
        p->caption = new char[strlen(text)+1];
        strcpy(p->caption, text);
    }
    p->checked = false;
    if (is_sorted) {
        if (!items)
            items = p;
        else {
            if (sortstr(text,items->caption) >=0 ) { // 0 = same, 1 = order ok
                p->next = items;
                items = p;
            } else {
                LBNode *l = items;
                LBNode *n = items->next;
                while (n) {
                    if (sortstr(n->caption, text) < 0)
                        break;
                    l = n;
                    n = n->next;
                }
                p->next = n;
                l->next = p;
            }
        }
    } else {
        if (items) {
            p->next = items;
        }
        items = p;
    }
    num_elements++;
    return p;
}





// ------------------------------------------------------------------------------------------------
//
//
void ListBox::removeItem(int index) {
    if (index<0) return;
    int i=1;
    LBNode *p = items;
    LBNode *l = items;
    if (index == 0) {
        items = items->next;
        delete p;
        num_elements--;
        return;
    }
    p = p->next;
    while(p) {
        if (i==index) {
            l->next = p->next;
            delete p;
            num_elements--;
            return;
        }
        l = p;
        p = p->next;
        i++;
    }
    return;
}




// ------------------------------------------------------------------------------------------------
//
//
bool ListBox::getCheck(int index) {
    if (index<0) return false;
    LBNode *p = getNode(index);
    if (p)
        return p->checked;
    else
        return false;
}





// ------------------------------------------------------------------------------------------------
//
//
void ListBox::setCheck(int index, bool state) {
    if (index<0) return;
    LBNode *p = getNode(index);
    if (p) {
        p->checked = state;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void ListBox::selectAll() {
    LBNode *p = items;
    while(p) {
        p->checked = true;
        p = p->next;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void ListBox::selectNone() {
    LBNode *p = items;
    while(p) {
        p->checked = false;
        p = p->next;
    }
}




// ------------------------------------------------------------------------------------------------
//
//
void ListBox::setCursor(int index) {
    if (index<0) return;
    if (index>=num_elements) return;
    if (index<y_start) {
        cur_sel = 0;
        y_start = index;
    } else
    if (index>y_start+ysize) {
        cur_sel = ysize;
        y_start = index - ysize ;
    } else
        cur_sel = index-y_start;
}




// ------------------------------------------------------------------------------------------------
//
//
LBNode *ListBox::findNodeWithChar(char c, LBNode *start) {
    LBNode *work = start;
    if (start) {
        work = start->next;
        while( work != start ) {
            if (!work)
                work = items;
            if (work->next == NULL)
                start = NULL;
            char *p = work->caption;
            if (p) {
                if (tolower(p[0]) == tolower(c))
                    return work;
            }
            work = work->next;
        }
    }
    return NULL;
}





// ------------------------------------------------------------------------------------------------
//
//
int ListBox::update() {
    KBKey key,act=0;
    int ret=0;
    key = Keys.checkkey();
    int old_cur_sel = cur_sel;
    int old_y_start = y_start;
    if (key) {
        if (num_elements && use_key_select) {
            signed int retchar = key;//getKeyChar(key);
            LBNode *p = getNode(cur_sel + y_start);
            if (isalpha(retchar) && p) {
                char c = toupper(retchar);
                LBNode *n = findNodeWithChar(c,p);
                if (n) {
                    this->setCursor(findItem(n->caption));
                    act++;
                }
            }
        }
        
        switch(key) {
            case DIK_TAB: ret = 1; act++; break;
            case DIK_UP: 
                if (cur_sel>0)
                    cur_sel--;
                else
                if (y_start>0)
                    y_start--;
                act++; 
                break;
            case DIK_DOWN: 
                if (cur_sel+y_start<num_elements-1) {
                    if (cur_sel<ysize)
                        cur_sel++;
                    else
                        y_start++;
                }
                act++;
                break;
            case DIK_PGDN:
                if (cur_sel<ysize)
                    cur_sel = ysize;
                else
                    y_start+=ysize;
                act++;
                break;
            case DIK_PGUP:
                if (cur_sel>0)
                    cur_sel = 0;
                else
                    y_start -= ysize;
                act++;
                break;
            case DIK_HOME:
                if (cur_sel>0)
                    cur_sel = 0;
                else
                    y_start=0;
                act++;
                break;
            case DIK_END:
                if (cur_sel<ysize)
                    cur_sel = ysize;
                else
                    y_start = num_elements - ysize -1;
                act++;
                break;
            case DIK_RETURN: 
                OnSelect(getNode(cur_sel+y_start));
                act++; 
                break;
//            case DIK_HOME: cur_sel=0; act++; break;
//            case DIK_END: cur_sel = MidiOut->numMidiDevs-1; act++; break;
        }
        if (act) {
            Keys.getkey();
            need_refresh++;
            need_redraw++;
            if (cur_sel<0) cur_sel=0;
            if (cur_sel>ysize) cur_sel = ysize;
            if (y_start > num_elements - ysize -1 ) y_start = num_elements - ysize -1;
            if (y_start < 0) y_start = 0;
            if (cur_sel>y_start+num_elements) cur_sel = num_elements - y_start-1;
        }
    }
    if (cur_sel != old_cur_sel || y_start != old_y_start)
        OnSelectChange();
    return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
char *ListBox::getCurrentItem(void) {
    char * p = getItem(cur_sel + y_start);
    if (!p) p = "";
    return p;
}




// ------------------------------------------------------------------------------------------------
//
//
int ListBox::getCurrentItemIndex(void) {
    return cur_sel+y_start;
}




// ------------------------------------------------------------------------------------------------
//
//
void ListBox::OnSelect(LBNode *selected) {
    mousestate = 0;
}





// ------------------------------------------------------------------------------------------------
//
//
void ListBox::draw(Drawable *S, int active) {
    int cy;
    TColor f,b;
    unsigned char *str;
    str = (unsigned char *)malloc(xsize+1+2);
    LBNode *node = getNode(y_start);
    for (cy=0;cy<=ysize;cy++) {
        memset(str,0,xsize+1);
        memset(str,' ',xsize);
        if (node) {
            if (use_checks) {
                if (node->checked)
                    str[0] = check_on;
                else
                    str[0] = check_off;
                strc((char *)str+2, node->caption);
            } else {
                strc((char *)str, node->caption);
            }
            node = node->next;
        }
        if (cy == cur_sel) {
            if (active)
                b = COLORS.Highlight;
            else
                b = COLORS.EditText;
            f = *color_itemsel;
        } else {
            b = COLORS.EditBG;
            f = *color_itemnosel;
        }
        if (num_elements == 0 && cy==0) {
            f = COLORS.EditText;
            b = COLORS.EditBG;
            if (empty_message)
                strc((char*)str, empty_message);
        }
        printBGu(col(x),row(cy+y),str,f,b,S);
    }
    frm.type=0;
    frm.x=x; frm.y=y; frm.xsize=xsize; frm.ysize=ysize+1;
    frm.draw(S,0);
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+ysize+1));
    free(str);
}





// ------------------------------------------------------------------------------------------------
//
//
SkinSelector::SkinSelector() {
    empty_message = "No skins found";
    is_sorted = true;
    use_checks = true;
    OnChange();
}




// ------------------------------------------------------------------------------------------------
//
//
SkinSelector::~SkinSelector() {
}
bool file_exists(char *file) {
    FILE *fp;
    fp = fopen(file,"rb");
    if (!fp) return false;
    fclose(fp);
    return true;
}




// ------------------------------------------------------------------------------------------------
//
//
void SkinSelector::OnChange() {
    clear();
    long res,hnd;
    _finddata_t f;
    char s[512];
    strcpy(s,cur_dir);
    strcat(s,"/skins/*.*");
    hnd = res = _findfirst(s,&f);
    while(res != -1) {
        if (f.attrib & _A_SUBDIR) {
            LBNode *p;
            if (f.name[0] != '.'){
                char str[200];
                strcpy(str,cur_dir);
                strcat(str,"/skins/");
                strcat(str, f.name);
                strcat(str, "/colors.conf");
                if (file_exists(str)) {
                    p = insertItem(f.name);
                    if (zcmp(p->caption,zt_config_globals.skin))
                        p->checked = true;
                }
            }
        }
        res = _findnext(hnd, &f);
    }
    _findclose(hnd);
}





// ------------------------------------------------------------------------------------------------
//
//
void SkinSelector::OnSelect(LBNode *selected) 
{
//    if (selected->checked)
//        return;
    Skin *old = CurrentSkin;
    Skin *todel = CurrentSkin->switchskin(selected->caption);    
    if (old == todel) {
        strcpy(zt_config_globals.skin,CurrentSkin->strSkinName);
        color_itemsel = &COLORS.EditBG;
        color_itemnosel = &COLORS.EditText;
        selectNone();
        selected->checked = true;
        ListBox::OnSelect(selected);
    }
    delete todel;    
    this->mousestate = 0;
}
void SkinSelector::OnSelectChange() {
}





// ------------------------------------------------------------------------------------------------
//
//
MidiOutDeviceSelector::MidiOutDeviceSelector() {
    empty_message = "No MIDI-OUT devices open";
    is_sorted = true;
    use_checks = true;
    use_key_select = false; // so you can audition different devices
    OnChange();
}




// ------------------------------------------------------------------------------------------------
//
//
MidiOutDeviceSelector::~MidiOutDeviceSelector() {
}




// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceSelector::enter(void) {
    OnChange();
}





// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceSelector::OnChange() {
    clear();
    for (int i=0;i<MidiOut->GetNumOpenDevs();i++) {
        int dev = MidiOut->GetDevID(i);
        LBNode *p = insertItem((strlen(MidiOut->outputDevices[dev]->alias) > 1)?MidiOut->outputDevices[dev]->alias:MidiOut->outputDevices[dev]->szName);
        if (dev == song->instruments[cur_inst]->midi_device)
            p->checked = true;
        else
            p->checked = false;
        p->int_data = dev;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceSelector::OnSelect(LBNode *selected) {
    if (selected->checked) {
        song->instruments[cur_inst]->midi_device =  MAX_MIDI_DEVS;
        selected->checked = false;
    } else {
        song->instruments[cur_inst]->midi_device =  selected->int_data;
        this->selectNone();
        selected->checked = true;
    }
    ListBox::OnSelect(selected);
}



// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceSelector::OnSelectChange() {
}





// ------------------------------------------------------------------------------------------------
//
//
MidiOutDeviceOpener::MidiOutDeviceOpener() {
    empty_message = "No MIDI-OUT devices found";
    is_sorted = true;
    use_checks = true;
    OnChange();
}



// ------------------------------------------------------------------------------------------------
//
//
MidiOutDeviceOpener::~MidiOutDeviceOpener() {
}




// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceOpener::enter(void) {
    OnChange();
}




// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceOpener::OnChange() {
    clear();        
    for (unsigned int i=0;i<MidiOut->numOuputDevices;i++) {
//        LBNode *p = insertItem((MidiOut->outputDevices[i]->alias != NULL)?MidiOut->outputDevices[i]->alias:MidiOut->outputDevices[i]->szName);
        LBNode *p = insertItem(MidiOut->outputDevices[i]->szName);
        if (MidiOut->QueryDevice(i))
            p->checked = true;
        else
            p->checked = false;
        p->int_data = i;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceOpener::OnSelect(LBNode *selected) {
    midi_out_sel(selected->int_data);
    if (MidiOut->QueryDevice(selected->int_data))
        selected->checked = true;
    else
        selected->checked = false;
    statusmsg = szStatmsg;
    status_change = 1;
    ListBox::OnSelect(selected);
    Keys.flush();
}



// ------------------------------------------------------------------------------------------------
//
//
void MidiOutDeviceOpener::OnSelectChange() {
}



// ------------------------------------------------------------------------------------------------
//
//
int MidiOutDeviceOpener::mouseupdate(int cur_element) {
/*    if (cur_element == this->ID)
        bDontKeyRepeat = true;
    else
        bDontKeyRepeat = false;
*/
    return ListBox::mouseupdate(cur_element);
    //return cur_element;
}



// ------------------------------------------------------------------------------------------------
//
//
int MidiOutDeviceOpener::update() {
    LatencyValueSlider *l = (LatencyValueSlider *)lvs;
    l->sync();
    BankSelectCheckBox *b = (BankSelectCheckBox *)this->bscb;
    b->sync();
    AliasTextInput *t = (AliasTextInput *)al;
    t->sync();
    return ListBox::update();
    //return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
MidiInDeviceOpener::MidiInDeviceOpener() {
    empty_message = "No MIDI-IN devices found";
    is_sorted = true;
    use_checks = true;
    OnChange();
}



// ------------------------------------------------------------------------------------------------
//
//
MidiInDeviceOpener::~MidiInDeviceOpener() {
}



// ------------------------------------------------------------------------------------------------
//
//
int MidiInDeviceOpener::mouseupdate(int cur_element) {
/*
    if (cur_element == this->ID)
        bDontKeyRepeat = true;
    else
        bDontKeyRepeat = false;
*/
    return ListBox::mouseupdate(cur_element);
}




// ------------------------------------------------------------------------------------------------
//
//
void MidiInDeviceOpener::enter(void) {
    OnChange();
}




// ------------------------------------------------------------------------------------------------
//
//
void MidiInDeviceOpener::OnChange() {
    clear();        
    for (unsigned int i=0;i<MidiIn->numMidiDevs;i++) {
        LBNode *p = insertItem(MidiIn->midiInDev[i]->szName);
        if (MidiIn->QueryDevice(i))
            p->checked = true;
        else
            p->checked = false;
        p->int_data = i;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void MidiInDeviceOpener::OnSelect(LBNode *selected) {
    midi_in_sel(selected->int_data);
    if (MidiIn->QueryDevice(selected->int_data))
        selected->checked = true;
    else
        selected->checked = false;
    statusmsg = szStatmsg;
    status_change = 1;
    ListBox::OnSelect(selected);
    Keys.flush();
}



// ------------------------------------------------------------------------------------------------
//
//
void MidiInDeviceOpener::OnSelectChange() {
}






// ------------------------------------------------------------------------------------------------
//
//
LatencyValueSlider::LatencyValueSlider(MidiOutDeviceOpener *m) {    
    listbox = m;
    sel = -1;
    value=0;
    sync();
}



// ------------------------------------------------------------------------------------------------
//
//
LatencyValueSlider::~LatencyValueSlider() {
}



// ------------------------------------------------------------------------------------------------
//
//
void LatencyValueSlider::sync() {
    LBNode *p = listbox->getNode(listbox->getCurrentItemIndex() );
    if (p) {
        if (p->int_data != sel) {
            sel = p->int_data;
            value = MidiOut->get_delay_ticks(sel);
            need_redraw++;
            need_refresh++;
        } else {
            if (MidiOut->get_delay_ticks(sel) != this->value) {
                MidiOut->set_delay_ticks(sel, this->value);
                need_redraw++;
                need_refresh++;
            }
        }
    }
}



// ------------------------------------------------------------------------------------------------
//
//
int LatencyValueSlider::update() {
    sync(); 
    return ValueSlider::update();
}




// ------------------------------------------------------------------------------------------------
//
//
BankSelectCheckBox::BankSelectCheckBox(MidiOutDeviceOpener *m) {
    this->value = &i_value;
    listbox = m;
    sel = -1;
    i_value = 0;
    sync();
}




// ------------------------------------------------------------------------------------------------
//
//
BankSelectCheckBox::~BankSelectCheckBox() {
}



// ------------------------------------------------------------------------------------------------
//
//
void BankSelectCheckBox::sync(void) {
    LBNode *p = listbox->getNode(listbox->getCurrentItemIndex() );
    if (p) {
        if (p->int_data != sel) {
            sel = p->int_data;
            i_value = MidiOut->get_bank_select(sel);
            need_redraw++;
            need_refresh++;
        } else {
            if (MidiOut->get_bank_select(sel) != this->i_value) {
                MidiOut->set_bank_select(sel, this->i_value);
                need_redraw++;
                need_refresh++;
            }
        }
    }    
}



// ------------------------------------------------------------------------------------------------
//
//
int BankSelectCheckBox::update() {
    sync(); 
    return CheckBox::update();
}




// ------------------------------------------------------------------------------------------------
//
//
AliasTextInput::AliasTextInput(MidiOutDeviceOpener *m) {

	for(int i = 0; i < 1024; i++)
		alias[i] = '\0';
    this->str = (alias != NULL)?(unsigned char*)alias:(unsigned char*)"";
    listbox = m;
    cursor = 0;
    length = 1023 ;//(alias != NULL)?strlen(alias):0;
    sel = -1;
    sync();
}




// ------------------------------------------------------------------------------------------------
//
//
AliasTextInput::~AliasTextInput() {
}




// ------------------------------------------------------------------------------------------------
//
//
void AliasTextInput::sync(void) {
    
    LBNode *p = listbox->getNode(listbox->getCurrentItemIndex() );
    if (p) {
        if (p->int_data != sel) {
            sel = p->int_data;
            strcpy(alias,MidiOut->get_alias(sel));
            this->cursor = strlen(this->alias);
            need_redraw++;
            need_refresh++;
        } else {
            if (strcmp(MidiOut->get_alias(sel),this->alias)) {
                MidiOut->set_alias(sel, this->alias);
                this->cursor = strlen(this->alias);
                need_redraw++;
                need_refresh++;
            }
        }
    }
}




// ------------------------------------------------------------------------------------------------
//
//
int AliasTextInput::update() {
    sync(); 
    return TextInput::update();
}




// ------------------------------------------------------------------------------------------------
//
//
CommentEditor::CommentEditor() {
    bUseColors = FALSE;
    bWordWrap = true;
}




// ------------------------------------------------------------------------------------------------
//
//
CommentEditor::~CommentEditor() {

}




// ------------------------------------------------------------------------------------------------
//
//
int CommentEditor::update() {
    KBKey key,act=0;
    int ret=0;
    key = Keys.checkkey();
    unsigned char ch = Keys.getactualchar();
    if (key) {
        switch(key) {
        //case SDLK_m
        case DIK_UP  : act++; startline--;  break;
        case DIK_DOWN: act++; if (!bEof) startline++;  break;
        case DIK_PGUP: act++; startline-=16;  break;
        case DIK_PGDN: act++; if (!bEof) startline+=16;  break;
        case DIK_LEFT: act++; soff--; break;
        case DIK_RIGHT:act++; soff++; break;
        case DIK_HOME: if (soff>0) soff=0; else startline=0; act++; break;

        default:
            if (ch != 0x0) {
                if (target) {
                    target->pushc(ch);
                    act++;
                }
            }
            //        case DIK_SPACE:  //  / Same thing
//        case DIK_RETURN: //  \ Same thingg
//            break;
        }
        if (act) {
            Keys.getkey();
                need_refresh++;
            need_redraw++;

        }
    }
    if (startline<0) startline = 0;
    if (soff<0) soff=0;
    return ret;
}



/* eof */
