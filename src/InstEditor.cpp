#include "InstEditor.h"


#define INSTRUMENT_USED_MARK    "+"



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
                        int i = (MousePressY/8) - this->y;
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
            case DIK_MOUSE_1_ON:
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
            case DIK_MOUSE_1_OFF:
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
            case DIK_MOUSE_1_ON:
                if (checkclick(col(this->x),row(this->y),col(this->x+this->xsize),row(this->y+this->ysize))) {
                    int i = (MousePressY/8) - this->y;// + list_start;
                    int j = (MousePressX/8) - this->x;
                    this->text_cursor = j;
                    this->cursor = i;
                    act++;
                    mousestate = 1;
                    
                }
                break;
            case DIK_MOUSE_1_OFF:
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

  char *str = (char *)&song->instruments[list_start+cursor]->title[0];

  key = Keys.checkkey();
  kstate = Keys.getstate();

  if (key) {

    if (!(kstate&KS_CTRL) && !(kstate&KS_ALT)) {

      switch(key)
      {
        case DIK_HOME:

          if (text_cursor>0) text_cursor = 0;
          else {

            if (cursor == 0) list_start = 0;
            else cursor = 0;
          }

          act++;

          break;

        case DIK_END:

          if (text_cursor<length-1) text_cursor = length-1;
          else {

            if (cursor == ysize-1) list_start = MAX_INSTS-ysize;
            else cursor = ysize-1;
          }

          act++;

          break;

        case DIK_PGUP:

          if (cursor == 0) list_start-=16;
          else cursor-=16;
          
          act++;

          break;

        case DIK_PGDN:

          if (cursor == ysize-1) list_start+=16;
          else cursor+=16;
          
          act++;

          break;

        case DIK_DOWN: 

          cursor++;

          if (cursor>=ysize) list_start++;
          
          act++;

          break;

        case DIK_UP:

          cursor--;
          if (cursor<0) list_start--;

          act++;

          break;

        case DIK_LEFT: 
          
          text_cursor--;
          act++; 
          
          break;
        
        case DIK_RIGHT:
          
          text_cursor++;
          act++; 
          
          break;
      } ;

      if (text_cursor == length-1) {

        switch(key)
        {
          case DIK_TAB: 

            if (kstate == KS_SHIFT) ret = -1;
            else ret = 1;

            act++;

            break;
        }
      }
      else {

        switch(key)
        {
          case DIK_TAB:           text_cursor = length-1; act++;       break;

          case DIK_A:             retletter=1; act++;                  break;
          case DIK_B:             retletter=2; act++;                  break;
          case DIK_C:             retletter=3; act++;                  break;
          case DIK_D:             retletter=4; act++;                  break;
          case DIK_E:             retletter=5; act++;                  break;
          case DIK_F:             retletter=6; act++;                  break;
          case DIK_G:             retletter=7; act++;                  break;
          case DIK_H:             retletter=8; act++;                  break;
          case DIK_I:             retletter=9; act++;                  break;
          case DIK_J:             retletter=10; act++;                 break;
          case DIK_K:             retletter=11; act++;                 break;
          case DIK_L:             retletter=12; act++;                 break;
          case DIK_M:             retletter=13; act++;                 break;
          case DIK_N:             retletter=14; act++;                 break;
          case DIK_O:             retletter=15; act++;                 break;
          case DIK_P:             retletter=16; act++;                 break;
          case DIK_Q:             retletter=17; act++;                 break;
          case DIK_R:             retletter=18; act++;                 break;
          case DIK_S:             retletter=19; act++;                 break;
          case DIK_T:             retletter=20; act++;                 break;
          case DIK_U:             retletter=21; act++;                 break;
          case DIK_V:             retletter=22; act++;                 break;
          case DIK_W:             retletter=23; act++;                 break;
          case DIK_X:             retletter=24; act++;                 break;
          case DIK_Y:             retletter=25; act++;                 break;
          case DIK_Z:             retletter=26; act++;                 break;
          case DIK_0:             retletter=0xFF; act++;               break;
          case DIK_1:             retletter=0xFF; act++;               break;
          case DIK_2:             retletter=0xFF; act++;               break;
          case DIK_3:             retletter=0xFF; act++;               break;
          case DIK_4:             retletter=0xFF; act++;               break;
          case DIK_5:             retletter=0xFF; act++;               break;
          case DIK_6:             retletter=0xFF; act++;               break;
          case DIK_7:             retletter=0xFF; act++;               break;
          case DIK_8:             retletter=0xFF; act++;               break;
          case DIK_9:             retletter=0xFF; act++;               break;

          case DIK_SEMICOLON:     retletter=0xff; act++;               break;
          case DIK_SPACE:         retletter=0xff; act++;               break;
          case DIK_PERIOD:        retletter=0xff; act++;               break;
          case DIK_COMMA:         retletter=0xff; act++;               break;
          case DIK_BACKSLASH:     retletter=0xff; act++;               break;
          case DIK_MINUS:         retletter=0xff; act++;               break;
          case DIK_EQUALS:        retletter=0xff; act++;               break;
          case DIK_BACKSPACE:     retletter=0xFFF; act++;              break;
          case DIK_DELETE:        retletter=0xFFF; act++;              break;
        }
      }           
    }
    else {

      switch(key)
      {
      case DIK_C:

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

              switch(key)
              {
              case DIK_SEMICOLON: 

                if (kstate & KS_SHIFT) str[text_cursor] = ':';
                else str[text_cursor] = ';'; 

                break;

              case DIK_SPACE:  str[text_cursor] = ' ';             break;
              case DIK_PERIOD: str[text_cursor] = '.';             break;
              case DIK_COMMA:  str[text_cursor] = ',';             break;
              case DIK_BACKSLASH:  str[text_cursor] = '\\';        break;
              case DIK_MINUS:  str[text_cursor] = '-';             break;
              case DIK_EQUALS: str[text_cursor] = '=';             break;
              case DIK_0: str[text_cursor] = '0';                  break;
              case DIK_1: str[text_cursor] = '1';                  break;
              case DIK_2: str[text_cursor] = '2';                  break;
              case DIK_3: str[text_cursor] = '3';                  break;
              case DIK_4: str[text_cursor] = '4';                  break;
              case DIK_5: str[text_cursor] = '5';                  break;
              case DIK_6: str[text_cursor] = '6';                  break;
              case DIK_7: str[text_cursor] = '7';                  break;
              case DIK_8: str[text_cursor] = '8';                  break;
              case DIK_9: str[text_cursor] = '9';                  break;
              }
            }

            text_cursor++;
          }
        } 
        else { // Is backspace/del

          if (key == DIK_BACKSPACE) text_cursor--;

          if (key != DIK_BACKSPACE || (text_cursor>=0)) {  /* FIX THIS SHIT! */

            if (key == DIK_BACKSPACE && text_cursor == length-1) {

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

    if(song->instruments[inst_num]->IsUsed()) printBG(col(x-4), row(cy+y), INSTRUMENT_USED_MARK, COLORS.Text, COLORS.Background, S);

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






