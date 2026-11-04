#include "zt.h"



CUI_About::CUI_About(void) {
    UI = new UserInterface();

    TextBox *l = new TextBox();
    tb = l;
    UI->add_element(l,0);
    l->x = 1;
    // Font cells are a fixed 8x8 regardless of INTERNAL_RESOLUTION, so
    // xsize is in characters. The longest line we want to fully contain
    // is "This fork is currently maintained by Manuel Montoto (Debvgger)"
    // (62 chars from column 2). Right edge aligns with end of "(Debvgger)".
    // Narrower screens shrink it to the full width; the text is
    // reflowed for whatever width it gets (see draw()).
    l->xsize = 66;
    // Layout policy (CUI_About::draw will scale the bmAbout bitmap to
    // fit between the page title row and the textbox top, so this is
    // the single source of truth for both):
    //
    //   row 9               page title bar
    //   row 10..box_top-1   bmAbout logo (scaled to fit)
    //   row box_top..end    textbox
    //   row total_rows-9    start of toolbar reserve (7 rows + 2 safety)
    //
    // We give the textbox 13 rows when there's room, and place it so
    // its bottom sits 9 rows above the screen bottom. The logo gets
    // whatever rows remain above it.
    // Bottom: 1 row lower than the prior 9-row toolbar reserve (so 8).
    const int TOOLBAR_RESERVE_ROWS = 8;
    int total_rows  = (INTERNAL_RESOLUTION_Y / 8);
    int max_end_row = total_rows - TOOLBAR_RESERVE_ROWS;
    // Top: 26 rows higher than the (row(9)+300px)/8 anchor (1 row
    // lower than the previous -27 setting).
    int box_top = ((row(9) + 300) / 8) - 27;
    if (box_top < 0) box_top = 0;
    int box_size = ((max_end_row - box_top) * 75) / 100 + 3;
    if (box_size < 6) box_size = 6;
    l->y     = box_top;
    l->ysize = box_size;
    l->bWordWrap = false ;   // pre-wrapped by zt_reflow_text
    l->text = NULL;
    shown_width = -1;
    source_text =R"about_text(
|H|About|U|

zTracker Prime is a fork of the original zTracker project from 2001, which was a clone of Impulse Tracker, itself a clone of Scream Tracker. The original zTracker was developed by Christopher Micali until 2002.

It had become my go-to software for composing music, so I forked it in 2003. Over 20 years later, it remains my primary sequencer, and as of 2026 I continue to work on it.

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
    Esa Juhani Ruoho

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

   zTracker is released under the BSD license.

Refer to the included |H|LICENSE.TXT|U| for details on the licensing terms.

Copyright (c) 2000-2001, Christopher Micali
Copyright (c) 2000-2001, Austin Luminais
Copyright (c) 2001, Nicolas Soudee
Copyright (c) 2001, Daniel Kahlin
Copyright (c) 2003-2026, Manuel Montoto

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
        int key = Keys.getkey();
        if (key == SDLK_ESCAPE) {
            switch_page(UIP_Patterneditor);
        }
    }
    //need_refresh++;
}

void CUI_About::draw(Drawable *S) {
        // Logo at row 9, sitting under the page title bar. The textbox
        // (in CUI_About::CUI_About) is positioned at row 51 — right
        // below where this 319px-tall bitmap ends — so the two never
        // overlap on screens tall enough to fit both.
        S->copy(CurrentSkin->bmAbout,5,row(9));

        tb->xsize = 66;
        if (tb->xsize > TextBox::full_width_xsize()) tb->xsize = TextBox::full_width_xsize();
        if (tb->text_width() != shown_width) {
            shown_width = tb->text_width();
            shown_text = zt_reflow_text(source_text, shown_width);
            tb->text = shown_text.c_str();
            tb->startline = 0;
        }
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