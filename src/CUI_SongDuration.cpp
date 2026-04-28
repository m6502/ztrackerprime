#include "zt.h"
#include "Button.h"


// ------------------------------------------------------------------------------------------------
//
//
void BTNCLK_close_songduration(void) {
    Keys.flush();
    close_popup_window();
    fixmouse++;
    need_refresh++;
}


// ------------------------------------------------------------------------------------------------
//
//
// Returns whole-seconds duration. For sub-second precision use
// calcSongMs() — same row-walk, but expressed in milliseconds so the
// caller can render MM:SS.t without the integer-truncation chunkiness.
int calcSongMs(int cur_row, int cur_ord)
{
    int o = 0;
    int64_t rows = 0;
    int64_t cr = 0;

    while (o < ZTM_ORDERLIST_LEN && song->orderlist[o] == 0x101) {
        o++;
    }

    while (o < ZTM_ORDERLIST_LEN && song->orderlist[o] < 0x100) {
        int len = song->patterns[song->orderlist[o]]->length;
        rows += len;
        if (cur_row != -1) {
            if (o < cur_ord) cr += len;
            if (o == cur_ord) cr += cur_row;
        }
        o++;
        while (o < ZTM_ORDERLIST_LEN && song->orderlist[o] == 0x101)
            o++;
    }

    if (song->tpb <= 0 || song->bpm <= 0) return 0;

    // Time per row = 60/(tpb*bpm) seconds → milliseconds = row*60000/(tpb*bpm).
    int64_t denom = (int64_t)song->tpb * (int64_t)song->bpm;
    int64_t target_rows = (cur_row > -1) ? cr : rows;
    return (int)((target_rows * 60000) / denom);
}

int calcSongSeconds(int cur_row, int cur_ord)
{
    return calcSongMs(cur_row, cur_ord) / 1000;
}


// ------------------------------------------------------------------------------------------------
//
//
CUI_SongDuration::CUI_SongDuration(void) {
    Button *b;
    int tabindex=0;
    UI = new UserInterface;

    this->x = 30 + ((INTERNAL_RESOLUTION_X-640)/16);
    this->y = 20 + ((INTERNAL_RESOLUTION_Y-480)/16);
    this->xsize = 20;
    this->ysize = 7;

    b = new Button;
    UI->add_element(b,tabindex++);
    b->xsize = 6;
    b->ysize = 1;
    b->x = this->x + (this->xsize/2)-(b->xsize/2);
    b->y = this->y + 5;
    b->caption = "  Ok";
    b->OnClick = (ActFunc)BTNCLK_close_songduration;
/*
    b = new Button;
    UI->add_element(b,tabindex++);
    b->x = 32+7+2;
    b->y = 23;
    b->xsize = 7;
    b->ysize = 1;
    b->caption = "  No";
    b->OnClick = (ActFunc)BTNCLK_newsong_no;
*/
}

// ------------------------------------------------------------------------------------------------
//
//
void CUI_SongDuration::enter(void) {
    need_refresh = 1;
    UI->cur_element = 1;
    seconds = calcSongSeconds();
}


// 16 rows @ 8 rows/beat = 16/8 = 2 beats
// 138 beats/min    (2*60)/138

// ------------------------------------------------------------------------------------------------
//
//
void CUI_SongDuration::leave(void) {
}


// ------------------------------------------------------------------------------------------------
//
//
void CUI_SongDuration::update() {
    int key=0,act=0;;
//  act = Keys.size(); 
//  key = Keys.checkkey();
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
        switch(key) {
            case SDLK_ESCAPE:
                act++;
                break;
        }
    }   
    if (act) {
        close_popup_window();
        fixmouse++;
        need_refresh++;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_SongDuration::draw(Drawable *S) {
    int i;
    if (S->lock()==0) {
        S->fillRect(col(this->x),row(this->y),col(this->x+this->xsize)-1,row(this->y + this->ysize)-1,COLORS.Background);
        printline(col(this->x),row(this->y + this->ysize-1),148,this->xsize,COLORS.Lowlight,S);
        for (i=this->y;i<this->y+this->ysize;i++) {
            printchar(col(this->x),row(i),145,COLORS.Highlight,S);
            printchar(col(this->x+this->xsize-1),row(i),146,COLORS.Lowlight,S);
        } 
        printline(col(this->x),row(this->y ),143,this->xsize,COLORS.Highlight,S);
        print(col(this->x),row(this->y + 1),"    Song Duration",COLORS.Text,S);
        char str[50];
        int m = seconds/60;
        int s = seconds%60;
        sprintf(str,"Total: %d:%.2d",m,s);
        print(col(textcenter(str)),row(this->y + 3),str,COLORS.Text,S);
        UI->draw(S);
        S->unlock();
        need_refresh = 0;
        updated++;
    }
}
