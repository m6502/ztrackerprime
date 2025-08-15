#include "zt.h"



CUI_About::CUI_About(void) {
    UI = new UserInterface();

    double xscale = (double)INTERNAL_RESOLUTION_X / 640;
    double yscale = (double)INTERNAL_RESOLUTION_Y / 480;
    
    TextBox *l = new TextBox();
    UI->add_element(l,0);
    l->x = 2;
    l->xsize = (int)((double)(38+3)*xscale);
    l->ysize = (int)((double)26*yscale);
    l->y = 25 + ((INTERNAL_RESOLUTION_Y-480)/8) - (l->ysize-26);
    l->bWordWrap = true ;
    l->text = "\n"

"|H|About|U|\n"
"\n"
"zTracker Prime is a fork of the original zTracker project from 2001, which was a clone of Impulse Tracker, itself a clone of Scream Tracker. The original zTracker was developed by Christopher Micali until 2002.\n"
"\n"
"It had become my go-to software for composing music, so I forked it in 2003. Over 20 years later, it remains my primary sequencer, and as of 2024 I continue to work on it.\n"
"\n"
"Check for new releases at the Github project page:\n"
"\n"
"https://github.com/m6502/ztrackerprime\n"
"\n"
"You can find an archived version of the old source code for the original version here:\n"
"\n"
"https://github.com/cmicali/ztracker\n"
"\n"
"|H|Contact|U|\n"
"" "\n"
"This fork is currently maintained by Manuel Montoto (Debvgger)" "\n"
"" "\n"
"Write me to |U|mail@manuelmontoto.com|U|" "\n"
"" "\n"
"|H|Credits:|U|\n"
"\n"
"  |H|Coding|U|" "\n"
"" "\n"
"    Austin Luminais" "\n"
"    Christopher Micali" "\n"
"    Daniel Kahlin" "\n"
"    Manuel Montoto" "\n"
"    Nic Soudee" "\n"
"" "\n"
"  |H|Support|U|" "\n"
"" "\n"
"    Amir Geva (libCON / TONS of help)" "\n"
"    Jeffry Lim (Impulse Tracker)" "\n"
"    libpng team" "\n"
"    Sami Tammilehto (Scream Tracker 3)" "\n"
"    SDL team" "\n"
"    zlib team" "\n"
"" "\n"
"  |H|Testing and Help|U|" "\n"
"" "\n"
"    arguru (NTK source made zt happen)" "\n"
"    Bammer" "\n"
"    czer323" "\n"
"    FSM" "\n"
"    in_tense" "\n"
"    Oguz Akin" "\n"
"    Quasimojo" "\n"
"    Raptorrvl" "\n"
"    Scurvee" "\n"
"" "\n"
"" "\n"
"|H|License|U|" "\n"
"" "\n"
"   ztracker  is released under the BSD license.   Refer to the included |H|LICENSE.TXT|U| for details on the licensing terms." "\n"
"" "\n"
"Copyright (c) 2000-2001," "\n"
"                    Christopher Micali " "\n"
"Copyright (c) 2000-2001," "\n"
"                    Austin Luminais" "\n"
"Copyright (c) 2001, Nicolas Soudee" "\n"
"Copyright (c) 2001, Daniel Kahlin" "\n"
"Copyright (c) 2003-2025," "\n"
"                    Manuel Montoto" "\n"
"All rights reserved." "\n"
"" "\n"
"\0"
;
}

CUI_About::~CUI_About(void) {

}

void CUI_About::enter(void) {
    cur_state = STATE_ABOUT;
    need_refresh = 1;
}

void CUI_About::leave(void) {
}

void CUI_About::update() {
    int key=0;
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
    }
    //need_refresh++;
}

void CUI_About::draw(Drawable *S) {
        S->copy(CurrentSkin->bmAbout,5,row(12));
    /*
    if (640 == INTERNAL_RESOLUTION_X && 480 == INTERNAL_RESOLUTION_Y) {
        S->copy(CurrentSkin->bmAbout,5,row(12));
    } else {
        double xscale = (double)INTERNAL_RESOLUTION_X / 640;
        double yscale = (double)INTERNAL_RESOLUTION_Y / 480;
        Drawable ss( zoomSurface(CurrentSkin->bmAbout->surface, xscale, yscale ,SMOOTHING_ON) , true );
        S->copy(&ss,5,row(12));
    }
    */
    UI->full_refresh();
    if (S->lock()==0) {
        printtitle(PAGE_TITLE_ROW_Y,"About zt",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        UI->draw(S);
        S->unlock();
    }
}