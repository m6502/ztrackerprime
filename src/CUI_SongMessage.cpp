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
    tb->y = 12;
    tb->xsize = TextBox::full_width_xsize();
    // Bottom-anchor: same formula as CUI_Help so the textbox never
    // bleeds into the toolbar strip at INTERNAL_RESOLUTION_Y - 55.
    tb->ysize = (INTERNAL_RESOLUTION_Y/8) - tb->y - 8;
    tb->text = NULL;
    needfree = 0;
    buffer = NULL;
}

CUI_SongMessage::~CUI_SongMessage(void) {
    TextBox *tb;
    if (needfree) {
        tb = (TextBox *)UI->get_element(0);
        delete[] tb->text;
    }
}

void CUI_SongMessage::enter(void) {
    need_refresh++;
    cur_state = STATE_SONG_MESSAGE;
    CommentEditor *cb = (CommentEditor*)UI->get_element(0);
    // Defensive: a corrupt or partially loaded song could leave the
    // songmessage chain incomplete. Bind target to NULL in that case
    // rather than dereferencing through a missing link.
    if (song && song->songmessage && song->songmessage->songmessage) {
        buffer = song->songmessage->songmessage;
    } else {
        buffer = NULL;
    }
    if (cb) {
        cb->target = buffer;
        // Land the caret at the end of the existing message so the
        // user can resume typing immediately (matches the legacy
        // append-only behaviour). Home / Left / arrows can move it
        // back to edit earlier text.
        cb->cursor = buffer ? (unsigned)buffer->getsize() : 0u;
        cb->refresh_display();
    }
    zt_text_input_start();
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
    zt_text_input_stop();
}

void CUI_SongMessage::update() {
    // Peek for ESC before UI->update() so the CommentEditor doesn't
    // get a chance to swallow it.
    if (Keys.size()) {
        int key = Keys.checkkey();
        if (key == SDLK_ESCAPE) {
            Keys.getkey(); // consume
            switch_page(UIP_Patterneditor);
            return;
        }
    }
    UI->update();
    if (Keys.size()) {
        Keys.getkey();
    }
}

void CUI_SongMessage::draw(Drawable *S) {
    // Re-run the bottom-anchored sizing in case the window has been
    // resized since the page was constructed.
    CommentEditor *cb = (CommentEditor*)UI->get_element(0);
    if (!cb) return;
    cb->xsize = TextBox::full_width_xsize();
    cb->ysize = (INTERNAL_RESOLUTION_Y/8) - cb->y - 8;
    // Re-bind target every frame: a song load (Ctrl-L) deletes and
    // re-creates song->songmessage, so any pointer cached in enter()
    // would be left dangling. Re-fetching here keeps cb->target valid
    // through page-resident song reloads.
    CDataBuf *current = (song && song->songmessage)
                        ? song->songmessage->songmessage
                        : NULL;
    if (cb->target != current) {
        cb->target = current;
        buffer = current;
    }
    // Refresh the null-terminated display mirror so TextBox::draw has
    // a clean string to walk (CDataBuf is not null-terminated; reading
    // past the live byte count walks heap garbage).
    cb->refresh_display();
    if (S->lock()==0) {
        UI->draw(S);
        draw_status(S);
        printtitle(PAGE_TITLE_ROW_Y,"Song Message (F10)",COLORS.Text,COLORS.Background,S);
        need_refresh = 0; updated=2;
        ztPlayer->num_orders();
        S->unlock();
    }
}
