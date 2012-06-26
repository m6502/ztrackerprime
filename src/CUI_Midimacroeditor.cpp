/*************************************************************************
 *
 * FILE  CUI_Midimacroeditor.cpp
 * $Id: CUI_Midimacroeditor.cpp,v 1.1 2001/09/19 20:47:18 tlr Exp $
 *
 * DESCRIPTION 
 *   The midi macro editor.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2001, Daniel Kahlin <tlr@users.sourceforge.net>
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
#define BASE_Y  13

CUI_Midimacroeditor::CUI_Midimacroeditor(void) {

    TextInput *ti;
    static unsigned char testname[256];


    UI = new UserInterface;

    /* Initialize Title TextInput */
        ti = new TextInput;
        UI->add_element(ti,0);
        ti->frame = 1;
        ti->x = 17; 
        ti->y = BASE_Y; 
        ti->xsize=24;
        ti->length=24;
        ti->str = testname;
    // END 
}

CUI_Midimacroeditor::~CUI_Midimacroeditor(void) {
    if (UI) {
        delete UI;
        UI=NULL;
    }
}

void CUI_Midimacroeditor::enter(void) {
    need_refresh++;
    cur_state = STATE_ARPEDIT;
}

void CUI_Midimacroeditor::leave(void) {

}

void CUI_Midimacroeditor::update() {
    int key=0;
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
    }
}

void CUI_Midimacroeditor::draw(Drawable *S) {
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(11,"Midimacro Editor",COLORS.Text,COLORS.Background,S);
        print(col(12),row(BASE_Y),"name",COLORS.Text,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}
/* eof */
