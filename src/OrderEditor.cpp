#include "OrderEditor.h"

// ------------------------------------------------------------------------------------------------
//
//
OrderEditor::OrderEditor(void) {
    frm = new Frame;
    cursor_y = cursor_x = list_start = 0;
    old_playing_ord = -1;
}



// ------------------------------------------------------------------------------------------------
//
//
OrderEditor::~OrderEditor(void) {
    delete frm;
}



// ------------------------------------------------------------------------------------------------
//
//
int OrderEditor::mouseupdate(int cur_element) {
    if (ztPlayer->playing) {
        if (ztPlayer->playing_cur_order != old_playing_ord) {
            need_redraw++;
            need_refresh++;
            old_playing_ord = ztPlayer->playing_cur_order;
        }
    } 
    else if (old_playing_ord != -1 && !ztPlayer->playing) {
        old_playing_ord = -1;
        need_redraw++;
        need_refresh++;
    }
    return cur_element;
}



// ------------------------------------------------------------------------------------------------
//
//
int OrderEditor::update() {
    KBKey key,act=0;
    int kstate;
    int ret=0,i,val=0xFF;
    short int p1,p2,p3,p;
    key = Keys.checkkey();
    kstate = Keys.getstate();

    if (key) {
        switch(key) {
            case DIK_TAB: ret = 1; act++; break;
            case DIK_HOME:
                if (cursor_y>0)
                    cursor_y=0;
                else
                    list_start=0;
                act++;
                break;
            case DIK_END:
                if (cursor_y<ysize-1)
                    cursor_y=ysize-1;
                else
                    list_start=256-ysize;
                act++;
                break;
            case DIK_PGUP:
                if (cursor_y>0)
                    cursor_y-=16;
                else
                    list_start-=16;
                act++;
                break;
            case DIK_PGDN:
                if (cursor_y<ysize-2)
                    cursor_y+=16;
                else
                    list_start+=16;
                act++;
                break;
            case DIK_UP: 
                cursor_y--; 
                if (cursor_y<0)
                    list_start--;
                act++; 
                break;
            case DIK_DOWN: 
                cursor_y++; 
                if (cursor_y>=ysize)
                    list_start++;
                act++; 
                break;
            case DIK_LEFT: cursor_x--; act++; break;
            case DIK_RIGHT: cursor_x++; act++; break;
            
            case DIK_PERIOD:
            case DIK_SPACE:
            case DIK_MINUS:
                song->orderlist[cursor_y+list_start] = 0x100;
                cursor_x=0; cursor_y++;
                if (cursor_y>=ysize)
                    list_start++;
                act++;
                break;
            case DIK_ADD:
                if(kstate == KS_SHIFT)
                {
                    if(cur_edit_order < 255 && song->orderlist[cur_edit_order + 1] != 0x100)
                    {
                        cur_edit_order++;
                        cur_edit_pattern = song->orderlist[cur_edit_order];
                            need_refresh++;
                        need_redraw++;
                    }
                    break;
                }
                song->orderlist[cursor_y+list_start] = 0x101;
                cursor_x=0; cursor_y++;
                if (cursor_y>=ysize)
                    list_start++;
                act++;
                break;
            case DIK_SUBTRACT:
                if(kstate == KS_SHIFT)
                {
                    if(cur_edit_order > 0 && song->orderlist[cur_edit_order -1] != 0x100)
                    {
                        cur_edit_order--;
                        cur_edit_pattern = song->orderlist[cur_edit_order];
                            need_refresh++;
                        need_redraw++;
                    }
                    break;
                }
                break;
            case DIK_DELETE:
                for(i=cursor_y+list_start;i<255;i++)
                    song->orderlist[i] = song->orderlist[i+1];
                song->orderlist[255] = 0x100;
                if(song->orderlist[cur_edit_order -1 ] == 0x100 || song->orderlist[cur_edit_order + 1] == 0x100)
                    for(;song->orderlist[cur_edit_order] == 0x100 && cur_edit_order >=0;cur_edit_order--);
                act++;
                if(cur_edit_order<0)
                    cur_edit_order=0;
                break;
            case DIK_INSERT:
                for(i=255;i>cursor_y+list_start;i--)
                    song->orderlist[i] = song->orderlist[i-1];
                song->orderlist[cursor_y+list_start] = 0x100;
                act++;
                break;
            case DIK_0: val = 0; act++; break;
            case DIK_1: val = 1; act++; break;
            case DIK_2: val = 2; act++; break;
            case DIK_3: val = 3; act++; break;
            case DIK_4: val = 4; act++; break;
            case DIK_5: val = 5; act++; break;
            case DIK_6: val = 6; act++; break;
            case DIK_7: val = 7; act++; break;
            case DIK_8: val = 8; act++; break;
            case DIK_9: val = 9; act++; break;

            case DIK_G: 
                p = song->orderlist[ cursor_y + list_start ];
                if (p < 0x100) {
                    cur_edit_pattern = p;
                    switch_page( UIP_Patterneditor );
                    act++;
                    doredraw++;
                    Keys.flush();
                    midiInQueue.clear();
                }
                break;
        }
        if (act) {
            if (val<0xFF) {
                file_changed++;
                p = song->orderlist[cursor_y+list_start];
                if (p>0xFF) p = 0;
                p1 = p / 100;   // p1
                p2 = (p-(p1*100)) / 10;
                p3 = p % 10;    // p3 = (54 % 10)  (=4)
                switch(cursor_x) {
                    case 2: p3=val; break;
                    case 1: p2=val; break;
                    case 0: p1=val; break;
                }
                p = p3 + (p2*10) + (p1*100);
                if (p<0) p=0;
                if (p>0xFF) p=0xFF;
                song->orderlist[cursor_y+list_start] = p;
                if (!zt_config_globals.step_editing) {
                    cursor_x++;
                    if (cursor_x>2) {
                        cursor_y++;
                        if (cursor_y>=ysize)
                            list_start++;
                        cursor_x=0;
                    }                        
                }
                else
                    cursor_y++;
            }
            if (cursor_x>2) {
                cursor_x = 2;
                if (cursor_y>=ysize)
                    list_start++;
            }
            if (cursor_y<0) cursor_y = 0;
            if (cursor_y>=ysize) cursor_y = ysize-1;
            if (list_start<0) list_start=0;
            if (list_start>256 - ysize) list_start = 256-ysize;
            if (cursor_x<0) cursor_x=0;
            Keys.getkey();
                need_refresh++;
            need_redraw++;

        }
    }
    sel_pat = song->orderlist[cursor_y+list_start];
    sel_order = cursor_y+list_start;
    return ret;
}



// ------------------------------------------------------------------------------------------------
//
//
void OrderEditor::draw(Drawable *S, int active) {
    int cy;
    char str[8];
    TColor f;
    for (cy=0;cy<ysize;cy++) {
        if (cy+list_start == ztPlayer->cur_order && ztPlayer->playing)
            f = COLORS.Highlight;
        else if(cy+list_start == cur_edit_order)
            f= COLORS.Lowlight;
        else
            f = COLORS.Text;
        sprintf(&str[0],"%.3d",cy+list_start);
        printBG(col(x),row(cy+y),str,f,COLORS.Background,S);
        if (song->orderlist[cy+list_start] < 255) {
            sprintf(&str[0],"%.3d",song->orderlist[cy+list_start]);
        } else {
            if (song->orderlist[cy+list_start] == 0x101 )
                sprintf(&str[0],"+++");
            else    
                sprintf(&str[0],"---");
        }
        printBG(col(x+4),row(cy+y),str,COLORS.EditText,COLORS.EditBG,S);
        if (active) 
            f = COLORS.Highlight;
        else
            f = COLORS.EditText;
        if (cy == cursor_y)
            printcharBG(col(x+4+cursor_x),row(cy+y),str[cursor_x],COLORS.EditBG,f,S);
    }
    frm->type=0;
    frm->x=x+4; frm->y=y; frm->xsize=3; frm->ysize=ysize;
    frm->draw(S,0);
    screenmanager.Update(col(x-1),row(y-1),col(x+xsize+1),row(y+ysize+1));
}


