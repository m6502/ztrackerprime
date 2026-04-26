#include "zt.h"



CUI_About::CUI_About(void) {
    UI = new UserInterface();

    double xscale = (double)INTERNAL_RESOLUTION_X / 640;

    TextBox *l = new TextBox();
    UI->add_element(l,0);
    l->x = 2;
    // 20% wider again so long sentences wrap further to the right
    // and don't break mid-word. (Was 49*xscale; now 59*xscale.)
    l->xsize = (int)(59.0 * xscale);
    // bmAbout is a 620x319 px logo drawn at y = row(9) (the row right
    // below the page title). The title sits at PAGE_TITLE_ROW_Y = 9,
    // logo immediately below it. Textbox lives in the bottom slice of
    // the page with a clean two-row margin above the toolbar so it
    // never paints into the toolbar pixels.
    //
    // Layout:
    //   row 9     page title
    //   row 9+    bmAbout logo (drawn in CUI_About::draw)
    //   ...       textbox (lower 55% of usable area)
    //   row N-9   start of toolbar reserve (55px = 7 rows + 2 rows safety)
    const int TOOLBAR_RESERVE_ROWS = 9;     // 7 rows toolbar + 2 rows safety
    int total_rows  = (INTERNAL_RESOLUTION_Y / 8);
    int max_end_row = total_rows - TOOLBAR_RESERVE_ROWS;
    int usable = max_end_row - 10;          // rows 10..max_end_row
    if (usable < 8) usable = 8;
    int text_rows = (usable * 55 + 50) / 100;
    if (text_rows < 6)  text_rows = 6;
    if (text_rows > 30) text_rows = 30;
    l->y     = max_end_row - text_rows;
    l->ysize = text_rows;
    l->bWordWrap = true ;
    l->text =R"about_text(
|H|About|U|

zTracker Prime is a fork of the original zTracker project from 2001, which was a clone of Impulse Tracker, itself a clone of Scream Tracker. The original zTracker was developed by Christopher Micali until 2002.

It had become my go-to software for composing music, so I forked it in 2003. Over 20 years later, it remains my primary sequencer, and as of 2024 I continue to work on it.

Check for new releases at the Github project page:

https://github.com/m6502/ztrackerprime

You can find an archived version of the old source code for the original version here:

https://github.com/cmicali/ztracker

|H|Contact|U|

This fork is currently maintained by Manuel Montoto (Debvgger)

Write me to |U|mail@manuelmontoto.com|U|

|H|Credits:|U|

  |H|Coding|U|

    Austin Luminais
    Christopher Micali
    Daniel Kahlin
    Manuel Montoto
    Nic Soudee

  |H|Support|U|

    Amir Geva (libCON / TONS of help)
    Jeffry Lim (Impulse Tracker)
    libpng team
    Sami Tammilehto (Scream Tracker 3)
    SDL team
    zlib team

  |H|Testing and Help|U|

    arguru (NTK source made zt happen)
    Bammer
    czer323
    FSM
    in_tense
    Oguz Akin
    Quasimojo
    Raptorrvl
    Scurvee


|H|License|U|

   ztracker  is released under the BSD license.   Refer to the included |H|LICENSE.TXT|U| for details on the licensing terms.

Copyright (c) 2000-2001,
                    Christopher Micali
Copyright (c) 2000-2001,
                    Austin Luminais
Copyright (c) 2001, Nicolas Soudee
Copyright (c) 2001, Daniel Kahlin
Copyright (c) 2003-2025,
                    Manuel Montoto

All rights reserved.
)about_text";
}

void CUI_About::enter(void) {
    cur_state = STATE_ABOUT;
    need_refresh = 1;
}

void CUI_About::leave(void) {
}

void CUI_About::update() {
    UI->update();
    if (Keys.size()) {
        Keys.getkey();
    }
    //need_refresh++;
}

void CUI_About::draw(Drawable *S) {
        // The bmAbout PNG has ~3 rows of transparent/whitespace padding
        // at the top of the bitmap, so even drawing at row(9) leaves a
        // visible gap below the page title. Anchor at row(7), which is
        // 2 rows above the title row — the bitmap's own top padding
        // absorbs that offset and the visible logo lands flush with
        // the title bar.
        S->copy(CurrentSkin->bmAbout,5,row(7));
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
        printtitle(PAGE_TITLE_ROW_Y,"About zTracker (Alt+F12)",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        UI->draw(S);
        S->unlock();
    }
}