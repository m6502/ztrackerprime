#include "InstEditor.h"
#include "zt.h"

#define INSTRUMENT_USED_MARK_CHAR    "+"



// ------------------------------------------------------------------------------------------------
//
//
InstEditor::InstEditor(void) {
    list_start = text_cursor = cursor = 0;
    frm = new Frame;
    frm->type = 0;
    last_cur_row = -1 ;

}



// ------------------------------------------------------------------------------------------------
//
//
InstEditor::~InstEditor(void) {
    delete frm;
}



// ------------------------------------------------------------------------------------------------
//
//
int InstEditor::mouseupdate(int cur_element) {
    KBKey key,act=0;
    key = Keys.checkkey();

    
    
    
    //    int old_cur_sel = cur_sel;
//   int old_y_start = y_start;
/*
    if (!bMouseIsDown)
        mousestate = 0;
    if (mousestate) {
        int i = (LastY/8) - this->y ;
        if (cur_sel+y_start != i) {
            if (i<0) {
                if (y_start >0 ) {
                    setCursor(y_start-1);
//                    y_start--;
                    act++;
                }
            } else
            if (i>ysize && cur_sel==ysize && y_start<num_elements-ysize-1) {
//                y_start++;
                setCursor(y_start+cur_sel+1);
                act++;
            } else
            if (i<=ysize && i+y_start<num_elements && i+y_start>=0) {
                setCursor(i+y_start);
                act++;
            }
        }
    }
    if (key) {
        switch(key) {
            case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT)):
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize+1))) {
                    int i = (MousePressY/8) - this->y;
                    if (cur_element == this->ID && i==this->cur_sel) {
                        LBNode *p = this->getNode(this->cur_sel+this->y_start);
                        if (p)
                            OnSelect( p );
                    }
                    mousestate = 1;
                    if (i<=ysize && i<num_elements)
                        setCursor(i+y_start);                    
                    act++;
                }
                break;
            case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT)):
                if (mousestate) {
                    act++;
                }
                mousestate=0;
                break;
        }
    }
    if (cur_sel != old_cur_sel || y_start != old_y_start)
        OnSelectChange();
    if (act)  key = Keys.getkey();
    if (act) {
        need_refresh++;
        need_redraw++;
        return this->ID;
    }

    return cur_element;
*/
    if (mousestate) {
        int i = (LastY/8)-this->y;
        if (cursor != i) {
            act++;
            cursor = i;
        }
        int j = (LastX/8) - this->x;
        if (text_cursor != j) {
            this->text_cursor = j;
            act++;
        }
    }
    if (key) {
        switch(key) {
            case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT)):
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
                    int i = (MousePressY/8) - this->y;// + list_start;
                    int j = (MousePressX/8) - this->x;
                    this->text_cursor = j;
                    this->cursor = i;
                    act++;
                    mousestate = 1;
                    
                }
                break;
            case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT)):
                if (mousestate) {
                    act++;
                }
                mousestate=0;
                break;

        }
    }
    if (act) {
        key = Keys.getkey();
        need_refresh++;
        need_redraw++;
        if (cursor>ysize-1)
            cursor = ysize-1;
        if (cursor<0)
            cursor=0;
        if (text_cursor<0)
            text_cursor = 0;
        if (text_cursor>24) text_cursor=24;     

        return this->ID;
    }
    return cur_element;
}




// ------------------------------------------------------------------------------------------------
//
//
int InstEditor::update()
{
  KBKey key,act=0,kstate;
  int ret=0,length=25;
  signed int retletter = 0,i;
  unsigned char actual = 0;
  unsigned char use_actual = 0;

  char *str = (char *)&song->instruments[list_start+cursor]->title[0];

  key = Keys.checkkey();
  kstate = Keys.getstate();
  actual = Keys.getactualchar();

  if (key) {

    if (!(kstate&KS_CTRL) && !(kstate&KS_ALT)) {

      switch(key)
      {
        case SDLK_HOME:

          if (text_cursor>0) text_cursor = 0;
          else {

            if (cursor == 0) list_start = 0;
            else cursor = 0;
          }

          act++;

          break;

        case SDLK_END:

          if (text_cursor<length-1) text_cursor = length-1;
          else {

            if (cursor == ysize-1) list_start = MAX_INSTS-ysize;
            else cursor = ysize-1;
          }

          act++;

          break;

        case SDLK_PAGEUP:

          if (cursor == 0) list_start-=16;
          else cursor-=16;
          
          act++;

          break;

        case SDLK_PAGEDOWN:

          if (cursor == ysize-1) list_start+=16;
          else cursor+=16;
          
          act++;

          break;

        case SDLK_DOWN: 

          cursor++;

          if (cursor>=ysize) list_start++;
          
          act++;

          break;

        case SDLK_UP:

          cursor--;
          if (cursor<0) list_start--;

          act++;

          break;

        case SDLK_LEFT: 
          
          text_cursor--;
          act++; 
          
          break;
        
        case SDLK_RIGHT:
          
          text_cursor++;
          act++; 
          
          break;
      } ;

      if (text_cursor == length-1) {

        switch(key)
        {
          case SDLK_TAB:
            if (kstate == KS_SHIFT) {
              text_cursor = 0;
              act++;
            } else {
              ret = 1;
              act++;
            }
            break;
        }
      }
      else {
        const bool is_nav_or_edit =
            (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT || key == SDLK_RIGHT ||
             key == SDLK_BACKSPACE || key == SDLK_DELETE || key == SDLK_HOME || key == SDLK_END);

        if (!is_nav_or_edit && actual >= 0x20 && actual != 0x7f) {
          if (actual == '`') {
            // Skip raw backtick; allow '~' via actual when shifted.
          } else {
            retletter = 0xff;
            use_actual = 1;
            act++;
          }
        }

        if (!use_actual) switch(key)
        {
          case SDLK_TAB:
            if (kstate == KS_SHIFT) {
              if (text_cursor > 0) {
                text_cursor = 0;
                act++;
              } else {
                ret = -1;
                act++;
              }
            } else {
              text_cursor = length-1;
              act++;
            }
            break;

          case SDLK_A:             retletter=1; act++;                  break;
          case SDLK_B:             retletter=2; act++;                  break;
          case SDLK_C:             retletter=3; act++;                  break;
          case SDLK_D:             retletter=4; act++;                  break;
          case SDLK_E:             retletter=5; act++;                  break;
          case SDLK_F:             retletter=6; act++;                  break;
          case SDLK_G:             retletter=7; act++;                  break;
          case SDLK_H:             retletter=8; act++;                  break;
          case SDLK_I:             retletter=9; act++;                  break;
          case SDLK_J:             retletter=10; act++;                 break;
          case SDLK_K:             retletter=11; act++;                 break;
          case SDLK_L:             retletter=12; act++;                 break;
          case SDLK_M:             retletter=13; act++;                 break;
          case SDLK_N:             retletter=14; act++;                 break;
          case SDLK_O:             retletter=15; act++;                 break;
          case SDLK_P:             retletter=16; act++;                 break;
          case SDLK_Q:             retletter=17; act++;                 break;
          case SDLK_R:             retletter=18; act++;                 break;
          case SDLK_S:             retletter=19; act++;                 break;
          case SDLK_T:             retletter=20; act++;                 break;
          case SDLK_U:             retletter=21; act++;                 break;
          case SDLK_V:             retletter=22; act++;                 break;
          case SDLK_W:             retletter=23; act++;                 break;
          case SDLK_X:             retletter=24; act++;                 break;
          case SDLK_Y:             retletter=25; act++;                 break;
          case SDLK_Z:             retletter=26; act++;                 break;
          case SDLK_0:             retletter=0xFF; act++;               break;
          case SDLK_1:             retletter=0xFF; act++;               break;
          case SDLK_2:             retletter=0xFF; act++;               break;
          case SDLK_3:             retletter=0xFF; act++;               break;
          case SDLK_4:             retletter=0xFF; act++;               break;
          case SDLK_5:             retletter=0xFF; act++;               break;
          case SDLK_6:             retletter=0xFF; act++;               break;
          case SDLK_7:             retletter=0xFF; act++;               break;
          case SDLK_8:             retletter=0xFF; act++;               break;
          case SDLK_9:             retletter=0xFF; act++;               break;

          case SDLK_SEMICOLON:     retletter=0xff; act++;               break;
          case SDLK_SPACE:         retletter=0xff; act++;               break;
          case SDLK_PERIOD:        retletter=0xff; act++;               break;
          case SDLK_COMMA:         retletter=0xff; act++;               break;
          case SDLK_BACKSLASH:     retletter=0xff; act++;               break;
          case SDLK_SLASH:         retletter=0xff; act++;               break;
          case SDLK_KP_DIVIDE:     retletter=0xff; act++;               break;
          case SDLK_KP_MULTIPLY:   retletter=0xff; act++;               break;
          case SDLK_MINUS:         retletter=0xff; act++;               break;
          case SDLK_EQUALS:        retletter=0xff; act++;               break;
          case SDLK_BACKSPACE:     retletter=0xFFF; act++;              break;
          case SDLK_DELETE:        retletter=0xFFF; act++;              break;
        }
      }           
    }
    else {

      switch(key)
      {
      case SDLK_C:

        if (kstate & KS_ALT) memset(&song->instruments[list_start+cursor]->title,' ',24);
        act++;

        break;
      }
    }


    if (act) {

      Keys.getkey();
      need_refresh++;
      need_redraw++;

      length=24;

      if (retletter) {

        if (retletter != 0xFFF) { // Regular Key

          for(i=length-1;i>text_cursor;i--) {

            str[i] = str[i-1];
          }

          if (text_cursor<length) {

            if (retletter != 0xff) {

              if (kstate & KS_SHIFT) str[text_cursor] = ((int)retletter + 'A' - 1);
              else str[text_cursor] = ((int)retletter + 'a' - 1);
            }
            else {
              if (use_actual) {
                str[text_cursor] = actual;
              } else {

              switch(key)
              {
              case SDLK_SEMICOLON: 

                if (kstate & KS_SHIFT) str[text_cursor] = ':';
                else str[text_cursor] = ';'; 

                break;

              case SDLK_SPACE:  str[text_cursor] = ' ';             break;
              case SDLK_PERIOD: str[text_cursor] = '.';             break;
              case SDLK_COMMA:
                if (kstate & KS_SHIFT) str[text_cursor] = '<';
                else str[text_cursor] = ',';
                break;
              case SDLK_BACKSLASH:  str[text_cursor] = '\\';        break;
              case SDLK_SLASH:
                if (kstate & KS_SHIFT) str[text_cursor] = '?';
                else str[text_cursor] = '/';
                break;
              case SDLK_KP_DIVIDE: str[text_cursor] = '/';          break;
              case SDLK_KP_MULTIPLY: str[text_cursor] = '*';        break;
              case SDLK_MINUS:
                if (kstate & KS_SHIFT) str[text_cursor] = '_';
                else str[text_cursor] = '-';
                break;
              case SDLK_EQUALS:
                if (kstate & KS_SHIFT) str[text_cursor] = '+';
                else str[text_cursor] = '=';
                break;
              case SDLK_0: str[text_cursor] = '0';                  break;
              case SDLK_1: str[text_cursor] = (kstate & KS_SHIFT) ? '!' : '1'; break;
              case SDLK_2: str[text_cursor] = (kstate & KS_SHIFT) ? '@' : '2'; break;
              case SDLK_3: str[text_cursor] = (kstate & KS_SHIFT) ? '#' : '3'; break;
              case SDLK_4: str[text_cursor] = (kstate & KS_SHIFT) ? '$' : '4'; break;
              case SDLK_5: str[text_cursor] = (kstate & KS_SHIFT) ? '%' : '5'; break;
              case SDLK_6: str[text_cursor] = (kstate & KS_SHIFT) ? '^' : '6'; break;
              case SDLK_7: str[text_cursor] = (kstate & KS_SHIFT) ? '&' : '7'; break;
              case SDLK_8: str[text_cursor] = (kstate & KS_SHIFT) ? '*' : '8'; break;
              case SDLK_9: str[text_cursor] = (kstate & KS_SHIFT) ? '(' : '9'; break;
              }
              }
            }

            text_cursor++;
          }
        } 
        else { // Is backspace/del

          if (key == SDLK_BACKSPACE) text_cursor--;

          if (key != SDLK_BACKSPACE || (text_cursor>=0)) {  /* FIX THIS SHIT! */

            if (key == SDLK_BACKSPACE && text_cursor == length-1) {

              str[length-2] = str[length-1];
              str[length-1] = ' ';
            } 
            else {

              str[length-1] = ' ';

              for(i=text_cursor;i<length-1;i++) {

                str[i] = str[i+1];
              }
            }
          }
        } // End RetLetter
      }
    }

    length=25;

    if (cursor<0) cursor=0;
    if (cursor>=ysize) cursor=ysize-1;
    if (list_start<0) list_start = 0;
    if (list_start>=MAX_INSTS-ysize) list_start=MAX_INSTS-ysize;

    if (text_cursor<0) text_cursor=0;
    if (text_cursor>length-1) text_cursor=length-1;     

  }


  return ret;
}



// ------------------------------------------------------------------------------------------------
//
//
void InstEditor::strc(char *dst, char *src) {
    int i=0;
    while (i<xsize && src[i]) {
        dst[i] = src[i];
        i++;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void InstEditor::draw(Drawable *S, int active)
{
  int cy;
  TColor f,b;
  char str[26],str2[5];

  //    memset(str,' ',26); str[25]=0;

  for (cy=0;cy<ysize;cy++) {

    int inst_num = list_start+cy ;

    memset(str,' ',26); str[25]=0;
    strc(str,(char *)&song->instruments[inst_num]->title[0]);
    printBG(col(x),row(cy+y),str,COLORS.EditText,COLORS.EditBG,S);

    //<Manu>

    if(song->instruments[inst_num]->IsUsed()) printBG(col(x-4), row(cy+y), INSTRUMENT_USED_MARK_CHAR, COLORS.Text, COLORS.Background, S);

    sprintf(str2,"%.2d", inst_num);
    printBG(col(x-3),row(cy+y),str2,COLORS.Text,COLORS.Background,S);



    if (active) {

      f = COLORS.EditBG;  
      b = COLORS.Highlight;

    }
    else {

      f = COLORS.EditBG;  
      b = COLORS.EditText;
    }

    if (cy == cursor) {

      if (text_cursor<24) {

        printcharBG(col(x + text_cursor),row(cy+y),song->instruments[inst_num]->title[text_cursor],f,b,S);
        printBG(col(x+25),row(cy+y),"Play",COLORS.EditText,COLORS.EditBG,S);
      }
      else printBG(col(x+25),row(cy+y),"Play",f,b,S);
    } 
    else printBG(col(x+25),row(cy+y),"Play",COLORS.EditText,COLORS.EditBG,S);
    
    printcharBG(col(x + 24),row(cy+y),168,COLORS.Lowlight,COLORS.EditBG,S);
  }

  frm->type=0;
  frm->x=x; frm->y=y; frm->xsize=xsize; frm->ysize=ysize;
  frm->draw(S,0);
  
  screenmanager.Update(col(x-4),row(y-1),col(x+xsize+1),row(y+ysize+1));
}






