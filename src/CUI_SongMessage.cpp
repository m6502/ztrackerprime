/*************************************************************************
 *
 * FILE  CUI_SongMessage.cpp
 * $Id: CUI_SongMessage.cpp,v 1.4 2001/10/16 05:45:06 cmicali Exp $
 *
 * DESCRIPTION 
 *   The song message editor.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2001, Christopher Micali <micali@concentric.net>
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

CUI_SongMessage::CUI_SongMessage(void) {

//  Help *oe;

    UI = new UserInterface;

    CommentEditor *tb;
    tb = new CommentEditor;
    UI->add_element(tb, 0);
    tb->x = 1;
    tb->y = 14;
    tb->xsize = 78 + ((INTERNAL_RESOLUTION_X-640)/8);
    tb->ysize = 36+ ((INTERNAL_RESOLUTION_Y-480)/8);
    tb->text = NULL;//"\n\n  This is the song comment.  Edit me!";
    needfree = 0;
    buffer = NULL;
}

CUI_SongMessage::~CUI_SongMessage(void) {
    TextBox *tb;
    if (needfree) {
        tb = (TextBox *)UI->get_element(0);
        delete[] tb->text;
    }
//    delete buffer;
}

void CUI_SongMessage::enter(void) {
    need_refresh++;
    cur_state = STATE_SONG_MESSAGE;
    CommentEditor *cb = (CommentEditor*)UI->get_element(0);
//    buffer = new CDataBuf;
//    buffer->pushstr(song->songmessage->songmessage);
    buffer = song->songmessage->songmessage;
    cb->target = buffer;
    cb->text = buffer->getbuffer();
    SDL_EnableUNICODE(1); // For trapping characters
}

void CUI_SongMessage::leave(void) {
    CommentEditor *cb = (CommentEditor*)UI->get_element(0);
    cb->text = NULL;
    cb->target = NULL;
/*
    unsigned int newsize = buffer->getsize();
    song->songmessage->resize(newsize+1);
    if (newsize>0) {
        char *buf = buffer->getbuffer();
        for (unsigned int k = 0; k < newsize; k++)
            song->songmessage[k] = buf[k];
//        song->songmessage[newsize] = 0;
    }
            
    //strcpy(song->songmessage->songmessage, buffer->getbuffer());
    delete buffer;
    buffer = NULL;
    */

    SDL_EnableUNICODE(0); // Dont need it anymore
}

void CUI_SongMessage::update() {
    int key=0;
    UI->update();
    if (Keys.size()) {
        key = Keys.getkey();
    }
}

void CUI_SongMessage::draw(Drawable *S) {
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(PAGE_TITLE_ROW_Y,"Song Message",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}
/* eof */
