#include "zt.h"



CUI_About::CUI_About(void) {
    UI = new UserInterface();

    double xscale = (double)RESOLUTION_X / 640;
    double yscale = (double)RESOLUTION_Y / 480;
    
    TextBox *l = new TextBox();
    UI->add_element(l,0);
    l->x = 2;
    l->xsize = (int)((double)(38+3)*xscale);
    l->ysize = (int)((double)26*yscale);
    l->y = 25 + ((RESOLUTION_Y-480)/8) - (l->ysize-26);
    l->text = "\n"

"|H|About|U|                          \n"
"                                       " "\n"
"   zt was started by Christopher Micali" "\n"
"in 2000.  Chris  wrote most of the code" "\n"
"and  is  still the  main developer, but" "\n"
"many  other  people  have  contributed," "\n"
"helped, and  really  made zt the result" "\n"
"of  a  team effort.  zt would  not have" "\n"
"been possible without the hard work and" "\n"
"great   attitudes   of  the   following" "\n"
"people: (in no order!)                 " "\n"
"                                       " "\n"
"                                       " "\n"
"|H|Credits|U|                          " "\n"
"                                       " "\n"
"  |H|Coding|U|                         " "\n"
"                                       " "\n"
"    Austin Luminais                    " "\n"
"    Christopher Micali                 " "\n"
"    Daniel Kahlin                      " "\n"
"    Nic Soudee                         " "\n"
"                                       " "\n"
"  |H|Support|U|                        " "\n"
"                                       " "\n"
"    Amir Geva (libCON / TONS of help)  " "\n"
"    Jeffry Lim (Impulse Tracker)       " "\n"
"    libpng team                        " "\n"
"    Sami Tammilehto (Scream Tracker 3) " "\n"
"    SDL team                           " "\n"
"    zlib team                          " "\n"
"                                       " "\n"
"  |H|Testing and Help|U|               " "\n"
"                                       " "\n"
"    arguru (NTK source made zt happen) " "\n"
"    FSM                                " "\n"
"    in_tense                           " "\n"
"    czer323                            " "\n"
"    Oguz Akin                          " "\n"
"    Quasimojo                          " "\n"
"    Raptorrvl                          " "\n"
"    Scurvee                            " "\n"
"                                       " "\n"
"                                       " "\n"
"|H|License|U|                          " "\n"
"                                       " "\n"
"   ztracker  is an open source  project" "\n"
"released under the BSD license.   Refer" "\n"
"to the included |H|LICENSE.TXT|U| for details" "\n"
"on the licensing terms.                " "\n"
"                                       " "\n"
"Copyright (c) 2000-2001,               " "\n"
"                    Christopher Micali " "\n"
"Copyright (c) 2000-2001,               " "\n"
"                    Austin Luminais    " "\n"
"Copyright (c) 2001, Nicolas Soudee     " "\n"
"Copyright (c) 2001, Daniel Kahlin      " "\n"
"All rights reserved.                   " "\n"
"                                       " "\n"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
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
    if (640 == RESOLUTION_X && 480 == RESOLUTION_Y) {
        S->copy(CurrentSkin->bmAbout,5,row(12));
    } else {
        double xscale = (double)RESOLUTION_X / 640;
        double yscale = (double)RESOLUTION_Y / 480;
        Drawable ss( zoomSurface(CurrentSkin->bmAbout->surface, xscale, yscale ,SMOOTHING_ON) , true );
        S->copy(&ss,5,row(12));
    }
    */
    UI->full_refresh();
    if (S->lock()==0) {
        printtitle(11,"About zt",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        UI->draw(S);
        S->unlock();
    }
}